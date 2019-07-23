/*
meshdesc.cpp - cached mesh for tracing custom objects
Copyright (C) 2012 Uncle Mike

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#include	"extdll.h"
#include  "util.h"
#include	"cbase.h"
#include	"com_model.h"
#include  "meshdesc.h"
#include  "trace.h"

CMeshDesc :: CMeshDesc( void )
{
	m_origin = m_angles = g_vecZero;
	memset( &m_mesh, 0, sizeof( m_mesh ));

	m_debugName = NULL;
	planehash = NULL;
	planepool = NULL;
	facets = NULL;
	m_iNumTris = 0;
}

CMeshDesc :: ~CMeshDesc( void )
{
	FreeMesh ();
}

void CMeshDesc :: InsertLinkBefore( link_t *l, link_t *before )
{
	l->next = before;
	l->prev = before->prev;
	l->prev->next = l;
	l->next->prev = l;
}

void CMeshDesc :: RemoveLink( link_t *l )
{
	l->next->prev = l->prev;
	l->prev->next = l->next;
}

void CMeshDesc :: ClearLink( link_t *l )
{
	l->prev = l->next = l;
}

/*
===============
CreateAreaNode

builds a uniformly subdivided tree for the given mesh size
===============
*/
areanode_t *CMeshDesc :: CreateAreaNode( int depth, const Vector &mins, const Vector &maxs )
{
	areanode_t	*anode;
	Vector		size;
	Vector		mins1, maxs1;
	Vector		mins2, maxs2;

	anode = &areanodes[numareanodes++];

	// use 'solid_edicts' to store facets
	ClearLink( &anode->solid_edicts );
	
	if( depth == AREA_DEPTH )
	{
		anode->axis = -1;
		anode->children[0] = anode->children[1] = NULL;
		return anode;
	}
	
	size = maxs - mins;

	if( size[0] > size[1] )
		anode->axis = 0;
	else anode->axis = 1;
	
	anode->dist = 0.5f * ( maxs[anode->axis] + mins[anode->axis] );
	mins1 = mins;	
	mins2 = mins;	
	maxs1 = maxs;	
	maxs2 = maxs;	
	
	maxs1[anode->axis] = mins2[anode->axis] = anode->dist;
	anode->children[0] = CreateAreaNode( depth+1, mins2, maxs2 );
	anode->children[1] = CreateAreaNode( depth+1, mins1, maxs1 );

	return anode;
}

void CMeshDesc :: FreeMesh( void )
{
	if( m_mesh.numfacets <= 0 )
		return;

	// single memory block
	free( m_mesh.planes );

	FreeMeshBuild();

	memset( &m_mesh, 0, sizeof( m_mesh ));
}

bool CMeshDesc :: InitMeshBuild( const char *debug_name, int numTriangles )
{
	if( numTriangles <= 0 )
		return false;

	// perfomance warning
	if( numTriangles >= 5000 )
		ALERT( at_warning, "%s have too many triangles (%i)\n", debug_name, numTriangles );

	if( numTriangles >= 256 )
		has_tree = true;	// too many triangles invoke to build AABB tree
	else has_tree = false;

	ClearBounds( m_mesh.mins, m_mesh.maxs );

	memset( areanodes, 0, sizeof( areanodes ));
	numareanodes = 0;

	m_debugName = debug_name;
	m_iNumTris = numTriangles;
	m_iTotalPlanes = 0;

	// create pools for construct mesh
	facets = (mfacet_t *)calloc( sizeof( mfacet_t ), numTriangles );
	planehash = (hashplane_t **)calloc( sizeof( hashplane_t* ), PLANE_HASHES );
	planepool = (hashplane_t *)calloc( sizeof( hashplane_t ), MAX_PLANES );

	mplane_t	badplane;

	// create default invalid plane for index 0
	memset( &badplane, 0, sizeof( badplane ));
	AddPlaneToPool( &badplane );

	return true;
}

word CMeshDesc :: AddPlaneToPool( const mplane_t *pl )
{
	hashplane_t *p;
	int hash;

	// trying to find equal plane
	hash = (int)fabs( pl->dist ) / 8;
	hash &= (PLANE_HASHES - 1);

	// search the border bins as well
	for( int i = -1; i <= 1; i++ )
	{
		int h = (hash + i) & (PLANE_HASHES - 1);
		for( p = planehash[h]; p; p = p->hash )
		{
			if( p->pl.normal == pl->normal && p->pl.dist == pl->dist )
				return (word)(p - planepool);	// already exist
		}
	}

	if( m_mesh.numplanes >= MAX_PLANES )
          {
		ALERT( at_error, "AddPlaneToPool: plane limit exceeded: planes %i, maxplanes %i\n", m_mesh.numplanes, MAX_PLANES );
		return 0;	// index of our bad plane
          }

	// create a new one
	p = &planepool[m_mesh.numplanes++];
	p->hash = planehash[hash];
	planehash[hash] = p;

	// record the new plane
	p->pl = *pl;

	return (m_mesh.numplanes - 1);
}

void CMeshDesc :: StudioCalcBoneQuaterion( mstudiobone_t *pbone, mstudioanim_t *panim, Vector4D &q )
{
	mstudioanimvalue_t *panimvalue;
	Vector angle;

	for( int j = 0; j < 3; j++ )
	{
		if( panim->offset[j+3] == 0 )
		{
			angle[j] = pbone->value[j+3]; // default;
		}
		else
		{
			panimvalue = (mstudioanimvalue_t *)((byte *)panim + panim->offset[j+3]);
			angle[j] = panimvalue[1].value;
			angle[j] = pbone->value[j+3] + angle[j] * pbone->scale[j+3];
		}
	}

	AngleQuaternion( angle, q );
}

void CMeshDesc :: StudioCalcBonePosition( mstudiobone_t *pbone, mstudioanim_t *panim, Vector &pos )
{
	mstudioanimvalue_t *panimvalue;

	for( int j = 0; j < 3; j++ )
	{
		pos[j] = pbone->value[j]; // default;

		if( panim->offset[j] != 0 )
		{
			panimvalue = (mstudioanimvalue_t *)((byte *)panim + panim->offset[j]);
			pos[j] += panimvalue[1].value * pbone->scale[j];
		}
	}
}

bool CMeshDesc :: StudioConstructMesh( CBaseEntity *pEnt )
{
	studiohdr_t *phdr = (studiohdr_t *)GET_MODEL_PTR( pEnt->edict() );

	if( !phdr || phdr->numbones < 1 )
	{
		ALERT( at_error, "StudioConstructMesh: bad model header\n" );
		return false;
	}

	float start_time = g_engfuncs.pfnTime();

	// compute default pose for building mesh from
	mstudioseqdesc_t *pseqdesc = (mstudioseqdesc_t *)((byte *)phdr + phdr->seqindex);
	mstudioseqgroup_t *pseqgroup = (mstudioseqgroup_t *)((byte *)phdr + phdr->seqgroupindex) + pseqdesc->seqgroup;

	// sanity check
	if( pseqdesc->seqgroup != 0 )
	{
		ALERT( at_error, "StudioConstructMesh: bad sequence group (must be 0)\n" );
		return false;
	}

	mstudioanim_t *panim = (mstudioanim_t *)((byte *)phdr + pseqgroup->data + pseqdesc->animindex);
	mstudiobone_t *pbone = (mstudiobone_t *)((byte *)phdr + phdr->boneindex);
	static Vector pos[MAXSTUDIOBONES];
	static Vector4D q[MAXSTUDIOBONES];
	int totalVertSize = 0;

	for( int i = 0; i < phdr->numbones; i++, pbone++, panim++ ) 
	{
		StudioCalcBoneQuaterion( pbone, panim, q[i] );
		StudioCalcBonePosition( pbone, panim, pos[i] );
	}

	pbone = (mstudiobone_t *)((byte *)phdr + phdr->boneindex);
	matrix3x4	transform, bonematrix, bonetransform[MAXSTUDIOBONES];
	transform = matrix3x4( pEnt->pev->origin, pEnt->pev->angles );

	// compute bones for default anim
	for( i = 0; i < phdr->numbones; i++ ) 
	{
		// initialize bonematrix
		bonematrix = matrix3x4( pos[i], q[i] );

		if( pbone[i].parent == -1 ) 
			bonetransform[i] = transform.ConcatTransforms( bonematrix );
		else bonetransform[i] = bonetransform[pbone[i].parent].ConcatTransforms( bonematrix );
	}

	// through all bodies to determine max vertices count
	for( i = 0; i < phdr->numbodyparts; i++ )
	{
		mstudiobodyparts_t *pbodypart = (mstudiobodyparts_t *)((byte *)phdr + phdr->bodypartindex) + i;

		int index = pEnt->pev->body / pbodypart->base;
		index = index % pbodypart->nummodels;

		mstudiomodel_t *psubmodel = (mstudiomodel_t *)((byte *)phdr + pbodypart->modelindex) + index;
		totalVertSize += psubmodel->numverts;
	}

	Vector *verts = new Vector[totalVertSize * 8]; // allocate temporary vertices array
	unsigned int *indices = new unsigned int[totalVertSize * 24];
	int numVerts = 0, numElems = 0, numTris = 0;
	Vector tmp, triangle[3];

	for( int k = 0; k < phdr->numbodyparts; k++ )
	{
		mstudiobodyparts_t *pbodypart = (mstudiobodyparts_t *)((byte *)phdr + phdr->bodypartindex) + k;

		int index = pEnt->pev->body / pbodypart->base;
		index = index % pbodypart->nummodels;

		mstudiomodel_t *psubmodel = (mstudiomodel_t *)((byte *)phdr + pbodypart->modelindex) + index;
		Vector *pstudioverts = (Vector *)((byte *)phdr + psubmodel->vertindex);
		Vector *m_verts = new Vector[psubmodel->numverts];
		byte *pvertbone = ((byte *)phdr + psubmodel->vertinfoindex);

		// setup all the vertices
		for( i = 0; i < psubmodel->numverts; i++ )
			m_verts[i] = bonetransform[pvertbone[i]].VectorTransform( pstudioverts[i] );

		mstudiotexture_t *ptexture = (mstudiotexture_t *)((byte *)phdr + phdr->textureindex);
		short *pskinref = (short *)((byte *)phdr + phdr->skinindex);

		for( int j = 0; j < psubmodel->nummesh; j++ ) 
		{
			mstudiomesh_t *pmesh = (mstudiomesh_t *)((byte *)phdr + psubmodel->meshindex) + j;
			short *ptricmds = (short *)((byte *)phdr + pmesh->triindex);

			if( phdr->numtextures != 0 && phdr->textureindex != 0 )
			{
				// skip this mesh it's probably foliage or somewhat
				if( ptexture[pskinref[pmesh->skinref]].flags & STUDIO_NF_TRANSPARENT )
					continue;
			}

			while( i = *( ptricmds++ ))
			{
				int	vertexState = 0;
				bool	tri_strip;

				if( i < 0 )
				{
					tri_strip = false;
					i = -i;
				}
				else
					tri_strip = true;

				numTris += (i - 2);

				for( ; i > 0; i--, ptricmds += 4 )
				{
					// build in indices
					if( vertexState++ < 3 )
					{
						indices[numElems++] = numVerts;
					}
					else if( tri_strip )
					{
						// flip triangles between clockwise and counter clockwise
						if( vertexState & 1 )
						{
							// draw triangle [n-2 n-1 n]
							indices[numElems++] = numVerts - 2;
							indices[numElems++] = numVerts - 1;
							indices[numElems++] = numVerts;
						}
						else
						{
							// draw triangle [n-1 n-2 n]
							indices[numElems++] = numVerts - 1;
							indices[numElems++] = numVerts - 2;
							indices[numElems++] = numVerts;
						}
					}
					else
					{
						// draw triangle fan [0 n-1 n]
						indices[numElems++] = numVerts - ( vertexState - 1 );
						indices[numElems++] = numVerts - 1;
						indices[numElems++] = numVerts;
					}

					verts[numVerts++] = m_verts[ptricmds[0]];
				}
			}
		}

		delete [] m_verts;	// don't keep this because different submodels may have difference count of vertices
	}

	if( numTris != ( numElems / 3 ))
		ALERT( at_error, "StudioConstructMesh: mismatch triangle count (%i should be %i)\n", (numElems / 3), numTris );

	InitMeshBuild( STRING( pEnt->pev->model ), numTris );

	for( i = 0; i < numElems; i += 3 )
	{
		// fill the triangle
		triangle[0] = verts[indices[i+0]];
		triangle[1] = verts[indices[i+1]];
		triangle[2] = verts[indices[i+2]];

		// add it to mesh
		AddMeshTrinagle( triangle );
	}

	delete [] verts;
	delete [] indices;

	if( !FinishMeshBuild( ))
	{
		ALERT( at_error, "StudioConstructMesh: failed to build mesh from %s\n", STRING( pEnt->pev->model ));
		return false;
	}

	// position are changed. Cache new values and rebuild mesh
	m_origin = pEnt->pev->origin;
	m_angles = pEnt->pev->angles;
#if 1
	// g-cont. i'm leave this for debug
	ALERT( at_aiconsole, "%s: build time %g secs, size %i k\n", m_debugName, g_engfuncs.pfnTime() - start_time, ( mesh_size / 1024 ));
#endif
	// done
	return true;
}

bool CMeshDesc :: AddMeshTrinagle( const Vector triangle[3] )
{
	int	i;

	if( m_iNumTris <= 0 )
		return false; // were not in a build mode!

	if( m_mesh.numfacets >= m_iNumTris )
	{
		ALERT( at_error, "AddMeshTriangle: %s overflow (%i >= %i)\n", m_debugName, m_mesh.numfacets, m_iNumTris );
		return false;
	}

	// add triangle to bounds
	for( i = 0; i < 3; i++ )
		AddPointToBounds( triangle[i], m_mesh.mins, m_mesh.maxs );

	mfacet_t *facet = &facets[m_mesh.numfacets];
	mplane_t mainplane;

	// calculate plane for this triangle
	PlaneFromPoints( triangle, &mainplane );

	if( ComparePlanes( &mainplane, g_vecZero, 0.0f ))
		return false; // bad plane

	mplane_t planes[MAX_FACET_PLANES];
	Vector normal;
	int numplanes;
	float dist;

	facet->numplanes = numplanes = 0;

	// add front plane
	SnapPlaneToGrid( &mainplane );

	planes[numplanes].normal = mainplane.normal;
	planes[numplanes].dist = mainplane.dist;
	numplanes++;

	// calculate mins & maxs
	ClearBounds( facet->mins, facet->maxs );

	for( i = 0; i < 3; i++ )
		AddPointToBounds( triangle[i], facet->mins, facet->maxs );

	// add the axial planes
	for( int axis = 0; axis < 3; axis++ )
	{
		for( int dir = -1; dir <= 1; dir += 2 )
		{
			for( i = 0; i < numplanes; i++ )
			{
				if( planes[i].normal[axis] == dir )
					break;
			}

			if( i == numplanes )
			{
				normal = g_vecZero;
				normal[axis] = dir;
				if( dir == 1 )
					dist = facet->maxs[axis];
				else dist = -facet->mins[axis];

				planes[numplanes].normal = normal;
				planes[numplanes].dist = dist;
				numplanes++;
			}
		}
	}

	// add the edge bevels
	for( i = 0; i < 3; i++ )
	{
		int j = (i + 1) % 3;
		int k = (i + 2) % 3;

		Vector vec = triangle[i] - triangle[j];
		if( vec.Length() < 0.5f ) continue;

		vec = vec.Normalize();
		SnapVectorToGrid( vec );

		for( j = 0; j < 3; j++ )
		{
			if( vec[j] == 1.0f || vec[j] == -1.0f )
				break; // axial
		}

		if( j != 3 ) continue; // only test non-axial edges

		// try the six possible slanted axials from this edge
		for( int axis = 0; axis < 3; axis++ )
		{
			for( int dir = -1; dir <= 1; dir += 2 )
			{
				// construct a plane
				Vector vec2 = g_vecZero;
				vec2[axis] = dir;
				normal = CrossProduct( vec, vec2 );

				if( normal.Length() < 0.5f )
					continue;

				normal = normal.Normalize();
				dist = DotProduct( triangle[i], normal );

				for( j = 0; j < numplanes; j++ )
				{
					// if this plane has already been used, skip it
					if( ComparePlanes( &planes[j], normal, dist ))
						break;
				}

				if( j != numplanes ) continue;

				// if all other points are behind this plane, it is a proper edge bevel
				for( j = 0; j < 3; j++ )
				{
					if( j != i )
					{
						float d = DotProduct( triangle[j], normal ) - dist;
						// point in front: this plane isn't part of the outer hull
						if( d > 0.1f ) break;
					}
				}

				if( j != 3 ) continue;

				// add this plane
				planes[numplanes].normal = normal;
				planes[numplanes].dist = dist;
				numplanes++;
			}
		}
	}

	facet->indices = new word[numplanes];
	facet->numplanes = numplanes;

	for( i = 0; i < facet->numplanes; i++ )
	{
		SnapPlaneToGrid( &planes[i] );
		CategorizePlane( &planes[i] );

		// add plane to global pool
		facet->indices[i] = AddPlaneToPool( &planes[i] );
	}

	for( i = 0; i < 3; i++ )
	{
		// spread the mins / maxs by a pixel
		facet->mins[i] -= 1.0f;
		facet->maxs[i] += 1.0f;
	}

	// added
	m_mesh.numfacets++;
	m_iTotalPlanes += numplanes;

	return true;
}

void CMeshDesc :: RelinkFacet( mfacet_t *facet )
{
	// find the first node that the facet box crosses
	areanode_t *node = areanodes;

	while( 1 )
	{
		if( node->axis == -1 ) break;
		if( facet->mins[node->axis] > node->dist )
			node = node->children[0];
		else if( facet->maxs[node->axis] < node->dist )
			node = node->children[1];
		else break; // crosses the node
	}
	
	// link it in	
	InsertLinkBefore( &facet->area, &node->solid_edicts );
}

bool CMeshDesc :: FinishMeshBuild( void )
{
	if( m_mesh.numfacets <= 0 )
	{
		FreeMesh();
		ALERT( at_error, "FinishMeshBuild: failed to build triangle mesh (no sides)\n" );
		return false;
	}

	for( int i = 0; i < 3; i++ )
	{
		// spread the mins / maxs by a pixel
		m_mesh.mins[i] -= 1.0f;
		m_mesh.maxs[i] += 1.0f;
	}

	size_t memsize = (sizeof( mfacet_t ) * m_mesh.numfacets) + (sizeof( mplane_t ) * m_mesh.numplanes) + (sizeof( word ) * m_iTotalPlanes);

	// create non-fragmented memory piece and move mesh
	byte *buffer = (byte *)malloc( memsize );
	byte *bufend = buffer + memsize;

	// setup pointers
	m_mesh.planes = (mplane_t *)buffer; // so we free mem with planes
	buffer += (sizeof( mplane_t ) * m_mesh.numplanes);
	m_mesh.facets = (mfacet_t *)buffer;
	buffer += (sizeof( mfacet_t ) * m_mesh.numfacets);

	// setup mesh pointers
	for( i = 0; i < m_mesh.numfacets; i++ )
	{
		m_mesh.facets[i].indices = (word *)buffer;
		buffer += (sizeof( word ) * facets[i].numplanes);
	}

	if( buffer != bufend )
		ALERT( at_error, "FinishMeshBuild: memory representation error! %x != %x\n", buffer, bufend );

	// copy planes into mesh array (probably aligned block)
	for( i = 0; i < m_mesh.numplanes; i++ )
		m_mesh.planes[i] = planepool[i].pl;

	// copy planes into mesh array (probably aligned block)
	for( i = 0; i < m_mesh.numfacets; i++ )
	{
		m_mesh.facets[i].mins = facets[i].mins;
		m_mesh.facets[i].maxs = facets[i].maxs;
		m_mesh.facets[i].area.next = m_mesh.facets[i].area.prev = NULL;
		m_mesh.facets[i].numplanes = facets[i].numplanes;

		for( int j = 0; j < facets[i].numplanes; j++ )
			m_mesh.facets[i].indices[j] = facets[i].indices[j];
	}

	if( has_tree )
	{
		// create tree
		CreateAreaNode( 0, m_mesh.mins, m_mesh.maxs );

		for( int i = 0; i < m_mesh.numfacets; i++ )
			RelinkFacet( &m_mesh.facets[i] );
	}

	FreeMeshBuild();

	mesh_size = sizeof( m_mesh ) + memsize;
#if 0
	ALERT( at_console, "FinishMesh: %s %i k", m_debugName, ( mesh_size / 1024 ));
	ALERT( at_aiconsole, " (planes reduced from %i to %i)", m_iTotalPlanes, m_mesh.numplanes );
	ALERT( at_console, "\n" );
#endif
	return true;
}

void CMeshDesc :: FreeMeshBuild( void )
{
	// no reason to keep these arrays
	for( int i = 0; facets && i < m_mesh.numfacets; i++ )
		delete [] facets[i].indices;

	free( planehash );
	free( planepool );
	free( facets );

	planehash = NULL;
	planepool = NULL;
	facets = NULL;
}

mmesh_t *CMeshDesc :: CheckMesh( const Vector &origin, const Vector &angles )
{
	if( origin == m_origin && angles == m_angles )
		return &m_mesh;

	// release old copy
	FreeMesh ();

	// position are changed. Cache new values and rebuild mesh
	m_origin = origin;
	m_angles = angles;

	return NULL;
}