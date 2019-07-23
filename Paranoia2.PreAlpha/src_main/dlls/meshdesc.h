/*
meshdesc.h - cached mesh for tracing custom objects
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

#ifndef MESHDESC_H
#define MESHDESC_H

#include "studio.h"

#define AREA_NODES			32
#define AREA_DEPTH			4

#define MAX_FACET_PLANES		32
#define MAX_PLANES			65536		// unsigned short limit
#define PLANE_HASHES		1024

typedef struct hashplane_s
{
	mplane_t		pl;
	struct hashplane_s	*hash;
} hashplane_t;

typedef struct
{
	link_t	area;				// linked to a division node or leaf
	Vector	mins, maxs;			// an individual size of each facet
	byte	numplanes;			// because numplanes for each facet can't exceeds MAX_FACET_PLANES!
	word	*indices;				// a indexes into mesh plane pool
} mfacet_t;

typedef struct
{
	Vector	mins, maxs;
	word	numfacets;
	word	numplanes;
	mfacet_t	*facets;
	mplane_t	*planes;				// shared plane pool
} mmesh_t;

class CMeshDesc
{
private:
	mmesh_t		m_mesh;
	const char	*m_debugName;		// just for debug purpoces
	Vector		m_origin, m_angles;		// cached values to compare with
	areanode_t	areanodes[AREA_NODES];	// AABB tree for speedup trace test
	int		numareanodes;
	bool		has_tree;			// build AABB tree
	int		m_iTotalPlanes;		// just for stats
	int		m_iNumTris;		// if > 0 we are in build mode
	size_t		mesh_size;		// mesh total size

	// used only while mesh is contsructed
	mfacet_t		*facets;
	hashplane_t	**planehash;
	hashplane_t	*planepool;
public:
	CMeshDesc();
	~CMeshDesc();

	// mesh construction
	bool InitMeshBuild( const char *debug_name, int numTrinagles ); 
	bool AddMeshTrinagle( const Vector triangle[3] );
	bool FinishMeshBuild( void );
	void FreeMeshBuild( void );
	void FreeMesh( void );

	// studio models processing
	void StudioCalcBoneQuaterion( mstudiobone_t *pbone, mstudioanim_t *panim, Vector4D &q );
	void StudioCalcBonePosition( mstudiobone_t *pbone, mstudioanim_t *panim, Vector &pos );
	bool StudioConstructMesh( CBaseEntity *pEnt );

	// linked list operations
	void InsertLinkBefore( link_t *l, link_t *before );
	void RemoveLink( link_t *l );
	void ClearLink( link_t *l );

	// AABB tree contsruction
	areanode_t *CreateAreaNode( int depth, const Vector &mins, const Vector &maxs );
	void RelinkFacet( mfacet_t *facet );
	_inline areanode_t *GetHeadNode( void ) { return (has_tree) ? &areanodes[0] : NULL; }

	// plane cache
	word AddPlaneToPool( const mplane_t *pl );

	// check for cache
	mmesh_t *CheckMesh( const Vector &origin, const Vector &angles );
	_inline mmesh_t *GetMesh() { return &m_mesh; } 
};

#endif//MESHDESC_H