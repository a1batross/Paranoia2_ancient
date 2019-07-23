/*
gl_shadows.cpp - render shadowmaps for directional lights
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

#include "hud.h"
#include "cl_util.h"
#include "const.h"
#include "com_model.h"
#include <pm_movevars.h>
#include "camera.h"
#include "pmtrace.h"
#include "r_studioint.h"
#include "ref_params.h"
#include "gl_local.h"
#include <mathlib.h>
#include <stringlib.h>
#include "gl_studio.h"
#include "gl_sprite.h"
#include "event_api.h"
#include "pm_defs.h"

/*
=============================================================

	SHADOW RENDERING

=============================================================
*/
int R_AllocateShadowTexture( bool copyToImage = true )
{
	int i = tr.num_shadows_used;

	if( i >= MAX_SHADOWS )
	{
		ALERT( at_error, "R_AllocateShadowTexture: shadow textures limit exceeded!\n" );
		return 0; // disable
	}

	int texture = tr.shadowTextures[i];
	tr.num_shadows_used++;

	if( !tr.shadowTextures[i] )
	{
		char txName[16];

		Q_snprintf( txName, sizeof( txName ), "*shadow%i", i );

		tr.shadowTextures[i] = CREATE_TEXTURE( txName, RI.viewport[2], RI.viewport[3], NULL, TF_SHADOW ); 
		texture = tr.shadowTextures[i];
	}

	if( copyToImage )
	{
		GL_Bind( GL_TEXTURE0, texture );
		pglCopyTexImage2D( GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, RI.viewport[0], RI.viewport[1], RI.viewport[2], RI.viewport[3], 0 );
	}

	return texture;
}

/*
===============
R_ShadowPassSetupFrame
===============
*/
static void R_ShadowPassSetupFrame( const DynamicLight *pl )
{
	if( !FBitSet( RI.params, RP_OLDVIEWLEAF ))
	{
		r_oldviewleaf = r_viewleaf;
		r_oldviewleaf2 = r_viewleaf2;
		r_viewleaf = Mod_PointInLeaf( pl->origin, worldmodel->nodes );		// light pvs
		r_viewleaf2 = Mod_PointInLeaf( RI.refdef.vieworg, worldmodel->nodes );	// client pvs
	}

	// build the transformation matrix for the given view angles
	RI.refdef.viewangles[0] = anglemod( pl->angles[0] );
	RI.refdef.viewangles[1] = anglemod( pl->angles[1] );
	RI.refdef.viewangles[2] = anglemod( pl->angles[2] );	

	if( pl->key == SUNLIGHT_KEY )
	{
		movevars_t *mv = RI.refdef.movevars;
		RI.vforward = Vector( mv->skyvec_x, mv->skyvec_y, mv->skyvec_z );
		VectorMatrix( RI.vforward, RI.vright, RI.vup );
	}
	else
	{
		AngleVectors( RI.refdef.viewangles, RI.vforward, RI.vright, RI.vup );
	}
	RI.vieworg = RI.refdef.vieworg = pl->origin;

	GL_ResetMatrixCache();

	tr.dlightframecount = tr.framecount++;

	// setup the screen FOV
	RI.refdef.fov_x = pl->fov;
	RI.refdef.fov_y = pl->fov;

	// setup frustum
	memcpy( RI.frustum, pl->frustum, sizeof( RI.frustum ));
	RI.clipFlags = pl->clipflags;

	RI.currentlight = pl;
}

/*
=============
R_ShadowPassSetupGL
=============
*/
static void R_ShadowPassSetupGL( const DynamicLight *pl )
{
	// matrices already computed
	RI.worldviewMatrix = pl->modelviewMatrix;
	RI.projectionMatrix = pl->projectionMatrix;
	RI.worldviewProjectionMatrix = RI.projectionMatrix.Concat( RI.worldviewMatrix );

	RI.worldviewMatrix.CopyToArray( RI.gl_modelviewMatrix );

	// tell engine about worldviewprojection matrix so TriWorldToScreen and TriScreenToWorld
	// will be working properly
	RI.worldviewProjectionMatrix.CopyToArray( RI.gl_modelviewProjectionMatrix );
	SET_ENGINE_WORLDVIEW_MATRIX( RI.gl_modelviewProjectionMatrix );

	pglViewport( RI.viewport[0], RI.viewport[1], RI.viewport[2], RI.viewport[3] );

	pglMatrixMode( GL_PROJECTION );
	GL_LoadMatrix( RI.projectionMatrix );

	pglMatrixMode( GL_MODELVIEW );
	GL_LoadMatrix( RI.worldviewMatrix );
	GL_Cull( GL_FRONT );

	pglColor4f( 1.0f, 1.0f, 1.0f, 1.0f );
	pglColorMask( GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE );
	pglDepthRange( 0.0001f, 1.0f ); // ignore paranoia opengl32.dll
	pglEnable( GL_POLYGON_OFFSET_FILL );
	pglDisable( GL_TEXTURE_2D );
	pglDepthMask( GL_TRUE );
	if( pl->key == SUNLIGHT_KEY )
		pglPolygonOffset( 2, 2 );
	else pglPolygonOffset( 8, 30 );
	pglEnable( GL_DEPTH_TEST );
	pglDisable( GL_ALPHA_TEST );
	pglDisable( GL_BLEND );
}

/*
=============
R_ShadowPassEndGL
=============
*/
static void R_ShadowPassEndGL( void )
{
	pglColorMask( GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE );
	pglDisable( GL_POLYGON_OFFSET_FILL );
	pglEnable( GL_TEXTURE_2D );
	pglPolygonOffset( -1, -2 );
	pglDepthRange( gldepthmin, gldepthmax );
}

/*
================
R_RecursiveShadowNode
================
*/
void R_RecursiveShadowNode( mnode_t *node, const mplane_t frustum[6], unsigned int clipflags )
{
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
		for( i = 0; i < 6; i++ )
		{
			clipplane = &frustum[i];

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
	R_RecursiveShadowNode( node->children[side], frustum, clipflags );

	// draw stuff
	for( c = node->numsurfaces, surf = worldmodel->surfaces + node->firstsurface; c; c--, surf++ )
	{
		if( R_CullSurfaceExt( surf, frustum, clipflags ))
			continue;

		if( r_newrenderer->value )
		{
			R_AddSurfaceToDrawList( surf );
		}
		else if( !FBitSet( surf->flags, ( SURF_DRAWTILED|SURF_REFLECT )))
		{
			// keep light surfaces seperate from world chains
			SURF_INFO( surf, RI.currentmodel )->lightchain = surf->texinfo->texture->lightchain;
			surf->texinfo->texture->lightchain = SURF_INFO( surf, RI.currentmodel );
		}
	}

	// recurse down the back side
	R_RecursiveShadowNode( node->children[!side], frustum, clipflags );
}

/*
=================
R_ShadowStaticModel

Merge static model brushes with world lighted surfaces
=================
*/
void R_ShadowStaticModel( const DynamicLight *pl, cl_entity_t *e )
{
	model_t		*clmodel;
	msurface_t	*surf;
	
	clmodel = e->model;

	if( R_CullBoxExt( pl->frustum, clmodel->mins, clmodel->maxs, pl->clipflags ))
		return;

	e->visframe = tr.framecount; // visible

	surf = &clmodel->surfaces[clmodel->firstmodelsurface];
	for( int i = 0; i < clmodel->nummodelsurfaces; i++, surf++ )
	{
		if( R_CullSurfaceExt( surf, pl->frustum, pl->clipflags ))
			continue;

		if( !FBitSet( surf->flags, ( SURF_DRAWTILED|SURF_REFLECT )))
		{
			// keep light surfaces seperate from world chains
			SURF_INFO( surf, RI.currentmodel )->lightchain = surf->texinfo->texture->lightchain;
			surf->texinfo->texture->lightchain = SURF_INFO( surf, RI.currentmodel );
		}
	}
}

/*
=================
R_ShadowStaticBrushes

Insert static brushes into world texture chains
=================
*/
void R_ShadowStaticBrushes( const DynamicLight *pl )
{
	// draw static entities
	for( int i = 0; i < tr.num_static_entities; i++ )
	{
		RI.currententity = tr.static_entities[i];
		RI.currentmodel = RI.currententity->model;
	
		ASSERT( RI.currententity != NULL );
		ASSERT( RI.currententity->model != NULL );

		// sky entities never gets the shadowing
		if( RI.currententity->curstate.renderfx == SKYBOX_ENTITY )
			continue;

		switch( RI.currententity->model->type )
		{
		case mod_brush:
			R_ShadowStaticModel( pl, RI.currententity );
			break;
		default:
			HOST_ERROR( "R_LightStatics: non bsp model in static list!\n" );
			break;
		}
	}

	// restore the world entity
	RI.currententity = GET_ENTITY( 0 );
	RI.currentmodel = RI.currententity->model;
}

/*
=================
R_DrawShadowChains

Shadow pass, zbuffer only
=================
*/
void R_DrawShadowChains( void )
{
	int		j;
	msurface_t	*s;
	mextrasurf_t	*es;
	texture_t		*t;
	float		*v;

	for( int i = 0; i < worldmodel->numtextures; i++ )
	{
		t = worldmodel->textures[i];
		if( !t ) continue;

		es = t->lightchain;
		if( !es ) continue;

		for( ; es != NULL; es = es->lightchain )
		{
			s = INFO_SURF( es, RI.currentmodel );
			pglBegin( GL_POLYGON );

			for( j = 0, v = s->polys->verts[0]; j < s->polys->numverts; j++, v += VERTEXSIZE )
				pglVertex3fv( v );
			pglEnd();
		}
		t->lightchain = NULL;
	}
}

/*
=============
R_ShadowPassDrawWorld
=============
*/
static void R_ShadowPassDrawWorld( const DynamicLight *pl )
{
	int	i;

	// restore worldmodel
	RI.currententity = GET_ENTITY( 0 );
	RI.currentmodel = RI.currententity->model;
	tr.num_draw_surfaces = 0;
	tr.num_draw_meshes = 0;
	tr.modelorg = RI.vieworg;

	R_LoadIdentity();

	// register worldviewProjectionMatrix at zero entry (~80% hits)
	RI.currententity->hCachedMatrix = GL_RegisterCachedMatrix( RI.gl_modelviewProjectionMatrix, RI.gl_modelviewMatrix, tr.modelorg );

	if( pl->key != SUNLIGHT_KEY )
		R_RecursiveShadowNode( worldmodel->nodes, pl->frustum, pl->clipflags );

	if( r_newrenderer->value )
	{
		// add all solid bmodels
		for( i = 0; i < tr.num_solid_bmodel_ents && pl->key != SUNLIGHT_KEY; i++ )
			R_AddBmodelToDrawList( tr.solid_bmodel_ents[i] );

		// add all solid studio
		for( i = 0; i < tr.num_solid_studio_ents; i++ )
			R_AddStudioToDrawList( tr.solid_studio_ents[i] );
	}
	else
	{
		R_ShadowStaticBrushes( pl );
		R_DrawShadowChains();
	}
}

/*
=================
R_ShadowPassDrawBrushModel
=================
*/
void R_ShadowPassDrawBrushModel( cl_entity_t *e, const DynamicLight *pl )
{
	Vector	mins, maxs;
	model_t	*clmodel;
	bool	rotated;

	if( r_newrenderer->value )
		return;

	clmodel = e->model;

	if( e->angles != g_vecZero )
	{
		for( int i = 0; i < 3; i++ )
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

	if( rotated ) R_RotateForEntity( e );
	else R_TranslateForEntity( e );

	e->visframe = tr.framecount; // visible

	if( rotated ) tr.modelorg = RI.objectMatrix.VectorITransform( RI.vieworg );
	else tr.modelorg = RI.vieworg - e->origin;

	// accumulate lit surfaces
	msurface_t *psurf = &clmodel->surfaces[clmodel->firstmodelsurface];
	for( int i = 0; i < clmodel->nummodelsurfaces; i++, psurf++ )
	{
		float *v;
		int k;

		if( psurf->flags & (SURF_DRAWTILED|SURF_REFLECT))
			continue;

		if( R_CullSurfaceExt( psurf, pl->frustum, 0 ))
			continue;

		// draw depth-mask on transparent textures
		if( psurf->flags & SURF_TRANSPARENT )
		{
			pglEnable( GL_ALPHA_TEST );
			pglEnable( GL_TEXTURE_2D );
			pglAlphaFunc( GL_GREATER, 0.0f );
			GL_Bind( GL_TEXTURE0, psurf->texinfo->texture->gl_texturenum );
		}

		pglBegin( GL_POLYGON );
		for( k = 0, v = psurf->polys->verts[0]; k < psurf->polys->numverts; k++, v += VERTEXSIZE )
		{
			if( psurf->flags & SURF_TRANSPARENT )
				pglTexCoord2f( v[3], v[4] );
			pglVertex3fv( v );
		}
		pglEnd();

		if( psurf->flags & SURF_TRANSPARENT )
		{
			pglDisable( GL_ALPHA_TEST );
			pglDisable( GL_TEXTURE_2D );
		}
	}

	R_LoadIdentity();	// restore worldmatrix
}

void R_ShadowPassDrawSolidEntities( const DynamicLight *pl )
{
	glState.drawTrans = false;

	R_RenderShadowBrushList();

	R_RenderShadowStudioList();

	// draw solid entities only.
	for( int i = 0; i < tr.num_solid_entities; i++ )
	{
		if( RI.refdef.onlyClientDraw )
			break;

		RI.currententity = tr.solid_entities[i];
		RI.currentmodel = RI.currententity->model;

		if( RI.currententity->curstate.renderfx == SKYBOX_ENTITY )
			continue;

		// tell engine about current entity
		SET_CURRENT_ENTITY( RI.currententity );
	
		ASSERT( RI.currententity != NULL );
		ASSERT( RI.currententity->model != NULL );

		switch( RI.currentmodel->type )
		{
		case mod_brush:
			R_ShadowPassDrawBrushModel( RI.currententity, pl );
			break;
		case mod_studio:
			R_DrawStudioModel( RI.currententity );
			break;
		case mod_sprite:
			R_DrawSpriteModel( RI.currententity );
			break;
		default:
			break;
		}
	}

	// may be solid cables
	CL_DrawBeams( false );

	GrassDraw();

	// NOTE: some mods with custom renderer may generate glErrors
	// so we clear it here
	while( pglGetError() != GL_NO_ERROR );
}

/*
================
R_RenderScene

RI.refdef must be set before the first call
fast version of R_RenderScene: no colors, no texcords etc
================
*/
void R_RenderShadowScene( const ref_params_t *pparams, const DynamicLight *pl )
{
	// set the worldmodel
	worldmodel = GET_ENTITY( 0 )->model;

	if( !worldmodel )
	{
		ALERT( at_error, "R_RenderShadowView: NULL worldmodel\n" );
		return;
	}

	R_ShadowPassSetupFrame( pl );
	R_ShadowPassSetupGL( pl );
	pglClear( GL_DEPTH_BUFFER_BIT );

	R_MarkLeaves();
	R_ShadowPassDrawWorld( pl );

	R_ShadowPassDrawSolidEntities( pl );

	R_ShadowPassEndGL();
}

void R_RenderShadowmaps( void )
{
	ref_instance_t	oldRI;
	unsigned int	oldFBO;

	if( RI.refdef.onlyClientDraw || r_fullbright->value || !r_shadows->value || RI.refdef.paused )
		return;

	if( FBitSet( RI.params, RP_NOSHADOWS ))
		return;

	// check for dynamic lights
	if( !HasDynamicLights( )) return;

	oldRI = RI; // make refinst backup
	oldFBO = glState.frameBuffer;
	tr.framecount++;

	for( int i = 0; i < MAX_DLIGHTS; i++ )
	{
		DynamicLight *pl = &cl_dlights[i];

		if( pl->die < GET_CLIENT_TIME() || !pl->radius )
			continue;

		if( !pl->spotlightTexture )
		{
			pl->shadowTexture = 0;
			continue;
                    }

		// don't cull by PVS, because we may cull visible shadow here
		if( R_CullSphereExt( RI.frustum, pl->origin, pl->radius, RI.clipFlags ))
			continue;

		RI.params = RP_SHADOWVIEW|RP_MERGEVISIBILITY;

		if( GL_Support( R_FRAMEBUFFER_OBJECT ))
		{
			if( pl->key == SUNLIGHT_KEY )
			{
				RI.viewport[2] = tr.fbo_sunshadow.GetWidth();
				RI.viewport[3] = tr.fbo_sunshadow.GetHeight();
				RI.params |= RP_SUNSHADOWS;

				pl->shadowTexture = tr.fbo_sunshadow.GetTexture();
				tr.fbo_sunshadow.Bind();
			}
			else
			{
				RI.viewport[2] = tr.fbo_shadow.GetWidth();
				RI.viewport[3] = tr.fbo_shadow.GetHeight();

				pl->shadowTexture = R_AllocateShadowTexture( false );
				tr.fbo_shadow.Bind( pl->shadowTexture );
			}
		}
		else
		{
			// simple size if FBO was missed
			RI.viewport[2] = RI.viewport[3] = 512;
		}

		R_RenderShadowScene( &RI.refdef, pl );

		if( !GL_Support( R_FRAMEBUFFER_OBJECT ))
			pl->shadowTexture = R_AllocateShadowTexture();
		RI = oldRI; // restore ref instance
	}

	// restore FBO state
	GL_BindFBO( oldFBO );
}