/*
gl_backend.cpp - renderer backend routines
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

#include "r_studioint.h"
#include "ref_params.h"
#include "gl_local.h"
#include "gl_sprite.h"

/*
=============
R_GetSpriteTexture

=============
*/
int R_GetSpriteTexture( const model_t *m_pSpriteModel, int frame )
{
	if( !m_pSpriteModel || m_pSpriteModel->type != mod_sprite || !m_pSpriteModel->cache.data )
		return 0;

	return R_GetSpriteFrame( m_pSpriteModel, frame )->gl_texturenum;
}

/*
==============
GL_BindFBO
==============
*/
void GL_BindFBO( GLuint buffer )
{
	if( !GL_Support( R_FRAMEBUFFER_OBJECT ))
		return;

	if( glState.frameBuffer == buffer )
		return;

	pglBindFramebuffer( GL_FRAMEBUFFER_EXT, buffer );
	glState.frameBuffer = buffer;
}

/*
==============
GL_DisableAllTexGens
==============
*/
void GL_DisableAllTexGens( void )
{
	GL_TexGen( GL_S, 0 );
	GL_TexGen( GL_T, 0 );
	GL_TexGen( GL_R, 0 );
	GL_TexGen( GL_Q, 0 );
}

/*
=================
GL_ClipPlane
=================
*/
void GL_ClipPlane( bool enable )
{
	// if cliplane was not set - do nothing
	if( !FBitSet( RI.params, RP_CLIPPLANE ))
		return;

	if( enable )
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
	else
	{
		pglDisable( GL_CLIP_PLANE0 );
	}
}


/*
=================
GL_Cull
=================
*/
void GL_Cull( GLenum cull )
{
	if( !cull )
	{
		pglDisable( GL_CULL_FACE );
		glState.faceCull = 0;
		return;
	}

	pglEnable( GL_CULL_FACE );
	pglCullFace( cull );
	glState.faceCull = cull;
}

/*
=================
GL_FrontFace
=================
*/
void GL_FrontFace( GLenum front )
{
	pglFrontFace( front ? GL_CW : GL_CCW );
	glState.frontFace = front;
}

/*
=================
GL_LoadTexMatrix
=================
*/
void GL_LoadTexMatrix( const matrix4x4 &source )
{
	GLfloat	dest[16];

	source.CopyToArray( dest );
	GL_LoadTextureMatrix( dest );
}

/*
=================
GL_LoadMatrix
=================
*/
void GL_LoadMatrix( const matrix4x4 &source )
{
	GLfloat	dest[16];

	source.CopyToArray( dest );
	pglLoadMatrixf( dest );
}

//===============
// TexEnv states cache
//
// only for 2D textures.
// First and second passes are pretty messy - polygon soup with different states.
//===============
static void GL_SetTexEnv_Internal( int env )
{
	switch( env )
	{
	case ENVSTATE_OFF:
		GL_TextureTarget( GL_NONE );
		break;
	case ENVSTATE_REPLACE:
		GL_TextureTarget( GL_TEXTURE_2D );
		pglTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE );
		break;
	case ENVSTATE_MUL_CONST:
		GL_TextureTarget( GL_TEXTURE_2D );
		pglTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_ARB );
		pglTexEnvi( GL_TEXTURE_ENV, GL_COMBINE_RGB_ARB, GL_MODULATE );
		pglTexEnvi( GL_TEXTURE_ENV, GL_SOURCE0_RGB_ARB, GL_CONSTANT_ARB );
		pglTexEnvi( GL_TEXTURE_ENV, GL_SOURCE1_RGB_ARB, GL_TEXTURE );
		pglTexEnvi( GL_TEXTURE_ENV, GL_RGB_SCALE_ARB, 1 );
		break;
	case ENVSTATE_MUL_PREV_CONST:
		GL_TextureTarget( GL_TEXTURE_2D );
		pglTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_ARB );
		pglTexEnvi( GL_TEXTURE_ENV, GL_COMBINE_RGB_ARB, GL_MODULATE );
		pglTexEnvi( GL_TEXTURE_ENV, GL_SOURCE0_RGB_ARB, GL_PREVIOUS_ARB );
		pglTexEnvi( GL_TEXTURE_ENV, GL_SOURCE1_RGB_ARB, GL_PRIMARY_COLOR_ARB );
		pglTexEnvi( GL_TEXTURE_ENV, GL_RGB_SCALE_ARB, 1 );
		break;
	case ENVSTATE_MUL:
		GL_TextureTarget( GL_TEXTURE_2D );
		pglTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
		break;
	case ENVSTATE_MUL_X2:
		GL_TextureTarget( GL_TEXTURE_2D );
		pglTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_ARB );
		pglTexEnvi( GL_TEXTURE_ENV, GL_COMBINE_RGB_ARB, GL_MODULATE );
		pglTexEnvi( GL_TEXTURE_ENV, GL_SOURCE0_RGB_ARB, GL_PREVIOUS_ARB );
		pglTexEnvi( GL_TEXTURE_ENV, GL_SOURCE1_RGB_ARB, GL_TEXTURE );
		pglTexEnvi( GL_TEXTURE_ENV, GL_RGB_SCALE_ARB, 2 );
		break;
	case ENVSTATE_ADD:
		GL_TextureTarget( GL_TEXTURE_2D );
		pglTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_ARB );
		pglTexEnvi( GL_TEXTURE_ENV, GL_COMBINE_RGB_ARB, GL_ADD );
		pglTexEnvi( GL_TEXTURE_ENV, GL_SOURCE0_RGB_ARB, GL_PREVIOUS_ARB );
		pglTexEnvi( GL_TEXTURE_ENV, GL_SOURCE1_RGB_ARB, GL_TEXTURE );
		pglTexEnvi( GL_TEXTURE_ENV, GL_RGB_SCALE_ARB, 1 );
		break;
	case ENVSTATE_DOT:
		GL_TextureTarget( GL_TEXTURE_2D );
		pglTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_ARB );
		pglTexEnvi( GL_TEXTURE_ENV, GL_COMBINE_RGB_ARB, GL_DOT3_RGBA_ARB );
		pglTexEnvi( GL_TEXTURE_ENV, GL_SOURCE0_RGB_ARB, GL_PREVIOUS_ARB );
		pglTexEnvi( GL_TEXTURE_ENV, GL_SOURCE1_RGB_ARB, GL_TEXTURE );
		pglTexEnvi( GL_TEXTURE_ENV, GL_RGB_SCALE_ARB, 1 );
		break;
	case ENVSTATE_DOT_CONST:
		GL_TextureTarget( GL_TEXTURE_2D );
		pglTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_ARB );
		pglTexEnvi( GL_TEXTURE_ENV, GL_COMBINE_RGB_ARB, GL_DOT3_RGBA_ARB );
		pglTexEnvi( GL_TEXTURE_ENV, GL_SOURCE0_RGB_ARB, GL_CONSTANT_ARB );
		pglTexEnvi( GL_TEXTURE_ENV, GL_SOURCE1_RGB_ARB, GL_TEXTURE );
		pglTexEnvi( GL_TEXTURE_ENV, GL_RGB_SCALE_ARB, 1 );
		break;
	case ENVSTATE_PREVCOLOR_CURALPHA:
		GL_TextureTarget( GL_TEXTURE_2D );
		pglTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_ARB );
		pglTexEnvi( GL_TEXTURE_ENV, GL_COMBINE_RGB_ARB, GL_REPLACE );
		pglTexEnvi( GL_TEXTURE_ENV, GL_SOURCE0_RGB_ARB, GL_PREVIOUS_ARB );
		pglTexEnvi( GL_TEXTURE_ENV, GL_RGB_SCALE_ARB, 1 );
		pglTexEnvi( GL_TEXTURE_ENV, GL_COMBINE_ALPHA_ARB, GL_REPLACE );
		pglTexEnvi( GL_TEXTURE_ENV, GL_SOURCE0_ALPHA_ARB, GL_TEXTURE );
		pglTexEnvi( GL_TEXTURE_ENV, GL_OPERAND0_ALPHA_ARB, GL_SRC_ALPHA );
		pglTexEnvi( GL_TEXTURE_ENV, GL_ALPHA_SCALE, 1 );
		break;
	}
}
	
void GL_SetTexEnvs( int env0, int env1, int env2, int env3 )
{
	if( glState.envStates[0] != env0 )
	{
		GL_SelectTexture( GL_TEXTURE0 );
		GL_SetTexEnv_Internal( env0 );
		glState.envStates[0] = env0;
	}

	if( glState.envStates[1] != env1 )
	{
		GL_SelectTexture( GL_TEXTURE1 );
		GL_SetTexEnv_Internal( env1 );
		glState.envStates[1] = env1;
	}

	if( glConfig.max_texture_units < 3 )
		return;

	if( glState.envStates[2] != env2 )
	{
		GL_SelectTexture( GL_TEXTURE2 );
		GL_SetTexEnv_Internal( env2 );
		glState.envStates[2] = env2;
	}

	if( glConfig.max_texture_units < 4 )
		return;

	if( glState.envStates[3] != env3 )
	{
		GL_SelectTexture( GL_TEXTURE3 );
		GL_SetTexEnv_Internal( env3 );
		glState.envStates[3] = env3;
	}
}

void GL_SetTexPointer( int unitnum, int tc )
{
	if( unitnum >= glConfig.max_texture_units )
		return;

	if( glState.texpointer[unitnum] == tc )
		return;

	GL_SelectTexture( unitnum );
	
	switch( tc )
	{
	case TC_OFF:
		pglDisableClientState( GL_TEXTURE_COORD_ARRAY );
		break;
	case TC_TEXTURE:
		if( tr.use_vertex_array == 2 )
			pglTexCoordPointer( 2, GL_FLOAT, sizeof( BrushVertex ), OFFSET( BrushVertex, texcoord ));
		else pglTexCoordPointer( 2, GL_FLOAT, sizeof( BrushVertex ), &tr.vbo_buffer_data[0].texcoord );
		pglEnableClientState( GL_TEXTURE_COORD_ARRAY );
		break;
	case TC_LIGHTMAP:
		if( tr.use_vertex_array == 2 )
			pglTexCoordPointer( 2, GL_FLOAT, sizeof( BrushVertex ), OFFSET( BrushVertex, lightmaptexcoord ));
		else pglTexCoordPointer( 2, GL_FLOAT, sizeof( BrushVertex ), &tr.vbo_buffer_data[0].lightmaptexcoord );
		pglEnableClientState( GL_TEXTURE_COORD_ARRAY );
		break;
	case TC_VERTEX_POSITION:
		if( tr.use_vertex_array == 2 )
			pglTexCoordPointer( 3, GL_FLOAT, sizeof( BrushVertex ), OFFSET( BrushVertex, pos ));
		else pglTexCoordPointer( 3, GL_FLOAT, sizeof( BrushVertex ), &tr.vbo_buffer_data[0].pos );
		pglEnableClientState( GL_TEXTURE_COORD_ARRAY );		
		break;
	}

	glState.texpointer[unitnum] = tc;
}

//===============
// Resets all caches
//===============
void ResetCache( void )
{
	for( int i = 0; i < MAX_TEXTURE_UNITS; i++ )
	{
		glState.envStates[i] = ENVSTATE_NOSTATE;
		glState.texpointer[i] = TC_NOSTATE;
	}
}

void ResetRenderState()
{
	// because we don't know how many units was used
	GL_SelectTexture( glConfig.max_texture_units - 1 ); // force to cleanup all the units

	for( int i = RENDER_GET_PARM( PARM_ACTIVE_TMU, 0 ); i > -1; i-- )
	{
		pglTexEnvi( GL_TEXTURE_ENV, GL_RGB_SCALE_ARB, 1.0f );
		GL_TextureTarget( GL_NONE );
		GL_LoadIdentityTexMatrix();
		GL_DisableAllTexGens();
		GL_SelectTexture( i - 1 );
	}

	GL_SelectTexture( GL_TEXTURE0 );
	GL_TextureTarget( GL_TEXTURE_2D );

	pglDisable( GL_BLEND );
	pglDepthMask( !glState.drawTrans );
	pglDepthFunc( GL_LEQUAL );
}

/*
================
R_BeginDrawProjection

Setup texture matrix for light texture
================
*/
void R_BeginDrawProjection( const DynamicLight *pl, bool decalPass )
{
	GLfloat	genVector[4][4];

	RI.currentlight = pl;
	pglEnable( GL_BLEND );

	if( glState.drawTrans )
	{
		// particle lighting
		pglDepthFunc( GL_LEQUAL );
		pglBlendFunc( GL_SRC_COLOR, GL_SRC_ALPHA );
		pglColor4f( pl->color.x*0.3f, pl->color.y*0.3f, pl->color.z*0.3f, 1.0f );
	}
	else
	{
		pglBlendFunc( GL_ONE, GL_ONE );
		pglColor4f( pl->color.x, pl->color.y, pl->color.z, 1.0f );
		if( decalPass ) pglDepthFunc( GL_LEQUAL );
		else pglDepthFunc( GL_EQUAL );
	}

	if( pl->spotlightTexture )
		GL_Bind( GL_TEXTURE0, pl->spotlightTexture );
	else GL_Bind( GL_TEXTURE0, tr.dlightCubeTexture );
	pglTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );

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

	if( tr.modelviewIdentity )
		GL_LoadTexMatrix( pl->textureMatrix );
	else GL_LoadTexMatrix( pl->textureMatrix2 );

	glState.drawProjection = true;

	// setup attenuation texture
	if( pl->spotlightTexture )
		GL_Bind( GL_TEXTURE1, tr.attenuation_1d );
	else GL_Bind( GL_TEXTURE1, tr.atten_point_3d ); 
	pglTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );

	if( !pl->spotlightTexture )
	{
		float r = 1.0f / (pl->radius * 2);
		Vector origin;

		if( !tr.modelviewIdentity )
		{
			// rotate attenuation texture into local space
			if( RI.currententity->angles != g_vecZero )
				origin = RI.objectMatrix.VectorITransform( pl->origin );
			else origin = pl->origin - RI.currententity->origin;
		}
		else origin = pl->origin;

		GLfloat planeS[] = { r, 0, 0, -origin[0] * r + 0.5 };
		GLfloat planeT[] = { 0, r, 0, -origin[1] * r + 0.5 };
		GLfloat planeR[] = { 0, 0, r, -origin[2] * r + 0.5 };

		GL_TexGen( GL_S, GL_EYE_LINEAR );
		GL_TexGen( GL_T, GL_EYE_LINEAR );
		GL_TexGen( GL_R, GL_EYE_LINEAR );

		pglTexGenfv( GL_S, GL_EYE_PLANE, planeS );
		pglTexGenfv( GL_T, GL_EYE_PLANE, planeT );
		pglTexGenfv( GL_R, GL_EYE_PLANE, planeR );
	}
	else
	{
		GLfloat	genPlaneS[4];
		Vector	origin, normal;

		if( !tr.modelviewIdentity )
		{
			if( RI.currententity->angles != g_vecZero )
			{
				// rotate attenuation texture into local space
				normal = RI.objectMatrix.VectorIRotate( pl->frustum[5].normal );
				origin = RI.objectMatrix.VectorITransform( pl->origin );
			}
			else
			{
				normal = pl->frustum[5].normal;
				origin = pl->origin - RI.currententity->origin;
			}
		}
		else
		{
			normal = pl->frustum[5].normal;
			origin = pl->origin;
		}

		genPlaneS[0] = normal[0] / pl->radius;
		genPlaneS[1] = normal[1] / pl->radius;
		genPlaneS[2] = normal[2] / pl->radius;
		genPlaneS[3] = -(DotProduct( normal, origin ) / pl->radius);

		GL_TexGen( GL_S, GL_OBJECT_LINEAR );
		pglTexGenfv( GL_S, GL_OBJECT_PLANE, genPlaneS );
	}

	GL_LoadIdentityTexMatrix();

	if( decalPass )
	{
		if( cg.decal0_shader && ( r_shadows->value <= 0.0f || !pl->spotlightTexture ))
		{
			pglEnable( GL_FRAGMENT_PROGRAM_ARB );
			pglBindProgramARB( GL_FRAGMENT_PROGRAM_ARB, (pl->spotlightTexture) ? cg.decal0_shader : cg.decal3_shader );
		}
		else if( r_shadows->value == 1.0f && cg.decal1_shader )
		{
			pglEnable( GL_FRAGMENT_PROGRAM_ARB );
			pglBindProgramARB( GL_FRAGMENT_PROGRAM_ARB, cg.decal1_shader );
		} 
		else if( r_shadows->value > 1.0f && cg.decal2_shader )
		{
			pglEnable( GL_FRAGMENT_PROGRAM_ARB );
			pglBindProgramARB( GL_FRAGMENT_PROGRAM_ARB, cg.decal2_shader );
		}
	}
	else if( r_shadows->value > 1.0f && cg.shadow_shader0 && pl->spotlightTexture )
	{
		pglEnable( GL_FRAGMENT_PROGRAM_ARB );
		pglBindProgramARB( GL_FRAGMENT_PROGRAM_ARB, cg.shadow_shader0 );
	}

	// TODO: allow shadows for pointlights
	if( r_shadows->value <= 0.0f || !pl->spotlightTexture )
		return;		

	GL_Bind( GL_TEXTURE2, pl->shadowTexture );
	pglTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );

	GL_TexGen( GL_S, GL_EYE_LINEAR );
	GL_TexGen( GL_T, GL_EYE_LINEAR );
	GL_TexGen( GL_R, GL_EYE_LINEAR );
	GL_TexGen( GL_Q, GL_EYE_LINEAR );

	pglTexGenfv( GL_S, GL_EYE_PLANE, genVector[0] );
	pglTexGenfv( GL_T, GL_EYE_PLANE, genVector[1] );
	pglTexGenfv( GL_R, GL_EYE_PLANE, genVector[2] );
	pglTexGenfv( GL_Q, GL_EYE_PLANE, genVector[3] );

	if( tr.modelviewIdentity )
		GL_LoadTexMatrix( pl->shadowMatrix );
	else GL_LoadTexMatrix( pl->shadowMatrix2 );
}

/*
================
R_EndDrawProjection

Restore identity texmatrix
================
*/
void R_EndDrawProjection( void )
{
	if( GL_Support( R_FRAGMENT_PROGRAM_EXT ))
		pglDisable( GL_FRAGMENT_PROGRAM_ARB );

	GL_CleanUpTextureUnits( 0 );

	pglMatrixMode( GL_MODELVIEW );
	glState.drawProjection = false;
	pglColor4ub( 255, 255, 255, 255 );

	pglDepthFunc( GL_LEQUAL );
	pglDisable( GL_BLEND );
	RI.currentlight = NULL;
}

void R_DrawAbsBBox( const Vector &absmin, const Vector &absmax )
{
	Vector	bbox[8];
	int	i;

	R_LoadIdentity();

	// compute a full bounding box
	for( i = 0; i < 8; i++ )
	{
  		bbox[i][0] = ( i & 1 ) ? absmin[0] : absmax[0];
  		bbox[i][1] = ( i & 2 ) ? absmin[1] : absmax[1];
  		bbox[i][2] = ( i & 4 ) ? absmin[2] : absmax[2];
	}

	pglDisable( GL_TEXTURE_2D );
	pglDisable( GL_DEPTH_TEST );

	pglColor4f( 0.0f, 1.0f, 0.0f, 1.0f );	// red bboxes for studiomodels
	pglBegin( GL_LINES );

	for( i = 0; i < 2; i += 1 )
	{
		pglVertex3fv( bbox[i+0] );
		pglVertex3fv( bbox[i+2] );
		pglVertex3fv( bbox[i+4] );
		pglVertex3fv( bbox[i+6] );
		pglVertex3fv( bbox[i+0] );
		pglVertex3fv( bbox[i+4] );
		pglVertex3fv( bbox[i+2] );
		pglVertex3fv( bbox[i+6] );
		pglVertex3fv( bbox[i*2+0] );
		pglVertex3fv( bbox[i*2+1] );
		pglVertex3fv( bbox[i*2+4] );
		pglVertex3fv( bbox[i*2+5] );
	}
	pglEnd();

	pglEnable( GL_TEXTURE_2D );
	pglEnable( GL_DEPTH_TEST );
}