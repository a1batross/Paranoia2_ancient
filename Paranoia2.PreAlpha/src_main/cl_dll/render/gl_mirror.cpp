/*
gl_mirror.cpp - draw reflected surfaces
Copyright (C) 2011 Uncle Mike

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#include "hud.h"
#include "cl_util.h"
#include "gl_local.h"
#include "mathlib.h"
#include <stringlib.h>


/*
================
R_BeginDrawMirror

Setup texture matrix for mirror texture
================
*/
void R_BeginDrawMirror( msurface_t *fa, unsigned int unit )
{
	GLfloat		genVector[4][4];
	mextrasurf_t	*es;

	GL_SelectTexture( unit );

	es = SURF_INFO( fa, RI.currentmodel );

	for( int i = 0; i < 4; i++ )
	{
		genVector[0][i] = i == 0 ? 1 : 0;
		genVector[1][i] = i == 1 ? 1 : 0;
		genVector[2][i] = i == 2 ? 1 : 0;
		genVector[3][i] = i == 3 ? 1 : 0;
	}

	GL_TexGen( GL_S, GL_OBJECT_LINEAR );
	GL_TexGen( GL_T, GL_OBJECT_LINEAR );
	GL_TexGen( GL_R, GL_OBJECT_LINEAR );
	GL_TexGen( GL_Q, GL_OBJECT_LINEAR );

	pglTexGenfv( GL_S, GL_OBJECT_PLANE, genVector[0] );
	pglTexGenfv( GL_T, GL_OBJECT_PLANE, genVector[1] );
	pglTexGenfv( GL_R, GL_OBJECT_PLANE, genVector[2] );
	pglTexGenfv( GL_Q, GL_OBJECT_PLANE, genVector[3] );

	GL_LoadTexMatrix( es->mirrormatrix );
}

/*
================
R_EndDrawMirror

Restore identity texmatrix
================
*/
void R_EndDrawMirror( void )
{
	GL_CleanUpTextureUnits( 0 );
	pglMatrixMode( GL_MODELVIEW );
}

/*
=============================================================

	MIRROR RENDERING

=============================================================
*/
/*
================
R_PlaneForMirror

Get transformed mirrorplane and entity matrix
================
*/
void R_PlaneForMirror( msurface_t *surf, mplane_t &out, matrix4x4 &m )
{
	cl_entity_t *ent;

	ent = RI.currententity;

	// setup mirror plane
	out = *surf->plane;

	if( surf->flags & SURF_PLANEBACK )
	{
		out.normal = -out.normal;
		out.dist = -out.dist;
	}

	if(( ent->origin != g_vecZero ) || ( ent->angles != g_vecZero ))
	{
		if( ent->angles != g_vecZero )
			m = matrix4x4( ent->origin, ent->angles );
		else m = matrix4x4( ent->origin, g_vecZero );

		m.TransformPositivePlane( out, out );
	}
	else m.Identity();
}

/*
================
R_AllocateMirrorTexture

Allocate the screen texture and make copy
================
*/
int R_AllocateMirrorTexture( bool copyToImage = true )
{
	int	i, texture;

	// first, search for available mirror texture
	for( i = 0; i < tr.num_mirrors_used; i++ )
	{
		if( tr.mirrorTextures[i].texframe == tr.realframecount )
			continue;	// already used for this frame

		if( RI.viewport[2] != RENDER_GET_PARM( PARM_TEX_WIDTH, tr.mirrorTextures[i].texture ))
			continue;	// width mismatched

		if( RI.viewport[3] != RENDER_GET_PARM( PARM_TEX_HEIGHT, tr.mirrorTextures[i].texture ))
			continue;	// height mismatched

		// found a valid spot
		tr.mirrorTextures[i].texframe = tr.realframecount; // now used
		texture = tr.mirrorTextures[i].texture;
		break;
	}

	if( i == tr.num_mirrors_used )
	{
		if( i == MAX_MIRRORS )
		{
			ALERT( at_error, "R_AllocateMirrorTexture: mirror textures limit exceeded!\n" );
			return 0;	// disable
		}

		tr.num_mirrors_used++; // allocate new one

		char txName[16];

		Q_snprintf( txName, sizeof( txName ), "*mirrorcopy%i", i );

		// create new mirror texture
		tr.mirrorTextures[i].texture = texture = CREATE_TEXTURE( txName, RI.viewport[2], RI.viewport[3], NULL, TF_IMAGE ); 
		tr.mirrorTextures[i].texframe = tr.realframecount; // now used
	}

	if( copyToImage )
	{
		GL_Bind( GL_TEXTURE0, texture );
		pglCopyTexImage2D( GL_TEXTURE_2D, 0, GL_RGB, RI.viewport[0], RI.viewport[1], RI.viewport[2], RI.viewport[3], 0 );
	}

	return texture;
}

/*
================
R_DrawMirrors

Draw all viewpasess from mirror position
Mirror textures will be drawn in normal pass
================
*/
void R_DrawMirrors( cl_entity_t *ignoreent )
{
	ref_instance_t	oldRI;
	unsigned int	oldFBO;
	mplane_t		plane;
	msurface_t	*surf, *surf2;
	mextrasurf_t	*es, *tmp, *mirrorchain;
	Vector		forward, right, up;
	Vector		origin, angles;
	matrix4x4		mirrormatrix;
	mleaf_t		*leaf, *leaf2;
	cl_entity_t	*e;
	model_t		*m;
	float		d;

	if( !tr.num_mirror_entities ) return; // mo mirrors for this frame

	oldFBO = glState.frameBuffer;
	oldRI = RI; // make refinst backup

	leaf = r_viewleaf;
	leaf2 = r_viewleaf2;

	for( int i = 0; i < tr.num_mirror_entities; i++ )
	{
		mirrorchain = tr.mirror_entities[i].chain;

		for( es = mirrorchain; es != NULL; es = es->mirrorchain )
		{
			RI.currententity = e = tr.mirror_entities[i].ent;
			RI.currentmodel = m = RI.currententity->model;

			if( e == ignoreent )
				continue;	// pass skiped

			surf = INFO_SURF( es, m );

			ASSERT( RI.currententity != NULL );
			ASSERT( RI.currentmodel != NULL );

			// NOTE: copy mirrortexture and mirrormatrix from another surfaces
			// from this entity\world that has same planes and reduce number of viewpasses

			// make sure what we have one pass at least
			if( es != mirrorchain )
			{
				for( tmp = mirrorchain; tmp != es; tmp = tmp->mirrorchain )
				{
					surf2 = INFO_SURF( tmp, m );

					if( !tmp->mirrortexturenum )
						continue;	// not filled?

					if( surf->plane->dist != surf2->plane->dist )
						continue;

					if( surf->plane->normal != surf2->plane->normal )
						continue;

					if( FBitSet( surf->flags, SURF_PLANEBACK ) != FBitSet( surf2->flags, SURF_PLANEBACK ))
						continue;

					// found surface with same plane!
					break;
				}

				if( tmp != es && tmp && tmp->mirrortexturenum )
				{
					// just copy reflection texture from surface with same plane
					es->mirrormatrix = tmp->mirrormatrix;
					es->mirrortexturenum = tmp->mirrortexturenum;
					continue;	// pass skiped
				}
			} 

			R_PlaneForMirror( surf, plane, mirrormatrix );

			d = -2.0f * ( DotProduct( RI.vieworg, plane.normal ) - plane.dist );
			origin = RI.vieworg + d * plane.normal;

			d = -2.0f * DotProduct( RI.vforward, plane.normal );
			forward = ( RI.vforward + d * plane.normal ).Normalize();

			d = -2.0f * DotProduct( RI.vright, plane.normal );
			right = ( RI.vright + d * plane.normal ).Normalize();

			d = -2.0f * DotProduct( RI.vup, plane.normal );
			up = ( RI.vup + d * plane.normal ).Normalize();

			matrix3x3	m;

			m.SetForward( forward );
			m.SetRight( right );
			m.SetUp( up );
			m.GetAngles( angles );

			RI.params = RP_MIRRORVIEW|RP_CLIPPLANE|RP_OLDVIEWLEAF;

			RI.clipPlane = plane;
			RI.clipFlags |= ( 1<<5 );

			RI.frustum[5] = plane;
			RI.frustum[5].signbits = SignbitsForPlane( RI.frustum[5].normal );
			RI.frustum[5].type = PLANE_NONAXIAL;

			RI.refdef.viewangles[0] = anglemod( angles[0] );
			RI.refdef.viewangles[1] = anglemod( angles[1] );
			RI.refdef.viewangles[2] = anglemod( angles[2] );
			RI.refdef.vieworg = origin;

			// put pvsorigin before the mirror plane to avoid get full visibility on world mirrors
			if( RI.currententity == GET_ENTITY( 0 ))
			{
				origin = es->origin + (1.0f * plane.normal);
			}
			else
			{
				origin = mirrormatrix.VectorTransform( es->origin ) + (1.0f * plane.normal);
			}

			RI.pvsorigin = origin;

			// combine two leafs from client and mirror views
			r_viewleaf = Mod_PointInLeaf( oldRI.pvsorigin, worldmodel->nodes );
			r_viewleaf2 = Mod_PointInLeaf( RI.pvsorigin, worldmodel->nodes );

			if( FBitSet( surf->flags, ( SURF_REFLECT_PUDDLE|SURF_REFLECT_FRESNEL )))
			{
				// more blur
				RI.viewport[2] = RI.viewport[3] = 256;
				RI.params |= RP_NOSHADOWS;
			}
			else
			{
				if( GL_Support( R_FRAMEBUFFER_OBJECT ))
				{
					RI.viewport[2] = tr.fbo_mirror.GetWidth();
					RI.viewport[3] = tr.fbo_mirror.GetHeight();
				}
				else
				{
					// simple size if FBO was missed
					RI.viewport[2] = RI.viewport[3] = 512;
				}
			}

			if( GL_Support( R_FRAMEBUFFER_OBJECT ))
			{
				es->mirrortexturenum = R_AllocateMirrorTexture( false );
				tr.fbo_mirror.Bind( es->mirrortexturenum );
			}

			tr.mirror_entity = e;

			tr.framecount++;
			r_stats.c_mirror_passes++;
			R_RenderScene( &RI.refdef );

			if( !GL_Support( R_FRAMEBUFFER_OBJECT ))
				es->mirrortexturenum = R_AllocateMirrorTexture();

			// create personal projection matrix for mirror
			if(( e->origin == g_vecZero ) && ( e->angles == g_vecZero ))
			{
				es->mirrormatrix = RI.worldviewProjectionMatrix;
			}
			else
			{
				RI.modelviewMatrix = RI.worldviewMatrix.ConcatTransforms( mirrormatrix );
				es->mirrormatrix = RI.projectionMatrix.Concat( RI.modelviewMatrix );
			}			

			matrix4x4	m1, m2;

			m1.CreateScale( 0.5f );
			m2 = m1.Concat( es->mirrormatrix );
			m1.CreateTranslate( 0.5f, 0.5f, 0.5f );
			es->mirrormatrix = m1.Concat( m2 );

			RI = oldRI; // restore ref instance
			r_viewleaf = leaf;
			r_viewleaf2 = leaf2;
		}

		// clear chain for this entity
		for( es = mirrorchain; es != NULL; )
		{
			tmp = es->mirrorchain;
			es->mirrorchain = NULL;
			es = tmp;
		}

		tr.mirror_entities[i].chain = NULL; // done
		tr.mirror_entities[i].ent = NULL;
	}

	// restore FBO state
	GL_BindFBO( oldFBO );

	r_viewleaf = r_viewleaf2 = NULL;	// force markleafs next frame
	tr.num_mirror_entities = 0;
	tr.mirror_entity = NULL;
}

/*
================
R_RecursiveMirrorNode
================
*/
void R_RecursiveMirrorNode( mnode_t *node, unsigned int clipflags, int &numMirrors )
{
	mextrasurf_t	*extrasurf;
	const mplane_t	*clipplane;
	int		i, clipped;
	msurface_t	*surf, **mark;
	mleaf_t		*pleaf;
	int		c, side;
	float		dot;

	if( node->contents == CONTENTS_SOLID )
		return; // hit a solid leaf

	if( node->visframe != tr.visframecount )
		return;

	if( clipflags )
	{
		for( i = 0, clipplane = RI.frustum; i < 6; i++, clipplane++ )
		{
			if(!( clipflags & ( 1<<i )))
				continue;

			clipped = BoxOnPlaneSide( node->minmaxs, node->minmaxs + 3, clipplane );
			if( clipped == 2 ) return;
			if( clipped == 1 ) clipflags &= ~(1<<i);
		}
	}

	// if a leaf node, draw stuff
	if( node->contents < 0 )
	{
		pleaf = (mleaf_t *)node;

		mark = pleaf->firstmarksurface;
		c = pleaf->nummarksurfaces;

		if( c )
		{
			do
			{
				(*mark)->visframe = tr.framecount;
				mark++;
			} while( --c );
		}
		return;
	}

	// node is just a decision point, so go down the apropriate sides

	// find which side of the node we are on
	dot = PlaneDiff( tr.modelorg, node->plane );
	side = (dot >= 0) ? 0 : 1;

	// recurse down the children, front side first
	R_RecursiveMirrorNode( node->children[side], clipflags, numMirrors );

	// draw stuff
	for( c = node->numsurfaces, surf = worldmodel->surfaces + node->firstsurface; c; c--, surf++ )
	{
		if( !FBitSet( surf->flags, ( SURF_REFLECT|SURF_REFLECT_PUDDLE|SURF_REFLECT_FRESNEL )))
			continue;

		if( FBitSet( surf->flags, SURF_REFLECT_FRESNEL ) && cv_bump->value < 2.0f )
			continue;

		if( R_CullSurface( surf, clipflags ))
			continue;

		extrasurf = SURF_INFO( surf, RI.currentmodel );
		extrasurf->mirrorchain = tr.mirror_entities[0].chain;
		tr.mirror_entities[0].chain = extrasurf;
		numMirrors++;
	}

	// recurse down the back side
	R_RecursiveMirrorNode( node->children[!side], clipflags, numMirrors );
}

/*
=================
R_FindBmodelMirrors

Check all bmodel surfaces and make personal mirror chain
=================
*/
void R_FindBmodelMirrors( cl_entity_t *e, bool static_entity, int &numMirrors )
{
	mextrasurf_t	*extrasurf;
	Vector		mins, maxs;
	msurface_t	*psurf;
	model_t		*clmodel;
	bool		rotated;
	int		i, clipFlags;

	clmodel = e->model;

	// check PVS
	if( !Mod_CheckEntityPVS( e ))
		return;

	if( static_entity )
	{
		RI.objectMatrix.Identity();

		if( R_CullBox( clmodel->mins, clmodel->maxs, RI.clipFlags ))
			return;

		tr.modelorg = RI.vieworg;
		clipFlags = RI.clipFlags;
	}
	else
	{
		if( e->angles != g_vecZero )
		{
			for( i = 0; i < 3; i++ )
			{
				mins[i] = e->origin[i] - clmodel->radius;
				maxs[i] = e->origin[i] + clmodel->radius;
			}
			rotated = true;
		}
		else
		{
			mins = e->origin + clmodel->mins;
			maxs = e->origin + clmodel->maxs;
			rotated = false;
		}

		if( R_CullBox( mins, maxs, RI.clipFlags ))
			return;

		if(( e->origin != g_vecZero ) || ( e->angles != g_vecZero ))
		{
			if( rotated ) RI.objectMatrix = matrix4x4( e->origin, e->angles );
			else RI.objectMatrix = matrix4x4( e->origin, g_vecZero );
		}
		else RI.objectMatrix.Identity();

		e->visframe = tr.framecount; // visible

		if( rotated ) tr.modelorg = RI.objectMatrix.VectorITransform( RI.vieworg );
		else tr.modelorg = RI.vieworg - e->origin;

		clipFlags = 0;
	}

	psurf = &clmodel->surfaces[clmodel->firstmodelsurface];
	for( i = 0; i < clmodel->nummodelsurfaces; i++, psurf++ )
	{
		if( !FBitSet( psurf->flags, ( SURF_REFLECT|SURF_REFLECT_PUDDLE )))
			continue;

		if( R_CullSurface( psurf, clipFlags ))
			continue;

		extrasurf = SURF_INFO( psurf, RI.currentmodel );

		if( static_entity )
		{
			extrasurf->mirrorchain = tr.mirror_entities[0].chain;
			tr.mirror_entities[0].chain = extrasurf;
		}
		else
		{
			extrasurf->mirrorchain = tr.mirror_entities[tr.num_mirror_entities].chain;
			tr.mirror_entities[tr.num_mirror_entities].chain = extrasurf;
		}
		numMirrors++; // count by surfaces not entities
	}

	// store new mirror entity
	if( !static_entity && tr.mirror_entities[tr.num_mirror_entities].chain != NULL )
	{
		tr.mirror_entities[tr.num_mirror_entities].ent = RI.currententity;
		tr.num_mirror_entities++;
	}
}

/*
=================
R_CheckMirrorEntitiesOnList

Check all bmodels for mirror surfaces
=================
*/
void R_CheckMirrorEntitiesOnList( int &numMirrors )
{
	int	i;

	// check static entities
	for( i = 0; i < tr.num_static_entities; i++ )
	{
		RI.currententity = tr.static_entities[i];
		RI.currentmodel = RI.currententity->model;
	
		ASSERT( RI.currententity != NULL );
		ASSERT( RI.currententity->model != NULL );

		switch( RI.currentmodel->type )
		{
		case mod_brush:
			R_FindBmodelMirrors( RI.currententity, true, numMirrors );
			break;
		}
	}

	// world or static entities has mirror surfaces
	if( tr.mirror_entities[0].chain != NULL )
	{
		tr.mirror_entities[0].ent = GET_ENTITY( 0 );
		tr.num_mirror_entities++;
	}

	// check solid entities
	for( i = 0; i < tr.num_solid_entities; i++ )
	{
		RI.currententity = tr.solid_entities[i];
		RI.currentmodel = RI.currententity->model;
	
		ASSERT( RI.currententity != NULL );
		ASSERT( RI.currententity->model != NULL );

		switch( RI.currentmodel->type )
		{
		case mod_brush:
			R_FindBmodelMirrors( RI.currententity, false, numMirrors );
			break;
		}
	}

	// check translucent entities
	for( i = 0; i < tr.num_trans_entities; i++ )
	{
		RI.currententity = tr.trans_entities[i];
		RI.currentmodel = RI.currententity->model;
	
		ASSERT( RI.currententity != NULL );
		ASSERT( RI.currententity->model != NULL );

		switch( RI.currentmodel->type )
		{
		case mod_brush:
			R_FindBmodelMirrors( RI.currententity, false, numMirrors );
			break;
		}
	}

	// check water entities
	for( i = 0; i < tr.num_water_entities; i++ )
	{
		RI.currententity = tr.water_entities[i];
		RI.currentmodel = RI.currententity->model;
	
		ASSERT( RI.currententity != NULL );
		ASSERT( RI.currententity->model != NULL );

		switch( RI.currentmodel->type )
		{
		case mod_brush:
			R_FindBmodelMirrors( RI.currententity, false, numMirrors );
			break;
		}
	}
}

/*
================
R_FindMirrors

Build mirror chains for this frame
================
*/
bool R_FindMirrors( const ref_params_t *fd )
{
	if( !tr.world_has_mirrors || !worldmodel || RI.refdef.paused )
		return false;

	RI.refdef = *fd;

	// build the transformation matrix for the given view angles
	RI.vieworg = RI.refdef.vieworg;
	AngleVectors( RI.refdef.viewangles, RI.vforward, RI.vright, RI.vup );

	R_FindViewLeaf();

	// player is outside world. Don't update portals in speedup reasons
	if(( r_viewleaf - worldmodel->leafs - 1) == -1 )
		return false;

	R_SetupFrustum();
	R_MarkLeaves ();

	tr.modelorg = RI.pvsorigin;
	RI.currententity = GET_ENTITY( 0 );
	RI.currentmodel = RI.currententity->model;

	int numMirrors = 0;

	R_RecursiveMirrorNode( worldmodel->nodes, RI.clipFlags, numMirrors );
	R_CheckMirrorEntitiesOnList( numMirrors );

	if( numMirrors != 0 )
		return true;
	return false;
}