//
// written by BUzer for HL: Paranoia modification
//
//		2006

#include "hud.h"
#include "cl_util.h"
#include "const.h"
#include "cdll_int.h"
#include "com_model.h"
#include "entity_types.h"
#include "r_efx.h"
#include "event_api.h"
#include "pm_defs.h"
#include "pmtrace.h"
#include "parsemsg.h"
#include "gl_local.h"
#include "gl_decals.h"
#include <stringlib.h>
#include "pm_movevars.h"

#define MAX_CLIPVERTS		64	// don't change this
#define MAX_GROUPENTRIES		256
#define MAX_BRUSH_DECALS		4096
#define MAX_CACHE_COUNT		(MAX_BRUSH_DECALS * 16)

// decal entry
typedef struct customdecal_s
{
	msurface_t		*surface;
	Vector			point;
	Vector			normal;
	model_t			*model;
	short			entityIndex;
	word			hProgram;
	const DecalGroupEntry	*texinfo;
	byte			flags;
	Vector			*verts;	// pointer to cache array
	Vector4D			*coord;
	byte			cache_size;
	byte			numpoints;
} customdecal_t;

// used for build new decals
typedef struct
{
	Vector			endpos;
	Vector			pnormal;
	Vector			pright;
	Vector			pup;
	float			radius;
	const DecalGroupEntry	*decalDesc;
	short			entityIndex;
	model_t			*model;
	byte			flags;
	customdecal_t		*current;
} decalinfo_t;

DecalGroup *pDecalGroupList = NULL;

DecalGroup :: DecalGroup( const char *name, int numelems, DecalGroupEntry *source )
{
	Q_strncpy( m_chGroupName, name, sizeof( m_chGroupName ));

	pEntryArray = new DecalGroupEntry[numelems];
	memcpy( pEntryArray, source, sizeof( DecalGroupEntry ) * numelems );
	size = numelems;

	// setup backpointer
	for( int i = 0; i < size; i++ )
		pEntryArray[i].group = this;

	pnext = pDecalGroupList;
	pDecalGroupList = this;
}

DecalGroup :: ~DecalGroup( void )
{
	for( int i = 0; i < size; i++ )
	{
		if( pEntryArray[i].gl_diffuse_id )
			FREE_TEXTURE( pEntryArray[i].gl_diffuse_id );
		if( pEntryArray[i].gl_normalmap_id != tr.normalmapTexture )
			FREE_TEXTURE( pEntryArray[i].gl_normalmap_id );
		if( pEntryArray[i].gl_heightmap_id != tr.whiteTexture )
			FREE_TEXTURE( pEntryArray[i].gl_heightmap_id );
	}

	delete[] pEntryArray;
}

DecalGroupEntry *DecalGroup :: GetEntry( int num )
{
	if( num < 0 || num >= size )
		return NULL;
	return &pEntryArray[num];
}

DecalGroupEntry *DecalGroup :: FindEntry( const char *name )
{
	for( int i = 0; i < size; i++ )
	{
		if( !Q_strcmp( pEntryArray[i].name, name ))
			return &pEntryArray[i];
	}

	return NULL;
}

const DecalGroupEntry *DecalGroup :: GetRandomDecal( void )
{
	return &pEntryArray[RANDOM_LONG( 0, size - 1 )];
}

DecalGroup *DecalGroup :: FindGroup( const char *name )
{
	DecalGroup *plist = pDecalGroupList;

	while( plist )
	{
		if( !Q_strcmp( plist->m_chGroupName, name ))
			return plist;
		plist = plist->pnext;
	}

	return NULL; // nothing found
}

// 1.25 mbytes here if max decals count is 4096
static Vector		g_decalVertsCache[MAX_CACHE_COUNT];
static Vector4D		g_decalCoordCache[MAX_CACHE_COUNT];
static unsigned int		g_decalVertexUsed;	// cache pointer

static customdecal_t	gDecalPool[MAX_BRUSH_DECALS];
static int		gDecalCycle;
static int		gDecalCount;
int			gDecalsRendered;

// ===========================
// Decals creation
// ===========================
static customdecal_t *DecalAlloc( void )
{
	int limit = MAX_BRUSH_DECALS;
	if( r_decals->value < limit )
		limit = r_decals->value;
	
	if( !limit ) return NULL;

	customdecal_t *pdecal = NULL;
	int count = 0;

	// check for the odd possiblity of infinte loop
	do 
	{
		if( gDecalCycle >= limit )
			gDecalCycle = 0;

		pdecal = &gDecalPool[gDecalCycle];
		gDecalCycle++;
		count++;
	} while(( pdecal->flags & FDECAL_PERMANENT ) && count < limit );

	// decal allocated
	if( gDecalCount < limit )
		gDecalCount++;

	return pdecal;	
}

/*
===============
R_ChooseBmodelProgram

Select the program for surface (diffuse\parallax\puddle)
===============
*/
static word R_ChooseDecalProgram( const msurface_t *s, const customdecal_t *decal )
{
	if( decal->flags & FDECAL_PUDDLE )
		return (glsl.bmodelSingleDecalPuddle - tr.glsl_programs);
	if( decal->texinfo->gl_heightmap_id != tr.whiteTexture )
		return (glsl.bmodelSingleDecalPOM - tr.glsl_programs);
	return (glsl.bmodelSingleDecal - tr.glsl_programs);
}

// ===========================
// Math
// ===========================
void FindIntersectionPoint( const Vector &p1, const Vector &p2, const Vector &normal, const Vector &planepoint, Vector &newpoint )
{
	Vector planevec = planepoint - p1;
	Vector linevec = p2 - p1;
	float planedist = DotProduct( normal, planevec );
	float linedist = DotProduct( normal, linevec );

	if( linedist != 0 )
	{
		newpoint = p1 + linevec * ( planedist / linedist );
		return;
	}

	newpoint = g_vecZero;
}

int ClipPolygonByPlane( const Vector arrIn[], int numpoints, const Vector &normal, const Vector &planepoint, Vector arrOut[] )
{
	int cur, prev;
	int first = -1;
	int outCur = 0;
	float dots[MAX_CLIPVERTS];

	if( numpoints > MAX_CLIPVERTS )
	{
		ALERT( at_warning, "ClipPolygonByPlane: too many points\n" );
		numpoints = MAX_CLIPVERTS;
	}

	for( int i = 0; i < numpoints; i++ )
	{
		Vector vecDir;
		vecDir = arrIn[i] - planepoint;
		dots[i] = DotProduct( vecDir, normal );
		if( dots[i] > 0.0f ) first = i;
	}

	if( first == -1 )
		return 0;

	arrOut[outCur] = arrIn[first];
	outCur++;

	cur = first + 1;
	if( cur == numpoints )
		cur = 0;

	while( cur != first )
	{
		if( dots[cur] > 0.0f )
		{
			arrOut[outCur] = arrIn[cur];
			outCur++;
			cur++;

			if( cur == numpoints )
				cur = 0;
		}
		else break;
	}

	if( cur == first )
		return outCur; // ничего не отсекается этой плоскостью

	if( dots[cur] < 0.0f )
	{
		if( cur > 0.0f )
			prev = cur-1;
		else prev = numpoints - 1;

		FindIntersectionPoint( arrIn[prev], arrIn[cur], normal, planepoint, arrOut[outCur] );
	}
	else
	{
		arrOut[outCur] = arrIn[cur];
	}

	outCur++;
	cur++;

	if( cur == numpoints )
		cur = 0;

	// g-cont. potential infinite loop
	while( dots[cur] < 0.0f )
	{
		cur++;
		if( cur == numpoints )
			cur = 0;
	}

	if( cur > 0.0f ) prev = cur - 1;
	else prev = numpoints - 1;

	if( dots[cur] > 0.0f && dots[prev] < 0.0f )
	{
		FindIntersectionPoint( arrIn[prev], arrIn[cur], normal, planepoint, arrOut[outCur] );
		outCur++;
	}

	while( cur != first )
	{
		arrOut[outCur] = arrIn[cur];
		outCur++;
		cur++;

		if( cur == numpoints )
			cur = 0;
	}

	return outCur;
}

/*
================
R_ISortDecalSurfaces

Insertion sort
================
*/
inline static void R_ISortDecalSurfaces( customdecal_t *decals, int num_decals )
{
	customdecal_t	tempbuf;

	for( int i = 1; i < num_decals; i++ )
	{
		tempbuf = decals[i];
		int j = i - 1;

		while(( j >= 0 ) && ( R_SurfCmp( decals[j], tempbuf )))
		{
			decals[j+1] = decals[j];
			j--;
		}
		if( i != ( j + 1 ))
			decals[j+1] = tempbuf;
	}
}

inline static void DecalSurface( msurface_t *surf, decalinfo_t *decalinfo )
{
	Vector norm = surf->plane->normal;
	float ndist = surf->plane->dist;

	if( surf->flags & SURF_PLANEBACK )
	{
		norm *= -1.0f;
		ndist *= -1.0f;
	}

	float normdot = DotProduct( decalinfo->pnormal, norm );
	if( normdot < 0.7f ) return;

	float dist = DotProduct( norm, decalinfo->endpos );
	dist = dist - ndist;

	if( dist < 0.0f ) dist *= -1.0f;
	if( dist > decalinfo->radius )
		return;

	// execute part of drawing process to see if polygon completely clipped
	glpoly_t *p = surf->polys;
	float *v = p->verts[0];
	Vector planepoint;
	int nv;

	int xsize = decalinfo->decalDesc->xsize;
	int ysize = decalinfo->decalDesc->ysize;
	float overlay = decalinfo->decalDesc->overlay;

	Vector array1[MAX_CLIPVERTS];
	Vector array2[MAX_CLIPVERTS];

	for( int j = 0; j < p->numverts; j++, v += VERTEXSIZE )
		array1[j] = Vector( v );

	planepoint = decalinfo->endpos + decalinfo->pright * -xsize;
	nv = ClipPolygonByPlane( array1, p->numverts, decalinfo->pright, planepoint, array2 );

	planepoint = decalinfo->endpos + decalinfo->pright * xsize;
	nv = ClipPolygonByPlane( array2, nv, -decalinfo->pright, planepoint, array1 );

	planepoint = decalinfo->endpos + decalinfo->pup * -ysize;
	nv = ClipPolygonByPlane( array1, nv, decalinfo->pup, planepoint, array2 );

	planepoint = decalinfo->endpos + decalinfo->pup * ysize;
	nv = ClipPolygonByPlane( array2, nv, -decalinfo->pup, planepoint, array1 );

	if( nv < 3 ) return; // no vertexes left after clipping

	float texc_orig_x = DotProduct( decalinfo->endpos, decalinfo->pright );
	float texc_orig_y = DotProduct( decalinfo->endpos, decalinfo->pup );
	customdecal_t *newdecal = NULL;

	if( decalinfo->flags & FDECAL_PERMANENT )
	{
		newdecal = DecalAlloc();

		if( !newdecal )
		{
			ALERT( at_error, "MAX_BRUSH_DECALS limit exceeded!\n" );
			return;
		}
	}
	else
	{
		// look other decals on this surface - is someone too close?
		// if so, clear him
		for( int k = 0; k < gDecalCount; k++ )
		{
			customdecal_t *cur = &gDecalPool[k];

			// not on this surface or permanent decal
			if(( cur->surface != surf ) || ( cur->flags & FDECAL_PERMANENT ))
				continue;

			if(( cur->texinfo->xsize == xsize ) && ( cur->texinfo->ysize == ysize ))
			{
				float texc_x = DotProduct( cur->point, decalinfo->pright ) - texc_orig_x;
				float texc_y = DotProduct( cur->point, decalinfo->pup ) - texc_orig_y;

				if( texc_x < 0.0f ) texc_x *= -1.0f;
				if( texc_y < 0.0f ) texc_y *= -1.0f;

				if( texc_x < (float)xsize * overlay && texc_y < (float)ysize * overlay )
				{
					// replace existed decal
					newdecal = cur;
					break;
				}
			}
		}

		if( !newdecal )
			newdecal = DecalAlloc();
	}

	// puddle required reflections
	if( decalinfo->flags & FDECAL_PUDDLE )
	{
		surf->flags |= SURF_REFLECT_PUDDLE;
		tr.world_has_mirrors = true;
	}

	newdecal->surface = surf;
	newdecal->point = decalinfo->endpos;
	newdecal->normal = decalinfo->pnormal;
	newdecal->model = decalinfo->model;
	newdecal->entityIndex = decalinfo->entityIndex;
	newdecal->texinfo = decalinfo->decalDesc;
	newdecal->flags = decalinfo->flags;
	newdecal->numpoints = nv;
	newdecal->hProgram = R_ChooseDecalProgram( surf, newdecal );

	// NOTE: any decal may potentially fragmented to multiple parts
	// don't save all the fragments because we don't need them to restore this right
	// only once (any) fragment needs to be saved to full restore all the fragments
	if( decalinfo->current )
		newdecal->flags |= FDECAL_DONTSAVE;
	else decalinfo->current = newdecal;

	// init decal cache
	if( newdecal->cache_size < nv )
	{
		// FIXME: cache realloc provoked 'leaks' in cache array
		if( newdecal->cache_size )
			ALERT( at_warning, "DecalSurface: decal cache was reallocated (%i < %i)\n", newdecal->cache_size, nv );
		newdecal->verts = &g_decalVertsCache[g_decalVertexUsed];
		newdecal->coord = &g_decalCoordCache[g_decalVertexUsed];
		g_decalVertexUsed += newdecal->numpoints;
		newdecal->cache_size = nv;
	}

	mtexinfo_t *tex = surf->texinfo;

	// init cache for decal
	for( int k = 0; k < newdecal->numpoints; k++ )
	{
		newdecal->coord[k].x = ((( DotProduct( array1[k], decalinfo->pright ) - texc_orig_x ) / xsize ) + 1.0f ) * 0.5f;
		newdecal->coord[k].y = ((( DotProduct( array1[k], decalinfo->pup ) - texc_orig_y ) / ysize ) + 1.0f ) * 0.5f;
		newdecal->coord[k].z = (( DotProduct( array1[k], tex->vecs[0] ) + tex->vecs[0][3] ) / tex->texture->width );
		newdecal->coord[k].w = (( DotProduct( array1[k], tex->vecs[1] ) + tex->vecs[1][3] ) / tex->texture->height );
		newdecal->verts[k] = array1[k];
	}
}

inline static void DecalNodeSurfaces( model_t *model, mnode_t *node, decalinfo_t *decalinfo )
{
	// iterate over all surfaces in the node
	msurface_t *surf = model->surfaces + node->firstsurface;

	for( int i = 0; i < node->numsurfaces; i++, surf++ ) 
	{
		// never apply decals on the water or sky surfaces
		if( FBitSet( surf->flags, ( SURF_DRAWTURB|SURF_DRAWSKY|SURF_CONVEYOR|SURF_DRAWTILED )))
			continue;

		// no puddles on transparent surfaces or mirrors
		if( FBitSet( decalinfo->flags, FDECAL_PUDDLE ) && FBitSet( surf->flags, ( SURF_TRANSPARENT|SURF_REFLECT )))
			continue;

		DecalSurface( surf, decalinfo );
	}
}

inline static void DecalNode( model_t *model, mnode_t *node, decalinfo_t *decalinfo )
{
	mplane_t	*splitplane;
	float	dist;
	
	ASSERT( node );

	if( node->contents < 0 )
	{
		// hit a leaf
		return;
	}

	splitplane = node->plane;
	dist = DotProduct( decalinfo->endpos, splitplane->normal ) - splitplane->dist;

	if( dist > decalinfo->radius )
	{
		DecalNode( model, node->children[0], decalinfo );
	}
	else if( dist < -decalinfo->radius )
	{
		DecalNode( model, node->children[1], decalinfo );
	}
	else 
	{
		DecalNodeSurfaces( model, node, decalinfo );

		DecalNode( model, node->children[0], decalinfo );
		DecalNode( model, node->children[1], decalinfo );
	}
}

void CreateDecal( const Vector &vecEndPos, const Vector &vecPlaneNormal, const char *name, int flags, int entityIndex, int modelIndex )
{
	if( !pDecalGroupList )
		return;

	if( g_decalVertexUsed >= MAX_CACHE_COUNT )
	{
		ALERT( at_error, "DecalSurface: cache decal is overflow!\n" );
		return;
	}

	decalinfo_t decalInfo;
	cl_entity_t *ent = NULL;

	if( flags & FDECAL_NORANDOM && Q_strchr( name, '@' ))
	{
		// NOTE: restored decal contain name same as 'group@name'
		// so we need separate them before searching the group
		char *sep = Q_strchr( name, '@' );
		if( sep != NULL ) *sep = '\0';
		char *decalname = sep + 1;

		DecalGroup *groupDesc = DecalGroup::FindGroup( name );
		if( !groupDesc )
		{
			ALERT( at_warning, "RestoreDecal: group %s is not exist\n", name );
			return;
                    }

		decalInfo.decalDesc = groupDesc->FindEntry( decalname );
		if( !decalInfo.decalDesc ) return;
	}
	else
	{
		DecalGroup *groupDesc = DecalGroup::FindGroup( name );
		if( !groupDesc )
		{
			ALERT( at_warning, "CreateDecal: group %s is not exist\n", name );
			return;
                    }

		decalInfo.decalDesc = groupDesc->GetRandomDecal();
		if( !decalInfo.decalDesc ) return;

		if( !Q_stricmp( name, "puddle" ))
			flags |= FDECAL_PUDDLE;
	}

	// puddles allowed only at flat surfaces
	if( flags & FDECAL_PUDDLE && vecPlaneNormal != Vector( 0.0f, 0.0f, 1.0f ))
		return;

	decalInfo.model = NULL;

	if( entityIndex > 0 )
	{
		ent = GET_ENTITY( entityIndex );

		if( modelIndex > 0 )
			decalInfo.model = gEngfuncs.pfnGetModelByIndex( modelIndex );
		else if( ent != NULL )
			decalInfo.model = gEngfuncs.pfnGetModelByIndex( ent->curstate.modelindex );
		else return;
	}
	else if( modelIndex > 0 )
		decalInfo.model = gEngfuncs.pfnGetModelByIndex( modelIndex );
	else decalInfo.model = worldmodel;

	if( !decalInfo.model ) return;
	
	if( decalInfo.model->type != mod_brush )
	{
		ALERT( at_error, "Decals must hit mod_brush!\n" );
		return;
	}

	if( ent && !FBitSet( flags, FDECAL_LOCAL_SPACE ))
	{
		// transform decal position in local bmodel space
		if( ent->angles != g_vecZero )
		{
			matrix4x4	matrix( ent->origin, ent->angles );

			// transfrom decal position into local space
			decalInfo.endpos = matrix.VectorITransform( vecEndPos );
			decalInfo.pnormal = matrix.VectorIRotate( vecPlaneNormal );
		}
		else
		{
			decalInfo.endpos = vecEndPos - ent->origin;
			decalInfo.pnormal = vecPlaneNormal;
		}

		flags |= FDECAL_LOCAL_SPACE; // decal position moved into local space
	}
	else
	{
		// pass position in global
		decalInfo.endpos = vecEndPos;
		decalInfo.pnormal = vecPlaneNormal;
	}

	// this decal must use landmark for correct transition
	// a models with origin brush just use local space
	if( !FBitSet( decalInfo.model->flags, MODEL_HAS_ORIGIN ))
	{
		flags |= FDECAL_USE_LANDMARK;
	}

	RI.currententity = GET_ENTITY( entityIndex );
	RI.currentmodel = RI.currententity->model;

	// don't allow random decal select on a next save\restore
	flags |= FDECAL_NORANDOM;

	int xsize = decalInfo.decalDesc->xsize;
	int ysize = decalInfo.decalDesc->ysize;
	VectorMatrix( decalInfo.pnormal, decalInfo.pright, decalInfo.pup );
	decalInfo.radius = sqrt( xsize * xsize + ysize * ysize );
	decalInfo.pright = -decalInfo.pright;
	decalInfo.entityIndex = entityIndex;
	decalInfo.current = NULL; // filled on a first hit of the polygon
	decalInfo.flags = flags;

	// g-cont. now using walking on bsp-tree instead of stupid linear search
	DecalNode( decalInfo.model, &decalInfo.model->nodes[decalInfo.model->hulls[0].firstclipnode], &decalInfo );

	// resort decals after each creation
	R_ISortDecalSurfaces( gDecalPool, gDecalCount );
}

void CreateDecal( pmtrace_t *tr, const char *name )
{
	if( tr->allsolid || tr->fraction == 1.0 )
		return;

	physent_t *pe = gEngfuncs.pEventAPI->EV_GetPhysent( tr->ent );
	int entityIndex = pe->info;
	int modelIndex = 0;

	// modelindex is needs for properly save\restore
	cl_entity_t *ent = GET_ENTITY( entityIndex );
	if( ent ) modelIndex = ent->curstate.modelindex;

	CreateDecal( tr->endpos, tr->plane.normal, name, 0, entityIndex, modelIndex );
}

// debugging feature
void PasteViewDecal( void )
{
	if( CMD_ARGC() <= 1 )
	{
		Msg( "usage: pastedecal <decal name>\n" );
		return;
	}

	pmtrace_t tr;
	gEngfuncs.pEventAPI->EV_SetTraceHull( 2 );
	gEngfuncs.pEventAPI->EV_PlayerTrace( RI.vieworg, (RI.vieworg + (RI.vforward * 1024)), PM_STUDIO_IGNORE, -1, &tr );
	CreateDecal( &tr, CMD_ARGV( 1 ));
}

int SaveDecalList( decallist_t *pBaseList, int count, qboolean changelevel )
{
	int maxBrushDecals = MAX_BRUSH_DECALS + (MAX_BRUSH_DECALS - count);
	decallist_t *pList = pBaseList + count;	// shift list to first free slot
	customdecal_t *pdecal;
	int total = 0;

	for( int i = 0; i < gDecalCount; i++ )
	{
		pdecal = &gDecalPool[i];
		const DecalGroupEntry *tex = pdecal->texinfo;
		if( !tex || !tex->group ) continue; // ???

		if(!( pdecal->flags & FDECAL_DONTSAVE ))	
		{
			pList[total].depth = 1;
			pList[total].flags = pdecal->flags;
			pList[total].entityIndex = pdecal->entityIndex;
			pList[total].position = pdecal->point;
			pList[total].impactPlaneNormal = pdecal->normal;
			pList[total].scale = 1.0f; // just to prevent write garbage in save file
			Q_snprintf( pList[total].name, sizeof( pList[total].name ), "%s@%s", tex->group->GetName(), tex->name );
			total++;
		}

		// check for list overflow
		if( total >= maxBrushDecals )
		{
			ALERT( at_error, "SaveDecalList: too many brush decals on save\restore\n" );
			goto end_serialize;
		}
	}
end_serialize:

	return total;
}

// ===========================
// Decals drawing
//
// Uses surface visframe information from custom renderer's pass
// ===========================
int DrawSingleDecal( customdecal_t *decal )
{
	// check for valid
	if( !decal->surface || !decal->texinfo )
		return FALSE; // bad decal?

	if( decal->model != RI.currentmodel )
		return FALSE; // don't draw bmodel decal from world call

	if( decal->surface->visframe != tr.framecount )
		return FALSE;

	GL_Bind( GL_TEXTURE0, decal->texinfo->gl_diffuse_id );

	// draw decal from cache
	pglBegin( GL_POLYGON );
	for( int k = 0; k < decal->numpoints; k++ )
	{
		pglTexCoord2f( decal->coord[k].x, decal->coord[k].y );
		pglVertex3fv( decal->verts[k] );
	}
	pglEnd();

	return TRUE;
}

void DrawDecals( void )
{
	if( !gDecalCount ) return;

	pglEnable( GL_BLEND );
	pglBlendFunc( GL_DST_COLOR, GL_SRC_COLOR );
	pglDepthMask( GL_FALSE );
	pglDepthFunc( GL_LEQUAL );

	pglPolygonOffset( -1, -1 );
	pglEnable( GL_POLYGON_OFFSET_FILL );

	// disable texturing on all units except first
	GL_SetTexEnvs( ENVSTATE_REPLACE );

	for( int i = 0; i < gDecalCount; i++ )
	{
		if( DrawSingleDecal( &gDecalPool[i] ))
			gDecalsRendered++;
	}

	pglDisable( GL_POLYGON_OFFSET_FILL );
	pglDisable( GL_BLEND );
	pglDepthMask( !glState.drawTrans );
	pglDepthFunc( GL_LEQUAL );
}

bool R_DrawDecalOnList( customdecal_t *decal, bool opaque, word &hLastShader )
{
	// check for valid
	if( !decal->surface || !decal->texinfo )
		return false; // bad decal?

	cl_entity_t *e = GET_ENTITY( decal->entityIndex );

	if( opaque && ( e->curstate.rendermode != kRenderNormal && e->curstate.rendermode != kRenderTransAlpha ))
		return false; // wrong pass (this is need for correct draw decals before refraction glass)

	if( !opaque && ( e->curstate.rendermode == kRenderNormal || e->curstate.rendermode == kRenderTransAlpha ))
		return false; // wrong pass (this is need for correct draw decals before refraction glass)

	if( decal->surface->visframe != tr.framecount )
		return false; // culled out

	// keep screencopy an actual
	if( FBitSet( decal->flags, FDECAL_PUDDLE ) && tr.scrcpydecalframe != tr.framecount )
	{
		GL_Bind( GL_TEXTURE0, tr.refractionTexture );
		pglCopyTexSubImage2D( GL_TEXTURE_RECTANGLE_NV, 0, 0, 0, 0, 0, glState.width, glState.height );
		tr.scrcpydecalframe = tr.framecount;
	}

	if( hLastShader != decal->hProgram )
	{
		GL_BindShader( &tr.glsl_programs[decal->hProgram] );
		pglUniform1iARB( RI.currentshader->u_ClipPlane, ( RI.params & RP_CLIPPLANE ));
		pglUniform1fARB( RI.currentshader->u_RealTime, RI.refdef.time );
		hLastShader = decal->hProgram;
	}

	material_t *mat = R_TextureAnimation( decal->surface->texinfo->texture, ( decal->surface - worldmodel->surfaces ))->material;
	mextrasurf_t *es = SURF_INFO( decal->surface, e->model );
	movevars_t *mv = RI.refdef.movevars;
	msurfmesh_t *mesh = es->mesh;
	Vector lightdir, dir = -Vector( mv->skyvec_x, mv->skyvec_y, mv->skyvec_z );

	// transform to tangent space
	lightdir.x = DotProduct( dir, mesh->verts[0].tangent );
	lightdir.y = DotProduct( dir, -mesh->verts[0].binormal ); // intentionally inverted. Don't touch
	lightdir.z = DotProduct( dir, mesh->verts[0].normal );

	pglUniform3fARB( RI.currentshader->u_LightDir, lightdir.x, lightdir.y, lightdir.z );

	// because puddles not required transparent
	if( decal->flags & FDECAL_PUDDLE )
	{
		pglUniform1iARB( RI.currentshader->u_FaceFlags, mat->flags|BRUSH_REFLECT );
		pglDisable( GL_BLEND );
	}
	else
	{
		pglUniform1iARB( RI.currentshader->u_FaceFlags, mat->flags );
		pglEnable( GL_BLEND );
	}
	int surfaceMask = tr.whiteTexture; // just for have something valid here

	if( FBitSet( mat->flags, BRUSH_TRANSPARENT ))
		surfaceMask = mat->gl_diffuse_id;

	GL_Bind( GL_TEXTURE0, decal->texinfo->gl_diffuse_id );
	GL_Bind( GL_TEXTURE1, surfaceMask );

	if( FBitSet( decal->flags, FDECAL_PUDDLE ) && r_allow_mirrors->value && !FBitSet( RI.params, RP_MIRRORVIEW ))
	{
		pglUniform1iARB( RI.currentshader->u_ParallaxMode, 0 );
		GL_Bind( GL_TEXTURE2, es->mirrortexturenum );
		GL_LoadTexMatrix( es->mirrormatrix );
	}
	else if(( decal->texinfo->gl_heightmap_id != tr.whiteTexture ) && cv_parallax->value )
	{
		// FIXME: get scale and steps from decalinfo.txt?
		pglUniform1iARB( RI.currentshader->u_ParallaxMode, bound( 0, (int)cv_parallax->value, 2 ));
		float scale_x = 15.0f / (float)RENDER_GET_PARM( PARM_TEX_WIDTH, decal->texinfo->gl_diffuse_id );
		float scale_y = 15.0f / (float)RENDER_GET_PARM( PARM_TEX_HEIGHT, decal->texinfo->gl_diffuse_id );
		pglUniform2fARB( RI.currentshader->u_ParallaxScale, scale_x, scale_y );
		pglUniform1iARB( RI.currentshader->u_ParallaxSteps, 30.0f );
		GL_Bind( GL_TEXTURE2, decal->texinfo->gl_heightmap_id );
		GL_LoadIdentityTexMatrix();
	}
	else
	{
		pglUniform1iARB( RI.currentshader->u_ParallaxMode, 0 );
		GL_Bind( GL_TEXTURE2, tr.blackTexture );
		GL_LoadIdentityTexMatrix();
	}

	GL_Bind( GL_TEXTURE3, decal->texinfo->gl_normalmap_id );
	GL_Bind( GL_TEXTURE4, tr.refractionTexture );

	// update transformation matrix
	gl_cachedmatrix_t *view = &tr.cached_matrices[e->hCachedMatrix];
	pglUniformMatrix4fvARB( RI.currentshader->u_ModelViewMatrix, 1, GL_FALSE, &view->modelviewMatrix[0] );
	pglUniformMatrix4fvARB( RI.currentshader->u_ModelViewProjectionMatrix, 1, GL_FALSE, &view->modelviewProjectionMatrix[0] );

	// draw decal from cache
	pglBegin( GL_POLYGON );

	for( int k = 0; k < decal->numpoints; k++ )
	{
		// parallax required viewdirection
		if( FBitSet( decal->flags, FDECAL_PUDDLE ) || ( decal->texinfo->gl_heightmap_id != tr.whiteTexture ))
		{
			Vector dir, eye = view->modelorg - decal->verts[k];
			dir.x = DotProduct( eye, mesh->verts[0].tangent );
			dir.y = DotProduct( eye, -mesh->verts[0].binormal ); // intentionally inverted. Don't touch
			dir.z = DotProduct( eye, mesh->verts[0].normal );
			pglNormal3fv( dir ); // store it into gl_Normal
		}

		pglMultiTexCoord2f( GL_TEXTURE0_ARB, decal->coord[k].x, decal->coord[k].y );
		pglMultiTexCoord2f( GL_TEXTURE1_ARB, decal->coord[k].z, decal->coord[k].w );
		pglVertex3fv( decal->verts[k] );
	}

	pglEnd();

	// add some stat
	if( opaque ) r_stats.c_solid_decals++;
	else r_stats.c_trans_decals++;

	return true;
}

void R_DrawDecalList( bool opaque )
{
	word hLastShader = -1;

	if( !gDecalCount ) return;

	pglEnable( GL_BLEND );
	pglBlendFunc( GL_DST_COLOR, GL_SRC_COLOR );
	pglDepthMask( GL_FALSE );
	pglDepthFunc( GL_LEQUAL );

	pglPolygonOffset( -1, -1 );
	pglEnable( GL_POLYGON_OFFSET_FILL );

	// disable texturing on all units except first
	GL_SetTexEnvs( ENVSTATE_REPLACE );

	for( int i = 0; i < gDecalCount; i++ )
	{
		if( R_DrawDecalOnList( &gDecalPool[i], opaque, hLastShader ))
			gDecalsRendered++;
	}

	pglDisable( GL_BLEND );
	GL_CleanUpTextureUnits( 0 );
	pglDisable( GL_POLYGON_OFFSET_FILL );
	pglDepthMask( opaque ? GL_TRUE : GL_FALSE );
	pglDepthFunc( GL_LEQUAL );
}

// NOTE: this message used twice - for initial creation and on the save\restore
int MsgCustomDecal( const char *pszName, int iSize, void *pbuf )
{
	char name[80];

	BEGIN_READ( pbuf, iSize );

	Vector pos, normal;
	pos.x = READ_COORD();
	pos.y = READ_COORD();
	pos.z = READ_COORD();
	normal.x = READ_COORD() / 8192.0f;
	normal.y = READ_COORD() / 8192.0f;
	normal.z = READ_COORD() / 8192.0f;
	int entityIndex = READ_SHORT();
	int modelIndex = READ_SHORT();
	Q_strncpy( name, READ_STRING(), sizeof( name ));
	int flags = READ_BYTE();

	CreateDecal( pos, normal, name, flags, entityIndex, modelIndex );

	return 1;
}

void ClearDecals( void )
{
	memset( gDecalPool, 0, sizeof( gDecalPool ));
	g_decalVertexUsed = 0;
	gDecalCount = 0;
	gDecalCycle = 0;
}

// ===========================
// Decals loading
// ===========================
void DecalsInit( void )
{
	ADD_COMMAND( "pastedecal", PasteViewDecal );
	ADD_COMMAND( "decalsclear", ClearDecals );
	gEngfuncs.pfnHookUserMsg( "CustomDecal", MsgCustomDecal );

	ALERT( at_aiconsole, "Loading decals\n" );

	char *pfile = (char *)gEngfuncs.COM_LoadFile( "gfx/decals/decalinfo.txt", 5, NULL );

	if( !pfile )
	{
		ALERT( at_error, "Cannot open file \"gfx/decals/decalinfo.txt\"\n" );
		return;
	}

	char *ptext = pfile;
	int counter = 0;

	while( 1 )
	{
		// store position where group names recorded
		char *groupnames = ptext;

		// loop until we'll find decal names
		char path[256], token[256];
		int numgroups = 0;

		while( 1 )
		{
			ptext = COM_ParseFile( ptext, token );
			if( !ptext ) goto getout;

			if( token[0] == '{' )
				break;
			numgroups++;
		}

		DecalGroupEntry	tempentries[MAX_GROUPENTRIES];
		int		numtemp = 0;

		while( 1 )
		{
			char sz_xsize[64];
			char sz_ysize[64];
			char sz_overlay[64];

			ptext = COM_ParseFile( ptext, token );
			if( !ptext ) goto getout;
			if( token[0] == '}' )
				break;

			if( numtemp >= MAX_GROUPENTRIES )
			{
				ALERT( at_error, "Too many decals in group (%d max) - skipping %s\n", MAX_GROUPENTRIES, token );
				continue;
			}

			ptext = COM_ParseFile( ptext, sz_xsize );
			if( !ptext ) goto getout;
			ptext = COM_ParseFile( ptext, sz_ysize );
			if( !ptext ) goto getout;
			ptext = COM_ParseFile( ptext, sz_overlay );
			if( !ptext ) goto getout;

			if( Q_strlen( token ) > 16 )
			{
				ALERT( at_error, "%s - got too large token when parsing decal info file\n", token );
				continue;
			}

			Q_snprintf( path, sizeof( path ), "gfx/decals/%s.tga", token );
			int id = LOAD_TEXTURE( path, NULL, 0, TF_CLAMP );
			if( !id ) continue;

			tempentries[numtemp].gl_diffuse_id = id;

			Q_snprintf( path, sizeof( path ), "gfx/decals/%s_norm.tga", token );
			if( FILE_EXISTS( path )) tempentries[numtemp].gl_normalmap_id = LOAD_TEXTURE( path, NULL, 0, TF_NORMALMAP );
			else tempentries[numtemp].gl_normalmap_id = tr.normalmapTexture;

			Q_snprintf( path, sizeof( path ), "gfx/decals/%s_height.tga", token );
			if( FILE_EXISTS( path )) tempentries[numtemp].gl_heightmap_id = LOAD_TEXTURE( path, NULL, 0, TF_CLAMP );
			else tempentries[numtemp].gl_heightmap_id = tr.whiteTexture;

			tempentries[numtemp].xsize = Q_atof( sz_xsize ) / 2.0f;
			tempentries[numtemp].ysize = Q_atof( sz_ysize ) / 2.0f;
			tempentries[numtemp].overlay = Q_atof( sz_overlay ) * 2.0f;
			Q_strncpy( tempentries[numtemp].name, token, sizeof( tempentries[numtemp].name ));
			numtemp++;
		}

		// get back to group names
		for( int i = 0; i < numgroups; i++ )
		{
			groupnames = COM_ParseFile( groupnames, token );
			if( !numtemp )
			{
				ALERT( at_warning, "got empty decal group: %s\n", token );
				continue;
			}

			new DecalGroup( token, numtemp, tempentries );
			counter++;
		}
	}

getout:
	gEngfuncs.COM_FreeFile( pfile );
	ALERT( at_console, "%d decal groups created\n", counter );
}

void DecalsShutdown( void )
{
	if( !pDecalGroupList )
		return;

	DecalGroup **prev = &pDecalGroupList;
	DecalGroup *item;

	while( 1 )
	{
		item = *prev;
		if( !item ) break;

		*prev = item->GetNext();
		delete item;
	}
}

void DecalsPrintDebugInfo( void )
{
	if( cv_decalsdebug->value )
	{
		char msg[256];
		Q_snprintf( msg, sizeof( msg ), "%d decals in memory, %d rendered\n", gDecalCount, gDecalsRendered );
		DrawConsoleString( XRES(10), YRES(100), msg );
	}
}