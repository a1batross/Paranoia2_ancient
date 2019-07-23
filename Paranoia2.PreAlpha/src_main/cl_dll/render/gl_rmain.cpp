/*
gl_rmain.cpp - renderer main loop
Copyright (C) 2013 Uncle Mike

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

#include "ref_params.h"
#include "entity_types.h"
#include "gl_local.h"
#include <mathlib.h>
#include <stringlib.h>
#include "pmtrace.h"
#include "gl_aurora.h"
#include "gl_rpart.h"
#include "triapiobjects.h"
#include "gl_studio.h"
#include "gl_sprite.h"
#include "rain.h"
#include "event_api.h"

extern "C" int DLLEXPORT HUD_GetRenderInterface( int version, render_api_t *renderfuncs, render_interface_t *callback );

ref_programs_t	cg;
ref_globals_t	tr;
ref_instance_t	RI;
ref_shaders_t	glsl;
ref_stats_t	r_stats;
ref_params_t	r_lastRefdef;
char		r_speeds_msg[2048];
model_t		*worldmodel = NULL;	// must be set at begin each frame
float		gldepthmin, gldepthmax;
mleaf_t		*r_viewleaf, *r_oldviewleaf;
mleaf_t		*r_viewleaf2, *r_oldviewleaf2;
int		r_currentMessageNum = 0;

bool R_SkyIsVisible( void )
{
	return ( RI.params & RP_SKYVISIBLE) ? true : false;
}

int R_RankForRenderMode( cl_entity_t *ent )
{
	switch( ent->curstate.rendermode )
	{
	case kRenderTransTexture:
		return 1;	// draw second
	case kRenderTransAdd:
		return 2;	// draw third
	case kRenderGlow:
		return 3;	// must be last!
	}
	return 0;
}

void GL_ResetMatrixCache( void )
{
	memset( tr.cached_matrices, 0, sizeof( tr.cached_matrices ));
	tr.num_cached_matrices = 0;
}

word GL_RegisterCachedMatrix( const GLfloat MVP[16], const GLfloat MV[16], const Vector &modelorg, const float *texofs )
{
	for( int i = 0; i < tr.num_cached_matrices; i++ )
	{
		if( !memcmp( tr.cached_matrices[i].modelviewProjectionMatrix, MVP, sizeof( GLfloat ) * 16 ))
		{
			if( texofs != NULL )
			{
				if( !memcmp( tr.cached_matrices[i].texofs, texofs, sizeof( GLfloat ) * 2 ))
					return i;
			}
			else
			{
				return i;
			}
		}
	}

	if( tr.num_cached_matrices >= MAX_CACHED_MATRICES )
	{
		ALERT( at_error, "GL_RegisterCachedMatrix: cache reached end (%i max)\n", MAX_CACHED_MATRICES );
		return 0;	// default
	}

	memcpy( tr.cached_matrices[i].modelviewProjectionMatrix, MVP, sizeof( GLfloat ) * 16 );
	memcpy( tr.cached_matrices[i].modelviewMatrix, MV, sizeof( GLfloat ) * 16 );
	tr.cached_matrices[i].modelorg = modelorg;

	if( texofs != NULL )
	{
		tr.cached_matrices[i].texofs[0] = texofs[0];
		tr.cached_matrices[i].texofs[1] = texofs[1];
	}
	
	return tr.num_cached_matrices++;
}

/*
===============
R_StaticEntity

Static entity is the brush which has no custom origin and not rotated
typically is a func_wall, func_breakable, func_ladder etc
===============
*/
static qboolean R_StaticEntity( cl_entity_t *ent )
{
	if( !r_allow_static->value )
		return false;

	if( ent->curstate.rendermode != kRenderNormal )
		return false;

	if( ent->curstate.renderfx == SKYBOX_ENTITY )
		return false;

	if( ent->model->type != mod_brush )
		return false;

	if( ent->curstate.effects & ( EF_SCREENMOVIE ))
		return false;

	if( ent->curstate.effects & ( EF_NOREFLECT|EF_REFLECTONLY ))
		return false;

	if( ent->curstate.frame || ent->model->flags & MODEL_CONVEYOR )
		return false;

	if( ent->curstate.scale ) // waveheight specified
		return false;

	if( ent->origin != g_vecZero || ent->angles != g_vecZero )
		return false;

	return true;
}

/*
===============
R_FollowEntity

Follow entity is attached to another studiomodel and used last cached bones
from parent
===============
*/
static qboolean R_FollowEntity( cl_entity_t *ent )
{
	if( ent->model->type != mod_studio )
		return false;

	if( ent->curstate.movetype != MOVETYPE_FOLLOW )
		return false;

	if( ent->curstate.aiment <= 0 )
		return false;

	return true;
}

/*
===============
R_OpaqueEntity

Opaque entity can be brush or studio model but sprite
===============
*/
static qboolean R_OpaqueEntity( cl_entity_t *ent )
{
	if( ent->curstate.rendermode == kRenderNormal )
		return true;

	if( ent->model->type == mod_sprite )
		return false;

	if( ent->curstate.rendermode == kRenderTransAlpha )
		return true;

	return false;
}

/*
===============
R_SolidEntityCompare

Sorting opaque entities by model type
===============
*/
static int R_SolidEntityCompare( const cl_entity_t **a, const cl_entity_t **b )
{
	cl_entity_t	*ent1, *ent2;

	ent1 = (cl_entity_t *)*a;
	ent2 = (cl_entity_t *)*b;

	if( ent1->model->type > ent2->model->type )
		return 1;
	if( ent1->model->type < ent2->model->type )
		return -1;

	return 0;
}

/*
===============
R_TransEntityCompare

Sorting translucent entities by rendermode then by distance
===============
*/
static int R_TransEntityCompare( const cl_entity_t **a, const cl_entity_t **b )
{
	cl_entity_t *ent1 = (cl_entity_t *)*a;
	cl_entity_t *ent2 = (cl_entity_t *)*b;

	// now sort by rendermode
	if( R_RankForRenderMode( ent1 ) > R_RankForRenderMode( ent2 ))
		return 1;
	if( R_RankForRenderMode( ent1 ) < R_RankForRenderMode( ent2 ))
		return -1;

	Vector	org;
	float	len1, len2;

	// then by distance
	if( ent1->model->type == mod_brush )
	{
		org = ent1->origin + ((ent1->model->mins + ent1->model->maxs) * 0.5f);
		len1 = (RI.vieworg - org).Length();
	}
	else
	{
		len1 = (RI.vieworg - ent1->origin).Length();
	}

	if( ent2->model->type == mod_brush )
	{
		org = ent2->origin + ((ent2->model->mins + ent2->model->maxs) * 0.5f);
		len2 = (RI.vieworg - org).Length();
	}
	else
	{
		len2 = (RI.vieworg - ent2->origin).Length();
	}

	if( len1 > len2 )
		return -1;
	if( len1 < len2 )
		return 1;

	return 0;
}

qboolean R_WorldToScreen( const Vector &point, Vector &screen )
{
	matrix4x4	worldToScreen;
	qboolean	behind;
	float	w;

	worldToScreen = RI.worldviewProjectionMatrix;

	screen[0] = worldToScreen[0][0] * point[0] + worldToScreen[1][0] * point[1] + worldToScreen[2][0] * point[2] + worldToScreen[3][0];
	screen[1] = worldToScreen[0][1] * point[0] + worldToScreen[1][1] * point[1] + worldToScreen[2][1] * point[2] + worldToScreen[3][1];
//	z = worldToScreen[0][2] * point[0] + worldToScreen[1][2] * point[1] + worldToScreen[2][2] * point[2] + worldToScreen[3][2];
	w = worldToScreen[0][3] * point[0] + worldToScreen[1][3] * point[1] + worldToScreen[2][3] * point[2] + worldToScreen[3][3];
	screen[2] = 0.0f; // just so we have something valid here

	if( w < 0.001f )
	{
		behind = true;
		screen[0] *= 100000;
		screen[1] *= 100000;
	}
	else
	{
		float invw = 1.0f / w;
		behind = false;
		screen[0] *= invw;
		screen[1] *= invw;
	}
	return behind;
}

void R_ScreenToWorld( const Vector &screen, Vector &point )
{
	matrix4x4	screenToWorld;
	Vector	temp;
	float	w;

	screenToWorld = RI.worldviewProjectionMatrix.InvertFull();

	temp[0] = 2.0f * (screen[0] - RI.viewport[0]) / RI.viewport[2] - 1;
	temp[1] = -2.0f * (screen[1] - RI.viewport[1]) / RI.viewport[3] + 1;
	temp[2] = 0.0f; // just so we have something valid here

	point[0] = temp[0] * screenToWorld[0][0] + temp[1] * screenToWorld[0][1] + temp[2] * screenToWorld[0][2] + screenToWorld[0][3];
	point[1] = temp[0] * screenToWorld[1][0] + temp[1] * screenToWorld[1][1] + temp[2] * screenToWorld[1][2] + screenToWorld[1][3];
	point[2] = temp[0] * screenToWorld[2][0] + temp[1] * screenToWorld[2][1] + temp[2] * screenToWorld[2][2] + screenToWorld[2][3];
	w = temp[0] * screenToWorld[3][0] + temp[1] * screenToWorld[3][1] + temp[2] * screenToWorld[3][2] + screenToWorld[3][3];
	if( w ) point *= ( 1.0f / w );
}

/*
===============
R_ComputeFxBlend
===============
*/
int R_ComputeFxBlend( cl_entity_t *e )
{
	int	blend = 0, renderAmt;
	float	offset, dist;
	Vector	tmp;

	offset = ((int)e->index ) * 363.0f; // Use ent index to de-sync these fx
	renderAmt = e->curstate.renderamt;

	switch( e->curstate.renderfx ) 
	{
	case kRenderFxPulseSlowWide:
		blend = renderAmt + 0x40 * sin( RI.refdef.time * 2 + offset );	
		break;
	case kRenderFxPulseFastWide:
		blend = renderAmt + 0x40 * sin( RI.refdef.time * 8 + offset );
		break;
	case kRenderFxPulseSlow:
		blend = renderAmt + 0x10 * sin( RI.refdef.time * 2 + offset );
		break;
	case kRenderFxPulseFast:
		blend = renderAmt + 0x10 * sin( RI.refdef.time * 8 + offset );
		break;
	// JAY: HACK for now -- not time based
	case kRenderFxFadeSlow:			
		if( renderAmt > 0 ) 
			renderAmt -= 1;
		else renderAmt = 0;
		blend = renderAmt;
		break;
	case kRenderFxFadeFast:
		if( renderAmt > 3 ) 
			renderAmt -= 4;
		else renderAmt = 0;
		blend = renderAmt;
		break;
	case kRenderFxSolidSlow:
		if( renderAmt < 255 ) 
			renderAmt += 1;
		else renderAmt = 255;
		blend = renderAmt;
		break;
	case kRenderFxSolidFast:
		if( renderAmt < 252 ) 
			renderAmt += 4;
		else renderAmt = 255;
		blend = renderAmt;
		break;
	case kRenderFxStrobeSlow:
		blend = 20 * sin( RI.refdef.time * 4 + offset );
		if( blend < 0 ) blend = 0;
		else blend = renderAmt;
		break;
	case kRenderFxStrobeFast:
		blend = 20 * sin( RI.refdef.time * 16 + offset );
		if( blend < 0 ) blend = 0;
		else blend = renderAmt;
		break;
	case kRenderFxStrobeFaster:
		blend = 20 * sin( RI.refdef.time * 36 + offset );
		if( blend < 0 ) blend = 0;
		else blend = renderAmt;
		break;
	case kRenderFxFlickerSlow:
		blend = 20 * (sin( RI.refdef.time * 2 ) + sin( RI.refdef.time * 17 + offset ));
		if( blend < 0 ) blend = 0;
		else blend = renderAmt;
		break;
	case kRenderFxFlickerFast:
		blend = 20 * (sin( RI.refdef.time * 16 ) + sin( RI.refdef.time * 23 + offset ));
		if( blend < 0 ) blend = 0;
		else blend = renderAmt;
		break;
	case kRenderFxHologram:
	case kRenderFxDistort:
		tmp = e->origin - RI.refdef.vieworg;
		dist = DotProduct( tmp, RI.refdef.forward );
			
		// Turn off distance fade
		if( e->curstate.renderfx == kRenderFxDistort )
			dist = 1;

		if( dist <= 0 )
		{
			blend = 0;
		}
		else 
		{
			renderAmt = 180;
			if( dist <= 100 ) blend = renderAmt;
			else blend = (int) ((1.0f - ( dist - 100 ) * ( 1.0f / 400.0f )) * renderAmt );
			blend += RANDOM_LONG( -32, 31 );
		}
		break;
	case kRenderFxGlowShell:	// safe current renderamt because it's shell scale!
	case kRenderFxDeadPlayer:	// safe current renderamt because it's player index!
		blend = renderAmt;
		break;
	case kRenderFxNone:
	case kRenderFxClampMinScale:
	default:
		if( e->curstate.rendermode == kRenderNormal )
			blend = 255;
		else blend = renderAmt;
		break;	
	}

	if( e->model->type != mod_brush )
	{
		// NOTE: never pass sprites with rendercolor '0 0 0' it's a stupid Valve Hammer Editor bug
		if( !e->curstate.rendercolor.r && !e->curstate.rendercolor.g && !e->curstate.rendercolor.b )
			e->curstate.rendercolor.r = e->curstate.rendercolor.g = e->curstate.rendercolor.b = 255;
	}

	// apply scale to studiomodels and sprites only
	if( e->model && e->model->type != mod_brush && !e->curstate.scale )
		e->curstate.scale = 1.0f;

	blend = bound( 0, blend, 255 );

	return blend;
}

/*
===============
R_ClearScene
===============
*/
void R_ClearScene( void )
{
	if( !g_fXashEngine || !g_fRenderInitialized )
		return;

	MY_DecayLights();

	memset( &r_stats, 0, sizeof( r_stats ));

	tr.num_solid_studio_ents = 0;
	tr.num_solid_bmodel_ents = 0;
	tr.num_trans_studio_ents = 0;
	tr.num_trans_bmodel_ents = 0;
	tr.local_client_added = false;

	tr.num_solid_entities = tr.num_trans_entities = 0;
	tr.num_child_entities = tr.num_beams_entities =0;
	tr.num_static_entities = tr.num_glow_sprites = 0;
	tr.num_water_entities = tr.num_glass_entities = 0;
	tr.num_shadows_used = tr.num_glow_sprites = 0;
	tr.num_mirror_entities = 0;

	if( r_sunshadows->value > 3.0f )
		CVAR_SET_FLOAT( "gl_sun_shadows", 3.0f );
	else if( r_sunshadows->value < 0.0f )
		CVAR_SET_FLOAT( "gl_sun_shadows", 0.0f );		

	// NOTE: sunlight must be added first in list
	if( tr.world_has_skybox && r_sunshadows->value && tr.fbo_sunshadow.Active( ))
	{
		if( RI.refdef.movevars != NULL )
		{
			DynamicLight *dl = MY_AllocDlight( SUNLIGHT_KEY );
			movevars_t *mv = RI.refdef.movevars;
			Vector origin, angles;

			VectorAngles2( Vector( mv->skyvec_x, mv->skyvec_y, mv->skyvec_z ), angles );
			origin = ((worldmodel->mins + worldmodel->maxs) * 0.5f) + worldmodel->maxs.z;
			R_SetupLightProjection( dl, origin, angles, 8192.0f, 90.0f, tr.whiteTexture );

			// shadow only
			dl->color[0] = dl->color[1] = dl->color[2] = 1.0f;
			dl->die = GET_CLIENT_TIME();
		}
	}
}

/*
===============
R_AddEntity
===============
*/
qboolean R_AddEntity( struct cl_entity_s *clent, int entityType )
{
	if( !r_drawentities->value )
		return false; // not allow to drawing

	if( !clent || !clent->model )
		return false; // if set to invisible, skip

	if( clent->curstate.effects & EF_NODRAW )
		return false; // done

	if( entityType == ET_PLAYER && RP_LOCALCLIENT( clent ))
	{
		if( tr.local_client_added )
			return false; // already present in list
		tr.local_client_added = true;
	}

	if( clent->curstate.renderfx == 71 ) // dynamic light
	{
		DynamicLight *dl = MY_AllocDlight( clent->curstate.number );

		float radius = clent->curstate.renderamt * 8;
		float fov = clent->curstate.scale;
		Vector origin, angles;
		int tex = 0;

		if( clent->curstate.scale ) // spotlight
		{
			int i = bound( 0, clent->curstate.rendermode, 7 );
			tex = tr.spotlightTexture[i];
		}		

		R_GetLightVectors( clent, origin, angles );
		R_SetupLightProjection( dl, origin, angles, radius, clent->curstate.scale, tex );

		dl->color[0] = (float)clent->curstate.rendercolor.r / 128;
		dl->color[1] = (float)clent->curstate.rendercolor.g / 128;
		dl->color[2] = (float)clent->curstate.rendercolor.b / 128;
		dl->die = GET_CLIENT_TIME() + 0.05f;

		return true; // no reason to drawing this entity
          }
	else if( clent->curstate.renderfx == 72 ) // dynamic light with avi file
	{
		if( !clent->curstate.sequence )
			return true; // bad avi file

		DynamicLight *dl = MY_AllocDlight( clent->curstate.number );

		if( dl->spotlightTexture == tr.spotlightTexture[1] )
			return true; // bad avi file

		float radius = clent->curstate.renderamt * 8;
		float fov = clent->curstate.scale;
		Vector origin, angles;

		// found the corresponding cinstate
		const char *cinname = gEngfuncs.pEventAPI->EV_EventForIndex( clent->curstate.sequence );
		int hCin = R_PrecacheCinematic( cinname );

		if( hCin >= 0 && !dl->cinTexturenum )
			dl->cinTexturenum = R_AllocateCinematicTexture( TF_SPOTLIGHT );

		if( hCin == -1 || dl->cinTexturenum <= 0 || !CIN_IS_ACTIVE( tr.cinematics[hCin].state ))
		{
			// cinematic textures limit exceeded or movie not found
			dl->spotlightTexture = tr.spotlightTexture[1];
			return true;
		}

		gl_movie_t *cin = &tr.cinematics[hCin];
		float cin_time;

		// advances cinematic time
		cin_time = fmod( clent->curstate.fuser2, cin->length );

		// read the next frame
		int cin_frame = CIN_GET_FRAME_NUMBER( cin->state, cin_time );

		if( cin_frame != dl->lastframe )
		{
			// upload the new frame
			byte *raw = CIN_GET_FRAMEDATA( cin->state, cin_frame );
			CIN_UPLOAD_FRAME( tr.cinTextures[dl->cinTexturenum-1], cin->xres, cin->yres, cin->xres, cin->yres, raw );
			dl->lastframe = cin_frame;
		}

		R_GetLightVectors( clent, origin, angles );
		R_SetupLightProjection( dl, origin, angles, radius, clent->curstate.scale, tr.cinTextures[dl->cinTexturenum-1] );

		dl->color[0] = (float)clent->curstate.rendercolor.r / 128;
		dl->color[1] = (float)clent->curstate.rendercolor.g / 128;
		dl->color[2] = (float)clent->curstate.rendercolor.b / 128;
		dl->die = GET_CLIENT_TIME() + 0.05f;

		return true; // no reason to drawing this entity
          }

	// handle glow entity
	if( GlowFilterEntities( entityType, clent, clent->model->name )) // buz
		return false;

	clent->curstate.renderamt = R_ComputeFxBlend( clent );

	if( clent->curstate.rendermode != kRenderNormal && clent->curstate.rendermode != kRenderTransAlpha )
	{
		if( clent->curstate.renderamt <= 0.0f )
			return true; // invisible
	}

	clent->curstate.entityType = entityType;

//	if( entityType == ET_TEMPENTITY || entityType == ET_FRAGMENTED )
//		clent->modelhandle = INVALID_HANDLE;

	if( R_FollowEntity( clent ))
	{
		// follow entity
		if( tr.num_child_entities >= MAX_VISIBLE_PACKET )
			return false;

		tr.child_entities[tr.num_child_entities] = clent;
		tr.num_child_entities++;

		return true;
	}

	if( R_OpaqueEntity( clent ))
	{
		if( R_StaticEntity( clent ))
		{
			// opaque static
			if( tr.num_static_entities >= MAX_VISIBLE_PACKET )
				return false;

			tr.static_entities[tr.num_static_entities] = clent;
			tr.num_static_entities++;
		}
		else
		{
			// opaque moving
			if( tr.num_solid_entities >= MAX_VISIBLE_PACKET )
				return false;

			tr.solid_entities[tr.num_solid_entities] = clent;
			tr.num_solid_entities++;
		}

		if( clent->model->type == mod_studio )
		{
			if( tr.num_solid_studio_ents < MAX_VISIBLE_PACKET )
			{
				tr.solid_studio_ents[tr.num_solid_studio_ents] = clent;
				tr.num_solid_studio_ents++;
			}
		}
		else if( clent->model->type == mod_brush )
		{
			if( tr.num_solid_bmodel_ents < MAX_VISIBLE_PACKET )
			{
				tr.solid_bmodel_ents[tr.num_solid_bmodel_ents] = clent;
				tr.num_solid_bmodel_ents++;
			}
		}
	}
	else
	{
		if( clent->model->type == mod_brush && glsl.pWaterShader && clent->model->flags & BIT( 2 )) //MODEL_LIQUID
		{
			if( !cv_water->value || tr.num_water_entities >= MAX_VISIBLE_PACKET )
				return false;

			tr.water_entities[tr.num_water_entities] = clent;
			tr.num_water_entities++;
		}
		else if( clent->model->type == mod_brush && clent->curstate.rendermode == kRenderTransTexture && glsl.pGlassShader )
		{
			if( tr.num_glass_entities >= MAX_VISIBLE_PACKET )
				return false;

			tr.glass_entities[tr.num_glass_entities] = clent;
			tr.num_glass_entities++;
		}
		else
		{
			// translucent
			if( tr.num_trans_entities >= MAX_VISIBLE_PACKET )
				return false;

			tr.trans_entities[tr.num_trans_entities] = clent;
			tr.num_trans_entities++;
		}

		if( clent->model->type == mod_studio )
		{
			if( tr.num_trans_studio_ents < MAX_VISIBLE_PACKET )
			{
				tr.trans_studio_ents[tr.num_trans_studio_ents] = clent;
				tr.num_trans_studio_ents++;
			}
		}
		else if( clent->model->type == mod_brush )
		{
			if( tr.num_trans_bmodel_ents < MAX_VISIBLE_PACKET )
			{
				tr.trans_bmodel_ents[tr.num_trans_bmodel_ents] = clent;
				tr.num_trans_bmodel_ents++;
			}
		}
	}

	// mark static entity as visible
	if( entityType == ET_FRAGMENTED )
		clent->visframe = tr.framecount;

	return true;
}

/*
=============
R_Clear
=============
*/
static void R_Clear( int bitMask )
{
	int	bits;

	pglClearColor( 0.5f, 0.5f, 0.5f, 1.0f );

	bits = GL_DEPTH_BUFFER_BIT;

	if( r_fastsky->value )
		bits |= GL_COLOR_BUFFER_BIT;

	if( glState.stencilEnabled )
		bits |= GL_STENCIL_BUFFER_BIT;

	bits &= bitMask;

	pglClear( bits );

	gldepthmin = 0.0f;	// 0.0f - 0.8f (world) 
	gldepthmax = 0.8f;	// 0.8f - 1.0f (3d sky)

	pglDepthFunc( GL_LEQUAL );
	pglDepthRange( gldepthmin, gldepthmax );
	GL_Cull( GL_FRONT );
}

/*
===============
R_SetupFrustum
===============
*/
void R_SetupFrustum( void )
{
	// 0 - left
	// 1 - right
	// 2 - down
	// 3 - up
	// 4 - farclip
	// 5 - nearclip

	Vector farPoint = RI.vieworg + RI.vforward * (RI.refdef.movevars->zmax * 1.5f);

	// rotate RI.vforward right by FOV_X/2 degrees
	RotatePointAroundVector( RI.frustum[0].normal, RI.vup, RI.vforward, -(90 - RI.refdef.fov_x / 2));
	// rotate RI.vforward left by FOV_X/2 degrees
	RotatePointAroundVector( RI.frustum[1].normal, RI.vup, RI.vforward, 90 - RI.refdef.fov_x / 2 );
	// rotate RI.vforward up by FOV_Y/2 degrees
	RotatePointAroundVector( RI.frustum[2].normal, RI.vright, RI.vforward, 90 - RI.refdef.fov_y / 2 );
	// rotate RI.vforward down by FOV_Y/2 degrees
	RotatePointAroundVector( RI.frustum[3].normal, RI.vright, RI.vforward, -(90 - RI.refdef.fov_y / 2));

	for( int i = 0; i < 4; i++ )
	{
		RI.frustum[i].type = PLANE_NONAXIAL;
		RI.frustum[i].dist = DotProduct( RI.vieworg, RI.frustum[i].normal );
		RI.frustum[i].signbits = SignbitsForPlane( RI.frustum[i].normal );
	}

	RI.frustum[4].normal = -RI.vforward;
	RI.frustum[4].type = PLANE_NONAXIAL;
	RI.frustum[4].dist = DotProduct( farPoint, RI.frustum[4].normal );
	RI.frustum[4].signbits = SignbitsForPlane( RI.frustum[4].normal );

	// no need to setup backplane for general view. It's only used for portals and mirrors
}

/*
=============
R_SetupProjectionMatrix
=============
*/
void R_SetupProjectionMatrix( float fov_x, float fov_y, matrix4x4 &m )
{
	GLdouble	xMin, xMax, yMin, yMax, zNear, zFar;

	RI.farClip = RI.refdef.movevars->zmax * 1.5f;

	zNear = 4.0f;
	zFar = max( 256.0f, RI.farClip );

	yMax = zNear * tan( fov_y * M_PI / 360.0 );
	yMin = -yMax;

	xMax = zNear * tan( fov_x * M_PI / 360.0 );
	xMin = -xMax;

	m.CreateProjection( xMax, xMin, yMax, yMin, zNear, zFar );
}

/*
=============
R_SetupModelviewMatrix
=============
*/
static void R_SetupModelviewMatrix( const ref_params_t *pparams, matrix4x4 &m )
{
#if 0
	m.Identity();
	m.ConcatRotate( -90, 1, 0, 0 );
	m.ConcatRotate( 90, 0, 0, 1 );
#else
	m.CreateModelview(); // init quake world orientation
#endif
	m.ConcatRotate( -pparams->viewangles[2], 1, 0, 0 );
	m.ConcatRotate( -pparams->viewangles[0], 0, 1, 0 );
	m.ConcatRotate( -pparams->viewangles[1], 0, 0, 1 );
	m.ConcatTranslate( -pparams->vieworg[0], -pparams->vieworg[1], -pparams->vieworg[2] );
}

/*
=============
R_LoadIdentity
=============
*/
void R_LoadIdentity( bool force )
{
	if( tr.modelviewIdentity && !force )
		return;

	RI.objectMatrix.Identity();
	RI.modelviewMatrix = RI.worldviewMatrix;
	RI.modelviewMatrix.CopyToArray( RI.gl_modelviewMatrix );
	RI.worldviewProjectionMatrix.CopyToArray( RI.gl_modelviewProjectionMatrix );

	pglMatrixMode( GL_MODELVIEW );
	GL_LoadMatrix( RI.modelviewMatrix );
	tr.modelviewIdentity = true;
}

/*
=============
R_RotateForEntity
=============
*/
void R_RotateForEntity( cl_entity_t *e )
{
	float	scale = 1.0f;

	if( e == GET_ENTITY( 0 ) || R_StaticEntity( e ))
	{
		R_LoadIdentity();
		return;
	}

	if( e->model->type == mod_studio && e->curstate.scale > 0.0f && e->curstate.scale <= 16.0f )
		scale = e->curstate.scale;

	RI.objectMatrix = matrix4x4( e->origin, e->angles, scale );
	RI.modelviewMatrix = RI.worldviewMatrix.ConcatTransforms( RI.objectMatrix );
	RI.modelviewMatrix.CopyToArray( RI.gl_modelviewMatrix );

	matrix4x4 modelviewProjectionMatrix = RI.projectionMatrix.Concat( RI.modelviewMatrix );
	modelviewProjectionMatrix.CopyToArray( RI.gl_modelviewProjectionMatrix );

	pglMatrixMode( GL_MODELVIEW );
	GL_LoadMatrix( RI.modelviewMatrix );
	tr.modelviewIdentity = false;
}

/*
=============
R_TranslateForEntity
=============
*/
void R_TranslateForEntity( cl_entity_t *e )
{
	float	scale = 1.0f;

	if( e == GET_ENTITY( 0 ) || R_StaticEntity( e ))
	{
		R_LoadIdentity();
		return;
	}

	if( e->model->type != mod_brush && e->curstate.scale > 0.0f )
		scale = e->curstate.scale;

	RI.objectMatrix = matrix4x4( e->origin, g_vecZero, scale );
	RI.modelviewMatrix = RI.worldviewMatrix.ConcatTransforms( RI.objectMatrix );
	RI.modelviewMatrix.CopyToArray( RI.gl_modelviewMatrix );

	matrix4x4 modelviewProjectionMatrix = RI.projectionMatrix.Concat( RI.modelviewMatrix );
	modelviewProjectionMatrix.CopyToArray( RI.gl_modelviewProjectionMatrix );

	pglMatrixMode( GL_MODELVIEW );
	GL_LoadMatrix( RI.modelviewMatrix );
	tr.modelviewIdentity = false;
}

/*
===============
R_FindViewLeaf
===============
*/
void R_FindViewLeaf( void )
{
	float	height;
	mleaf_t	*leaf;
	Vector	tmp;

	r_oldviewleaf = r_viewleaf;
	r_oldviewleaf2 = r_viewleaf2;
	leaf = Mod_PointInLeaf( RI.pvsorigin, worldmodel->nodes );
	r_viewleaf2 = r_viewleaf = leaf;
	height = RI.waveHeight ? RI.waveHeight : 16;

	// check above and below so crossing solid water doesn't draw wrong
	if( leaf->contents == CONTENTS_EMPTY )
	{
		// look down a bit
		tmp = RI.pvsorigin;
		tmp[2] -= height;
		leaf = Mod_PointInLeaf( tmp, worldmodel->nodes );
		if(( leaf->contents != CONTENTS_SOLID ) && ( leaf != r_viewleaf2 ))
			r_viewleaf2 = leaf;
	}
	else
	{
		// look up a bit
		tmp = RI.pvsorigin;
		tmp[2] += height;
		leaf = Mod_PointInLeaf( tmp, worldmodel->nodes );
		if(( leaf->contents != CONTENTS_SOLID ) && ( leaf != r_viewleaf2 ))
			r_viewleaf2 = leaf;
	}
}

/*
===============
R_SetupFrame
===============
*/
static void R_SetupFrame( void )
{
	// build the transformation matrix for the given view angles
	RI.vieworg = RI.refdef.vieworg;
	AngleVectors( RI.refdef.viewangles, RI.vforward, RI.vright, RI.vup );

	// setup viewplane dist
	RI.viewplanedist = DotProduct( RI.refdef.vieworg, RI.vforward );

	GL_ResetMatrixCache();

	tr.dlightframecount = tr.framecount++;

	// sort translucents entities by rendermode and distance
	qsort( tr.trans_entities, tr.num_trans_entities, sizeof( cl_entity_t* ), (cmpfunc)R_TransEntityCompare );

	// sort water entities by distance
	qsort( tr.water_entities, tr.num_water_entities, sizeof( cl_entity_t* ), (cmpfunc)R_TransEntityCompare );

	// sort glass entities by distance
	qsort( tr.glass_entities, tr.num_glass_entities, sizeof( cl_entity_t* ), (cmpfunc)R_TransEntityCompare );

	// current viewleaf
	RI.waveHeight = RI.refdef.movevars->waveHeight * 2.0f;	// set global waveheight

	if(!( RI.params & RP_OLDVIEWLEAF ))
		R_FindViewLeaf();
}

/*
=============
R_SetupGL
=============
*/
static void R_SetupGL( void )
{
	if( RI.refdef.waterlevel >= 3 )
	{
		float f = sin( RI.refdef.time * 0.4f * ( M_PI * 1.7f ));
		RI.refdef.fov_x += f;
		RI.refdef.fov_y -= f;
	}

	R_SetupModelviewMatrix( &RI.refdef, RI.worldviewMatrix );
	R_SetupProjectionMatrix( RI.refdef.fov_x, RI.refdef.fov_y, RI.projectionMatrix );

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

	if( RI.params & RP_CLIPPLANE )
	{
		GLdouble	clip[4];
		mplane_t	*p = &RI.clipPlane;

		clip[0] = p->normal[0];
		clip[1] = p->normal[1];
		clip[2] = p->normal[2];
		clip[3] = -p->dist;

		pglClipPlane( GL_CLIP_PLANE0, clip );
		pglEnable( GL_CLIP_PLANE0 );
	}

	GL_Cull( GL_FRONT );

	pglDisable( GL_BLEND );
	pglDisable( GL_ALPHA_TEST );
	pglColor4f( 1.0f, 1.0f, 1.0f, 1.0f );
}

/*
=============
R_EndGL
=============
*/
static void R_EndGL( void )
{
	if( RI.params & RP_CLIPPLANE )
		pglDisable( GL_CLIP_PLANE0 );
}

/*
=============
R_DrawParticles

NOTE: particles are drawing with engine methods
=============
*/
void R_DrawParticles( void )
{
	DRAW_PARTICLES( RI.vieworg, RI.vforward, RI.vright, RI.vup, RI.clipFlags );
}

/*
=============
R_DrawEntitiesOnList
=============
*/
void R_DrawEntitiesOnList( void )
{
	int	i;

	glState.drawTrans = false;

	R_RenderSolidBrushList();	// world + bmodels

	R_RenderSolidStudioList();	// opaque studio

	// first draw solid entities
	for( i = 0; i < tr.num_solid_entities; i++ )
	{
		if( RI.refdef.onlyClientDraw )
			break;

		RI.currententity = tr.solid_entities[i];
		RI.currentmodel = RI.currententity->model;

		// tell engine about current entity
		SET_CURRENT_ENTITY( RI.currententity );
	
		ASSERT( RI.currententity != NULL );
		ASSERT( RI.currententity->model != NULL );

		if( RENDER_GET_PARM( PARM_ACTIVE_TMU, 0 ) != 0 )
			ALERT( at_error, "Active TMU is %i\n", RENDER_GET_PARM( PARM_ACTIVE_TMU, 0 ));

		switch( RI.currentmodel->type )
		{
		case mod_brush:
			R_DrawBrushModel( RI.currententity );
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

	CL_DrawBeams( false );

	GrassDraw();

	// NOTE: some mods with custom renderer may generate glErrors
	// so we clear it here
	while( pglGetError() != GL_NO_ERROR );

	pglDepthMask( GL_FALSE );
	glState.drawTrans = true;

	// disable the fog on translucent surfaces
	BlackFog();

	// then draw translucent entities
	for( i = 0; i < tr.num_trans_entities; i++ )
	{
		if( RI.refdef.onlyClientDraw )
			break;

		RI.currententity = tr.trans_entities[i];
		RI.currentmodel = RI.currententity->model;

		// tell engine about current entity
		SET_CURRENT_ENTITY( RI.currententity );
	
		ASSERT( RI.currententity != NULL );
		ASSERT( RI.currententity->model != NULL );

		if( RENDER_GET_PARM( PARM_ACTIVE_TMU, 0 ) != 0 )
			ALERT( at_error, "Active TMU is %i\n", RENDER_GET_PARM( PARM_ACTIVE_TMU, 0 ));

		switch( RI.currentmodel->type )
		{
		case mod_brush:
			R_DrawBrushModel( RI.currententity );
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

	DrawTextureVecs ();

	g_pParticleSystems.UpdateSystems();

	g_pParticles.Update();

   	//22/03/03 LRC: shiny surfaces
	if( gHUD.m_pShinySurface )
		gHUD.m_pShinySurface->DrawAll( RI.vieworg );

	CL_DrawBeams( true );

	R_DrawParticles();

	g_objmanager.Draw();

	R_DrawWeather();

	DrawGlows();

	if( r_newrenderer->value )
		R_AddTransToDrawList();

	R_RenderTransBrushList();

	R_RenderTransStudioList();

	if( tr.num_water_entities && !RI.refdef.onlyClientDraw )
	{		
		pglDisable( GL_BLEND );
		pglColor4ub( 255, 255, 255, 255 );

		for( i = 0; i < tr.num_water_entities; i++ )
		{
			RI.currententity = tr.water_entities[i];
			RI.currentmodel = RI.currententity->model;

			// tell engine about current entity
			SET_CURRENT_ENTITY( RI.currententity );

			ASSERT( RI.currententity != NULL );
			ASSERT( RI.currententity->model != NULL );

			R_DrawWaterBrushModel( RI.currententity );
		}
	}

	if( tr.num_glass_entities && !RI.refdef.onlyClientDraw )
	{		
		pglDisable( GL_BLEND );
		pglColor4ub( 255, 255, 255, 255 );

		for( i = 0; i < tr.num_glass_entities; i++ )
		{
			RI.currententity = tr.glass_entities[i];
			RI.currentmodel = RI.currententity->model;

			// tell engine about current entity
			SET_CURRENT_ENTITY( RI.currententity );
		
			ASSERT( RI.currententity != NULL );
			ASSERT( RI.currententity->model != NULL );

			R_DrawGlassBrushModel( RI.currententity );
		}
	}

	// NOTE: some mods with custom renderer may generate glErrors
	// so we clear it here
	while( pglGetError() != GL_NO_ERROR );

	glState.drawTrans = false;
	pglDepthMask( GL_TRUE );
	pglDisable( GL_BLEND );	// Trinity Render issues
}

/*
================
R_RenderScene

RI.refdef must be set before the first call
================
*/
void R_RenderScene( const ref_params_t *pparams )
{
	RI.refdef = *pparams;

	R_RenderShadowmaps();

	R_SetupFrame();
	R_SetupFrustum();
	R_SetupGL();
	R_Clear( ~0 );

	R_MarkLeaves();

	if( r_newrenderer->value )
		R_AddSolidToDrawList();
	else R_DrawWorld();

	R_DrawEntitiesOnList();

	R_EndGL();
}

void HUD_PrintStats( void )
{
	if( r_speeds->value <= 0 )
		return;

	msurface_t *surf = r_stats.debug_surface;

	switch( (int)r_speeds->value )
	{
	case 1:
		Q_snprintf( r_speeds_msg, sizeof( r_speeds_msg ), "%3i wpoly, %3i epoly\n %3i spoly, %3i grass poly\n %3i light poly",
		r_stats.c_world_polys, r_stats.c_studio_polys, r_stats.c_sprite_polys, r_stats.c_grass_polys, r_stats.c_light_polys );
		break;		
	case 2:
		Q_snprintf( r_speeds_msg, sizeof( r_speeds_msg ), "visible leafs:\n%3i leafs\ncurrent leaf %3i",
		r_stats.c_world_leafs, r_viewleaf - worldmodel->leafs );
		break;
	case 3:
		Q_snprintf( r_speeds_msg, sizeof( r_speeds_msg ), "DIP count %3i\nShader bind %3i",
		r_stats.num_flushes, r_stats.num_shader_binds );
		break;
	case 5:
		Q_snprintf( r_speeds_msg, sizeof( r_speeds_msg ), "%3i tempents\n%3i viewbeams\n%3i particles",
		r_stats.c_active_tents_count, r_stats.c_view_beams_count, r_stats.c_particle_count );
		break;
	case 6:
		Q_snprintf( r_speeds_msg, sizeof( r_speeds_msg ), "%3i mirrors\n%3i portals\n%3i screens\n%3i sky passes\n%3i shadow passes",
		r_stats.c_mirror_passes, r_stats.c_portal_passes, r_stats.c_screen_passes, r_stats.c_sky_passes, r_stats.c_shadow_passes );
		break;
	case 7:
		Q_snprintf( r_speeds_msg, sizeof( r_speeds_msg ), "%3i projected lights\n",
		r_stats.c_plights );
		break;
	case 9:
		Q_snprintf( r_speeds_msg, sizeof( r_speeds_msg ), "%3i drawed aurora particles\n%3i particle systems\n%3i flushes",
		r_stats.num_drawed_particles, r_stats.num_particle_systems, r_stats.num_flushes );
		break;
	}

	memset( &r_stats, 0, sizeof( r_stats ));
}

/*
===============
HUD_RenderFrame

A callback that replaces RenderFrame
engine function. Return 1 if you
override ALL the rendering and return 0
if we don't want draw from
the client (e.g. playersetup preview)
===============
*/
int HUD_RenderFrame( const ref_params_t *pparams, qboolean drawWorld )
{
	tr.fCustomRendering = false;

	if( !g_fRenderInitialized )
		return 0;

	r_speeds_msg[0] = '\0';

	// always the copy params in case we need have valid movevars
	RI.refdef = *pparams;
	tr.cached_refdef = pparams;

	// it's playersetup overview, ignore it	
	if( !drawWorld ) return 0;

	// we are in dev_overview mode, ignore it
	if( r_overview && r_overview->value )
	{
		tr.fResetVis = true;
		return 0;
	}

	// use engine renderer
	if( cv_renderer->value == 0 )
	{
		tr.fResetVis = true;
		return 0;
	}

	// set the worldmodel
	worldmodel = gEngfuncs.pfnGetModelByIndex( 1 );

	if( !worldmodel )
	{
		ALERT( at_error, "R_RenderView: NULL worldmodel\n" );
		tr.fResetVis = true;
		return 0;
	}

	tr.fCustomRendering = true;
	r_lastRefdef = *pparams;
	RI.params = RP_NONE;
	RI.farClip = 0;
	RI.clipFlags = 15;
	RI.drawWorld = true;
	RI.thirdPerson = cam_thirdperson;
	RI.pvsorigin = pparams->vieworg;

	// setup scissor
	RI.scissor[0] = pparams->viewport[0];
	RI.scissor[1] = glState.height - pparams->viewport[3] - pparams->viewport[1];
	RI.scissor[2] = pparams->viewport[2];
	RI.scissor[3] = pparams->viewport[3];

	// setup viewport
	RI.viewport[0] = pparams->viewport[0];
	RI.viewport[1] = glState.height - pparams->viewport[3] - pparams->viewport[1];
	RI.viewport[2] = pparams->viewport[2];
	RI.viewport[3] = pparams->viewport[3];

	if( r_finish->value ) pglFinish();

	// used for lighting scope etc
	lightinfo_t light;
	R_LightForPoint( pparams->vieworg, &light, false ); // get static lighting
	tr.ambientLight = light.ambient;

	// sort opaque entities by model type to avoid drawing model shadows under alpha-surfaces
	qsort( tr.solid_entities, tr.num_solid_entities, sizeof( cl_entity_t* ), (cmpfunc)R_SolidEntityCompare );

	ResetCounters ();
	UpdateLightmaps ();	// catch gamma changes
	R_RunViewmodelEvents();

	if( r_allow_mirrors->value )
	{
		// render mirrors
		if( R_FindMirrors( pparams ))
			R_DrawMirrors ();
	}

	// draw main view
	R_RenderScene( pparams );

	R_DrawViewModel();

	for( int i = 0; i < MAX_DLIGHTS; i++ )
	{
		DynamicLight *pl = &cl_dlights[i];

		if( pl->die < GET_CLIENT_TIME() || !pl->radius )
			continue;

		pl->frustumTest.DrawFrustumDebug();

	}

	RenderDOF( pparams );

	R_DrawHeadShield();

	R_BloomBlend( pparams );
	RenderUnderwaterBlur();
	RenderMonochrome();

	RenderHDRL( pparams );

	tr.realframecount++;

	HUD_PrintStats();

	return 1;
}

void R_ProcessModelData( model_t *mod, qboolean create )
{
	if( mod == gEngfuncs.pfnGetModelByIndex( 1 ))
		R_ProcessWorldData( mod, create );
	else R_ProcessStudioData( mod, create );
}

BOOL HUD_SpeedsMessage( char *out, size_t size )
{
	if( !cv_renderer || !cv_renderer->value )
		return false; // let the engine use built-in counters

	if( r_speeds->value <= 0 ) return false;
	if( !out || !size ) return false;

	Q_strncpy( out, r_speeds_msg, size );

	return true;
}

//
// Xash3D render interface
//
static render_interface_t gRenderInterface = 
{
	CL_RENDER_INTERFACE_VERSION,
	HUD_RenderFrame,
	HUD_BuildLightmaps,
	NULL,	// HUD_SetOrthoBounds
	NULL,	// R_StudioDecalShoot,
	R_CreateStudioDecalList,
	R_ClearStudioDecals,
	HUD_SpeedsMessage,
	NULL,	// HUD_DrawCubemapView
	R_ProcessModelData,
};

int HUD_GetRenderInterface( int version, render_api_t *renderfuncs, render_interface_t *callback )
{
	if ( !callback || !renderfuncs || version != CL_RENDER_INTERFACE_VERSION )
	{
		return FALSE;
	}

	size_t iImportSize = sizeof( render_interface_t );

	if( g_iXashEngineBuildNumber <= 2210 )
		iImportSize = 28;

	// copy new physics interface
	memcpy( &gRenderfuncs, renderfuncs, sizeof( render_api_t ));

	// fill engine callbacks
	memcpy( callback, &gRenderInterface, iImportSize );

	g_fRenderInitialized = TRUE;

	return TRUE;
}