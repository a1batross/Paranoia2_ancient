//
// written by BUzer for HL: Paranoia modification
//
//		2005-2006

#include "hud.h"
#include "cl_util.h"
#include "const.h"
#include "com_model.h"
#include "pm_movevars.h"
#include "studio.h"
#include "r_studioint.h"
#include "ref_params.h"
#include "pm_defs.h"
#include "r_efx.h"
#include "event_api.h"
#include "pm_defs.h"
#include "pmtrace.h"
#include "gl_local.h"
#include <mathlib.h>

#include "gl_studio.h"

DynamicLight cl_dlights[MAX_DLIGHTS];
static glpoly_t *chain; // for two-pass light rendering
static qboolean twopass = FALSE;

/*
================
R_GetLightVectors

Get light vectors for entity
================
*/
void R_GetLightVectors( cl_entity_t *pEnt, Vector &origin, Vector &angles )
{
	// fill default case
	origin = pEnt->origin;
	angles = pEnt->angles; 

	// try to grab position from attachment
	if( pEnt->curstate.aiment > 0 && pEnt->curstate.movetype == MOVETYPE_FOLLOW )
	{
		cl_entity_t *pParent = GET_ENTITY( pEnt->curstate.aiment );
		studiohdr_t *pStudioHeader = (studiohdr_t *)IEngineStudio.Mod_Extradata( pParent->model );

		if( pParent && pParent->model && pStudioHeader != NULL )
		{
			// make sure what model really has attachements
			if( pEnt->curstate.body > 0 && ( pStudioHeader && pStudioHeader->numattachments > 0 ))
			{
				int num = bound( 1, pEnt->curstate.body, MAXSTUDIOATTACHMENTS );
				origin = pParent->ph[num-1].origin;
				VectorAngles( pParent->ph[num-1].angles, angles );
				angles[PITCH] = -angles[PITCH]; // stupid quake bug
			}
			else if( pParent->curstate.movetype == MOVETYPE_STEP )
			{
				float f;
                    
				// don't do it if the goalstarttime hasn't updated in a while.
				// NOTE:  Because we need to interpolate multiplayer characters, the interpolation time limit
				//  was increased to 1.0 s., which is 2x the max lag we are accounting for.
				if(( GET_CLIENT_TIME() < pParent->curstate.animtime + 1.0f ) && ( pParent->curstate.animtime != pParent->latched.prevanimtime ))
				{
					f = (GET_CLIENT_TIME() - pParent->curstate.animtime) / (pParent->curstate.animtime - pParent->latched.prevanimtime);
				}

				if( !( pParent->curstate.effects & EF_NOINTERP ))
				{
					// ugly hack to interpolate angle, position.
					// current is reached 0.1 seconds after being set
					f = f - 1.0f;
				}
				else
				{
					f = 0.0f;
				}

				// start lerping from
				origin = pParent->origin;
				angles = pParent->angles;

				InterpolateOrigin( pParent->latched.prevorigin, pParent->origin, origin, f, true );
				InterpolateAngles( pParent->latched.prevangles, pParent->angles, angles, f, true );

				// add the eye position for monster
				if( pParent->curstate.eflags & EFLAG_SLERP )
					origin += pStudioHeader->eyeposition;
			} 
			else
			{
				origin = pParent->origin;
				angles = pParent->angles;
			}
		}
	}
	// all other parent types will be attached on the server
}

/*
================
R_SetupLightProjection

General setup light projections.
Calling only once per frame
================
*/
void R_SetupLightProjection( DynamicLight *pl, const Vector &origin, const Vector &angles, float radius, float fov, int texture )
{
	pl->spotlightTexture = texture;

	// update the frustum only if needs
	if( pl->origin != origin || pl->angles != angles || pl->fov != fov || pl->radius != radius )
	{
		GLdouble	xMin, xMax, yMin, yMax, zNear, zFar;
		GLfloat	size_x, size_y;
		GLfloat	fov_x, fov_y;

		zNear = 0.1f;
		zFar = radius * 1.5f; // distance

		pl->origin = origin;
		pl->angles = angles;
		pl->radius = radius;
		pl->fov = fov;

		if( pl->key == SUNLIGHT_KEY )
		{
			Vector	size = worldmodel->maxs - worldmodel->mins;
			bool	rotated = ( size[1] <= size[0] ) ? true : false;
			float	mapAspect = size[!rotated] / size[rotated];
			float	screenAspect = (float)glState.width / (float)glState.height;
			float	zoomAspect = max( mapAspect, screenAspect );
			float	zoom = (( 8192.0f / size[rotated] ) / zoomAspect);
			movevars_t *mv = RI.refdef.movevars;

			if( r_test->value )
				zoom *= r_test->value;

			size_x = fabs( 8192.0f / zoom );
			size_y = fabs( 8192.0f / (zoom * screenAspect ));

			pl->projectionMatrix.CreateOrtho( -(size_x / 2), (size_x / 2), -(size_y / 2), (size_y / 2), 4.0f, mv->zmax );
			pl->modelviewMatrix.CreateModelview(); // init quake world orientation
		}
		else if( pl->spotlightTexture )
		{
			xMax = yMax = zNear * tan( fov * M_PI / 360.0 );
			xMin = yMin = -yMax;
			fov_x = fov_y = fov;

			fov = zNear * tan( fov * M_PI / 360.0 );
			pl->projectionMatrix.CreateProjection( xMax, xMin, yMax, yMin, zNear, zFar );
			pl->modelviewMatrix.CreateModelview(); // init quake world orientation
		}
		else
		{
			// 'quake oriented' cubemaps probably starts from Tenebrae
			// may be it was an optimization?
			pl->modelviewMatrix.Identity();
		}

		pl->modelviewMatrix.ConcatRotate( -pl->angles.z, 1, 0, 0 );
		pl->modelviewMatrix.ConcatRotate( -pl->angles.x, 0, 1, 0 );
		pl->modelviewMatrix.ConcatRotate( -pl->angles.y, 0, 0, 1 );
		pl->modelviewMatrix.ConcatTranslate( -pl->origin.x, -pl->origin.y, -pl->origin.z );

		matrix4x4 projectionView, m1, s1;

		m1.CreateTranslate( 0.5f, 0.5f, 0.5f );
		s1.CreateTranslate( 0.5f, 0.5f, 0.5f );

		if( pl->spotlightTexture )
		{
			projectionView = pl->projectionMatrix.Concat( pl->modelviewMatrix );
			m1.ConcatScale( 0.5f, -0.5f, 0.5f );
		}
		else
		{
			projectionView = pl->modelviewMatrix; // cubemaps not used projection matrix
			m1.ConcatScale( 0.5f, 0.5f, 0.5f );
		}

		s1.ConcatScale( 0.5f, 0.5f, 0.5f );

		pl->textureMatrix = m1.Concat( projectionView ); // for world, sprites and studiomodels
		pl->shadowMatrix = s1.Concat( projectionView );

		ClearBounds( pl->absmin, pl->absmax );

		if( pl->key == SUNLIGHT_KEY )
		{
			Vector vforward, vright, vup;
			AngleVectors( pl->angles, vforward, vright, vup );

			// setup the near and far planes.
			float orgOffset = DotProduct( pl->origin, vforward );

			pl->frustum[4].normal = -vforward;
			pl->frustum[4].dist = -8192.0f - orgOffset;

			pl->frustum[5].normal = vforward;
			pl->frustum[5].dist = -8192.0f + orgOffset;

			// left and right planes...
			orgOffset = DotProduct( pl->origin, vright );
			pl->frustum[0].normal = vright;
			pl->frustum[0].dist = -(size_x / 2) + orgOffset;

			pl->frustum[1].normal = -vright;
			pl->frustum[1].dist = -(size_x / 2) - orgOffset;

			// top and buttom planes...
			orgOffset = DotProduct( pl->origin, vup );
			pl->frustum[3].normal = vup;
			pl->frustum[3].dist = -(size_y / 2) + orgOffset;

			pl->frustum[2].normal = -vup;
			pl->frustum[2].dist = -(size_y / 2) - orgOffset;

			for( int i = 0; i < 6; i++ )
			{
				pl->frustum[i].type = PLANE_NONAXIAL;
				pl->frustum[i].signbits = SignbitsForPlane( pl->frustum[i].normal );
			}

			// setup base clipping
			pl->clipflags = 63;

			float xLeft = (size_x * 0.5f);
			float xRight = -(size_x * 0.5f);
			float yBottom = (size_y * 0.5f);
			float yTop = -(size_y * 0.5f);
   			zNear = Vector( worldmodel->maxs - worldmodel->mins ).Length() * -0.75f;
   			zFar = Vector( worldmodel->maxs - worldmodel->mins ).Length() * 0.75f;
			
			pl->frustumTest.InitOrthogonal( matrix3x4( pl->origin, pl->angles ), xLeft, xRight, yBottom, yTop, zNear, zFar );
		}
		else if( pl->spotlightTexture )
		{
			Vector vforward, vright, vup;
			AngleVectors( pl->angles, vforward, vright, vup );

			Vector farPoint = pl->origin + vforward * zFar;

			// rotate vforward right by FOV_X/2 degrees
			RotatePointAroundVector( pl->frustum[0].normal, vup, vforward, -(90 - fov_x / 2));
			// rotate vforward left by FOV_X/2 degrees
			RotatePointAroundVector( pl->frustum[1].normal, vup, vforward, 90 - fov_x / 2 );
			// rotate vforward up by FOV_Y/2 degrees
			RotatePointAroundVector( pl->frustum[2].normal, vright, vforward, 90 - fov_y / 2 );
			// rotate vforward down by FOV_Y/2 degrees
			RotatePointAroundVector( pl->frustum[3].normal, vright, vforward, -(90 - fov_y / 2));

			for( int i = 0; i < 4; i++ )
			{
				pl->frustum[i].type = PLANE_NONAXIAL;
				pl->frustum[i].dist = DotProduct( pl->origin, pl->frustum[i].normal );
				pl->frustum[i].signbits = SignbitsForPlane( pl->frustum[i].normal );
			}

			pl->frustum[4].normal = -vforward;
			pl->frustum[4].type = PLANE_NONAXIAL;
			pl->frustum[4].dist = DotProduct( farPoint, pl->frustum[4].normal );
			pl->frustum[4].signbits = SignbitsForPlane( pl->frustum[4].normal );

			// store vector forward to avoid compute it again
			pl->frustum[5].normal = vforward;

			// setup base clipping
			pl->clipflags = 15;

			pl->frustumTest.InitProjection( matrix3x4( pl->origin, pl->angles ), 0.0f, zFar, pl->fov, pl->fov );
		}
		else
		{
			for( int i = 0; i < 6; i++ )
			{
				pl->frustum[i].type = i>>1;
				pl->frustum[i].normal[i>>1] = (i & 1) ? 1.0f : -1.0f;
				pl->frustum[i].signbits = SignbitsForPlane( pl->frustum[i].normal );
				pl->frustum[i].dist = DotProduct( pl->origin, pl->frustum[i].normal ) - pl->radius;
			}

			// setup base clipping
			pl->clipflags = 63;

			pl->frustumTest.InitBoxFrustum( pl->origin, pl->radius );
		}
	}
}

/*
================
R_MergeLightProjection

merge projection for bmodels
================
*/
void R_MergeLightProjection( DynamicLight *pl )
{
	matrix4x4 modelviewMatrix = pl->modelviewMatrix.ConcatTransforms( RI.objectMatrix );
	matrix4x4 projectionView, m1, s1;

	m1.CreateTranslate( 0.5f, 0.5f, 0.5f );
	s1.CreateTranslate( 0.5f, 0.5f, 0.5f );
	s1.ConcatScale( 0.5f, 0.5f, 0.5f );

	if( pl->spotlightTexture )
	{
		projectionView = pl->projectionMatrix.Concat( modelviewMatrix );
		m1.ConcatScale( 0.5f, -0.5f, 0.5f );
	}
	else
	{
		projectionView = modelviewMatrix; // cubemaps not used projection matrix
		m1.ConcatScale( 0.5f, 0.5f, 0.5f );
	}

	pl->textureMatrix2 = m1.Concat( projectionView );
	pl->shadowMatrix2 = s1.Concat( projectionView );
}

void SetupProjection( const DynamicLight *pl )
{
	// enable automatic texture coordinates generation
	GLfloat planeS[] = { 1.0f, 0.0f, 0.0f, 0.0f };
	GLfloat planeT[] = { 0.0f, 1.0f, 0.0f, 0.0f };
	GLfloat planeR[] = { 0.0f, 0.0f, 1.0f, 0.0f };
	GLfloat planeQ[] = { 0.0f, 0.0f, 0.0f, 1.0f };

	GL_TexGen( GL_S, GL_EYE_LINEAR );
	GL_TexGen( GL_T, GL_EYE_LINEAR );
	GL_TexGen( GL_R, GL_EYE_LINEAR );
	GL_TexGen( GL_Q, GL_EYE_LINEAR );

	pglTexGenfv( GL_S, GL_EYE_PLANE, planeS );
	pglTexGenfv( GL_T, GL_EYE_PLANE, planeT );
	pglTexGenfv( GL_R, GL_EYE_PLANE, planeR );
	pglTexGenfv( GL_Q, GL_EYE_PLANE, planeQ );

	// load texture projection matrix
	if( tr.modelviewIdentity )
		GL_LoadTexMatrix( pl->textureMatrix );
	else GL_LoadTexMatrix( pl->textureMatrix2 );

	pglMatrixMode( GL_MODELVIEW );
}

void DisableProjection( void )
{
	GL_LoadIdentityTexMatrix();
	pglMatrixMode( GL_MODELVIEW );
	GL_DisableAllTexGens();
}


//=========================
// SetupAttenuationSpotlight
//
// sets 1d attenuation texture coordinate generation for spotlights
// (in model space)
//=========================
void SetupAttenuationSpotlight( const DynamicLight *pl )
{
	GLfloat planeS[4];
	Vector origin, normal;

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

	tr.modelorg = origin;

	planeS[0] = normal[0] / pl->radius;
	planeS[1] = normal[1] / pl->radius;
	planeS[2] = normal[2] / pl->radius;
	planeS[3] = -(DotProduct( normal, origin ) / pl->radius);

	GL_TexGen( GL_S, GL_EYE_LINEAR );
	pglTexGenfv( GL_S, GL_EYE_PLANE, planeS );

	GL_LoadIdentityTexMatrix();
}

void DisableAttenuationSpotlight( void )
{
	GL_DisableAllTexGens();
}


//=========================
// SetupAttenuation3D
//
// sets 3d attenuation texture coordinate generation for point lights
// (in model space)
//=========================
void SetupAttenuation3D( const DynamicLight *pl )
{
	float r = 1.0f / (pl->radius * 2.0f);
	GLfloat planeS[] = { r, 0, 0, -tr.modelorg[0] * r + 0.5f };
	GLfloat planeT[] = { 0, r, 0, -tr.modelorg[1] * r + 0.5f };
	GLfloat planeR[] = { 0, 0, r, -tr.modelorg[2] * r + 0.5f };

	GL_TexGen( GL_S, GL_EYE_LINEAR );
	GL_TexGen( GL_T, GL_EYE_LINEAR );
	GL_TexGen( GL_R, GL_EYE_LINEAR );

	pglTexGenfv( GL_S, GL_EYE_PLANE, planeS );
	pglTexGenfv( GL_T, GL_EYE_PLANE, planeT );
	pglTexGenfv( GL_R, GL_EYE_PLANE, planeR );
}

//=========================
// SetupAttenuation2D, SetupAttenuation1D
//
// used in pair, setups attenuations on cards withoud 3d textures support
//=========================
void SetupAttenuation2D( const DynamicLight *pl )
{
	float r = 1.0f / (pl->radius * 2.0f);
	GLfloat planeS[] = { r, 0, 0, -tr.modelorg[0] * r + 0.5f };
	GLfloat planeT[] = { 0, r, 0, -tr.modelorg[1] * r + 0.5f };

	GL_TexGen( GL_S, GL_EYE_LINEAR );
	GL_TexGen( GL_T, GL_EYE_LINEAR );

	pglTexGenfv( GL_S, GL_EYE_PLANE, planeS );
	pglTexGenfv( GL_T, GL_EYE_PLANE, planeT );
}

//=========================
// SetupAttenuation2D, SetupAttenuation1D
//
// used in pair, setups attenuations on cards withoud 3d textures support
//=========================
void SetupAttenuation1D( const DynamicLight *pl )
{
	float r = 1.0f / (pl->radius * 2.0f);
	GLfloat planeS[] = { 0, 0, r, -tr.modelorg[2] * r + 0.5f };

	GL_TexGen( GL_S, GL_EYE_LINEAR );

	pglTexGenfv( GL_S, GL_EYE_PLANE, planeS );
}

//============================
// SetupSpotLight, SetupPointLight
//
// Prepares texture stages and binds all appropriate textures
//============================
qboolean SetupSpotLight( const DynamicLight *pl )
{
	if( !pl->spotlightTexture ) // not a spotlight?
		return FALSE;

	if( r_shadows->value <= 0.0f && glConfig.max_texture_units >= 4 )
	{
		// setup blending
		pglEnable( GL_BLEND );
		pglBlendFunc( GL_ONE, GL_ONE );
		pglDepthMask( GL_FALSE );

		// setup texture stages
		GL_SetTexEnvs( ENVSTATE_REPLACE, ENVSTATE_DOT, ENVSTATE_MUL, ENVSTATE_MUL );

		GL_SetTexPointer( 0, TC_VERTEX_POSITION );
		GL_SetTexPointer( 1, TC_TEXTURE );
		GL_SetTexPointer( 2, TC_OFF ); // auto generated
		GL_SetTexPointer( 3, TC_OFF ); // auto generated

		// normalization cube map
		GL_Bind( GL_TEXTURE0, tr.normalizeCubemap );

		// spotlight texture
		GL_Bind( GL_TEXTURE2, pl->spotlightTexture );
		SetupProjection( pl );

		// attenuation
		GL_Bind( GL_TEXTURE3, tr.attenuation_1d );
		SetupAttenuationSpotlight( pl );
		twopass = FALSE;

		return TRUE;
	}
	
	// setup first pass rendering - store dot in alpha
	chain = NULL;
	pglEnable( GL_BLEND );
	pglBlendFunc( GL_ONE, GL_ZERO );
	pglDepthMask( GL_FALSE );
	pglColorMask( GL_FALSE, GL_FALSE, GL_FALSE, GL_TRUE ); // render only alpha

	GL_SetTexEnvs( ENVSTATE_REPLACE, ENVSTATE_DOT );
	GL_SetTexPointer( 0, TC_VERTEX_POSITION );
	GL_SetTexPointer( 1, TC_TEXTURE );

	// normalization cube map
	GL_Bind( GL_TEXTURE0, tr.normalizeCubemap );
	twopass = TRUE;

	return TRUE;
}

//============================
// FinishSpotLight, FinishPointLight
//
// renders second pass if requed and clears all used gl states
//============================
void FinishSpotLight( const DynamicLight *pl )
{
	GL_SelectTexture( GL_TEXTURE0 );
	GL_TextureTarget( GL_NONE );
	GL_LoadIdentityTexMatrix();
	pglMatrixMode( GL_MODELVIEW );

	if( twopass )
	{
		GL_SetTexEnvs( ENVSTATE_REPLACE, ENVSTATE_MUL );
		pglColor4ub( 255, 255, 255, 255 );

		// attenuation
		GL_Bind( GL_TEXTURE0, tr.attenuation_1d );
		SetupAttenuationSpotlight( pl );

		// spotlight texture
		GL_Bind( GL_TEXTURE1, pl->spotlightTexture );
		SetupProjection( pl );		

		pglBlendFunc( GL_DST_ALPHA, GL_ONE ); // alpha additive
		pglColorMask( GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE ); // render RGB

		GL_SetTexPointer( 0, TC_OFF );
		GL_SetTexPointer( 1, TC_OFF );

		if( r_shadows->value > 0.0f )
		{
			if( r_shadows->value > 1.0f && cg.shadow_shader0 && pl->spotlightTexture )
			{
				pglEnable( GL_FRAGMENT_PROGRAM_ARB );
				pglBindProgramARB( GL_FRAGMENT_PROGRAM_ARB, cg.shadow_shader1 );
			}

			GLfloat planeS[] = { 1.0f, 0.0f, 0.0f, 0.0f };
			GLfloat planeT[] = { 0.0f, 1.0f, 0.0f, 0.0f };
			GLfloat planeR[] = { 0.0f, 0.0f, 1.0f, 0.0f };
			GLfloat planeQ[] = { 0.0f, 0.0f, 0.0f, 1.0f };

			GL_Bind( GL_TEXTURE2, pl->shadowTexture );
			pglTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );

			GL_TexGen( GL_S, GL_EYE_LINEAR );
			GL_TexGen( GL_T, GL_EYE_LINEAR );
			GL_TexGen( GL_R, GL_EYE_LINEAR );
			GL_TexGen( GL_Q, GL_EYE_LINEAR );

			pglTexGenfv( GL_S, GL_EYE_PLANE, planeS );
			pglTexGenfv( GL_T, GL_EYE_PLANE, planeT );
			pglTexGenfv( GL_R, GL_EYE_PLANE, planeR );
			pglTexGenfv( GL_Q, GL_EYE_PLANE, planeQ );

			if( tr.modelviewIdentity )
				GL_LoadTexMatrix( pl->shadowMatrix );
			else GL_LoadTexMatrix( pl->shadowMatrix2 );

			GL_SetTexPointer( 2, TC_OFF );
                    }

		glpoly_t *p = chain;

		while( p )
		{
			DrawPolyFromArray( p );
			p = p->next;
		}

		if( r_shadows->value > 1.0f && GL_Support( R_FRAGMENT_PROGRAM_EXT ))
			pglDisable( GL_FRAGMENT_PROGRAM_ARB );

		GL_CleanUpTextureUnits( 0 );
		pglMatrixMode( GL_MODELVIEW );
	}
	else
	{
		GL_SelectTexture( GL_TEXTURE2 );
		DisableProjection();

		GL_SelectTexture( GL_TEXTURE3 );
		GL_TextureTarget( GL_TEXTURE_2D );
		GL_DisableAllTexGens();
	}
}

int SetupPointLight( const DynamicLight *pl )
{
	pglEnable( GL_BLEND );
	pglDepthMask( GL_FALSE );

	// setup modelorigin
	if( !tr.modelviewIdentity )
	{
		// rotate attenuation texture into local space
		if( RI.currententity->angles != g_vecZero )
			tr.modelorg = RI.objectMatrix.VectorITransform( pl->origin );
		else tr.modelorg = pl->origin - RI.currententity->origin;
	}
	else tr.modelorg = pl->origin;

	if( !cv_dyntwopass->value && tr.atten_point_3d && glConfig.max_texture_units >= 4 )
	{
		Vector color;

		color[0] = ApplyGamma( pl->color[0] ) / 2.0f;
		color[1] = ApplyGamma( pl->color[1] ) / 2.0f;
		color[2] = ApplyGamma( pl->color[2] ) / 2.0f;

		// draw light in one pass
		pglBlendFunc( GL_ONE, GL_ONE );
	
		// setup texture stages
		GL_SetTexEnvs( ENVSTATE_REPLACE, ENVSTATE_DOT, ENVSTATE_MUL, ENVSTATE_MUL_PREV_CONST );
		pglColor3f( color[0], color[1], color[2] );

		GL_SetTexPointer( 0, TC_VERTEX_POSITION );
		GL_SetTexPointer( 1, TC_TEXTURE );
		GL_SetTexPointer( 2, TC_OFF ); // auto generated
		GL_SetTexPointer( 3, TC_OFF ); // not used

		// normalization cube map
		GL_Bind( GL_TEXTURE0, tr.normalizeCubemap );

		// 3d attenuation texture
		GL_Bind( GL_TEXTURE2, tr.atten_point_3d );
		SetupAttenuation3D( pl );

		// light color (bind dummy texture)
		GL_Bind( GL_TEXTURE3, tr.normalmapTexture );
		twopass = FALSE;

		return TRUE;
	}

	// setup first pass rendering - store dot in alpha.
	chain = NULL;
	pglBlendFunc( GL_ONE, GL_ZERO );
	pglColorMask( GL_FALSE, GL_FALSE, GL_FALSE, GL_TRUE ); // render only alpha

	GL_SetTexEnvs( ENVSTATE_REPLACE, ENVSTATE_DOT );
	GL_SetTexPointer( 0, TC_VERTEX_POSITION );
	GL_SetTexPointer( 1, TC_TEXTURE );

	// normalization cube map
	GL_Bind( GL_TEXTURE0, tr.normalizeCubemap );
	twopass = TRUE;

	return TRUE;
}

void FinishPointLight( const DynamicLight *pl )
{
	GL_SelectTexture( GL_TEXTURE0 );
	GL_TextureTarget( GL_NONE );
	GL_LoadIdentityTexMatrix();
	pglMatrixMode( GL_MODELVIEW );

	if( twopass )
	{
		Vector color;

		// add attenuation and color
		color[0] = ApplyGamma( pl->color[0] ) / 2.0f;
		color[1] = ApplyGamma( pl->color[1] ) / 2.0f;
		color[2] = ApplyGamma( pl->color[2] ) / 2.0f;

		// attenuations
		GL_Bind( GL_TEXTURE1, tr.atten_point_2d );
		SetupAttenuation2D( pl );

		GL_Bind( GL_TEXTURE0, tr.atten_point_1d );
		SetupAttenuation1D( pl );

		GL_SetTexPointer( 0, TC_OFF );
		GL_SetTexPointer( 1, TC_OFF );

		if( GL_Support( R_NV_COMBINE_EXT ) && glConfig.max_nv_combiners >= 2 && !cv_specular_nocombiners->value )
		{
			pglBlendFunc( GL_DST_ALPHA, GL_ONE ); // alpha additive
			pglColorMask( GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE ); // render RGB
			
			pglEnable( GL_REGISTER_COMBINERS_NV );
			pglCombinerParameteriNV( GL_NUM_GENERAL_COMBINERS_NV, 1 );
			pglCombinerParameterfvNV( GL_CONSTANT_COLOR0_NV, (float *)&color );

			// RC 0 setup:
			// spare0 = tex0 + tex1
			pglCombinerInputNV( GL_COMBINER0_NV, GL_RGB, GL_VARIABLE_A_NV, GL_TEXTURE0_ARB, GL_UNSIGNED_IDENTITY_NV, GL_ALPHA );
			pglCombinerInputNV( GL_COMBINER0_NV, GL_RGB, GL_VARIABLE_B_NV, GL_ZERO,GL_UNSIGNED_INVERT_NV, GL_RGB );
			pglCombinerInputNV( GL_COMBINER0_NV, GL_RGB, GL_VARIABLE_C_NV, GL_TEXTURE1_ARB, GL_UNSIGNED_IDENTITY_NV, GL_ALPHA );
			pglCombinerInputNV( GL_COMBINER0_NV, GL_RGB, GL_VARIABLE_D_NV, GL_ZERO, GL_UNSIGNED_INVERT_NV, GL_RGB );
			pglCombinerOutputNV( GL_COMBINER0_NV, GL_RGB, GL_DISCARD_NV, GL_DISCARD_NV, GL_SPARE0_NV, GL_NONE, GL_NONE, GL_FALSE, GL_FALSE, GL_FALSE );

			// Final RC setup:
			// out = (1 - spare0) * color
			pglFinalCombinerInputNV( GL_VARIABLE_A_NV, GL_SPARE0_NV, GL_UNSIGNED_IDENTITY_NV, GL_RGB );
			pglFinalCombinerInputNV( GL_VARIABLE_B_NV, GL_ZERO, GL_UNSIGNED_IDENTITY_NV, GL_RGB );
			pglFinalCombinerInputNV( GL_VARIABLE_C_NV, GL_CONSTANT_COLOR0_NV, GL_UNSIGNED_IDENTITY_NV, GL_RGB );
			pglFinalCombinerInputNV( GL_VARIABLE_D_NV, GL_ZERO, GL_UNSIGNED_IDENTITY_NV, GL_RGB );
			pglFinalCombinerInputNV( GL_VARIABLE_G_NV, GL_ZERO, GL_UNSIGNED_INVERT_NV, GL_ALPHA );

			glpoly_t *p = chain;

			while( p )
			{
				DrawPolyFromArray( p );
				p = p->next;
			}

			pglDisable( GL_REGISTER_COMBINERS_NV );

			GL_SelectTexture( GL_TEXTURE0 );
			GL_TextureTarget( GL_TEXTURE_2D );
			GL_DisableAllTexGens();

			GL_SelectTexture( GL_TEXTURE1 );
			GL_DisableAllTexGens();
		}
		else
		{
			// no combiners, so it will reque 2 more passes

			// add attenuation to alpha
			pglBlendFunc( GL_ZERO, GL_ONE_MINUS_SRC_ALPHA );
			GL_SetTexEnvs( ENVSTATE_REPLACE, ENVSTATE_REPLACE );
			
			// setup alpha summation
			GL_SelectTexture( GL_TEXTURE0 );
			pglTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_ARB );

			pglTexEnvi( GL_TEXTURE_ENV, GL_COMBINE_RGB_ARB, GL_REPLACE );
			pglTexEnvi( GL_TEXTURE_ENV, GL_SOURCE0_RGB_ARB, GL_TEXTURE );
			pglTexEnvi( GL_TEXTURE_ENV, GL_RGB_SCALE_ARB, 1.0f );

			pglTexEnvi( GL_TEXTURE_ENV, GL_COMBINE_ALPHA_ARB, GL_REPLACE) ;
			pglTexEnvi( GL_TEXTURE_ENV, GL_SOURCE0_ALPHA_ARB, GL_TEXTURE );
			pglTexEnvi( GL_TEXTURE_ENV, GL_OPERAND0_ALPHA_ARB, GL_SRC_ALPHA );
			pglTexEnvi( GL_TEXTURE_ENV, GL_ALPHA_SCALE, 1.0f );

			
			GL_SelectTexture( GL_TEXTURE1 );
			pglTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_ARB );

			pglTexEnvi( GL_TEXTURE_ENV, GL_COMBINE_RGB_ARB, GL_REPLACE );
			pglTexEnvi( GL_TEXTURE_ENV, GL_SOURCE0_RGB_ARB, GL_TEXTURE );
			pglTexEnvi( GL_TEXTURE_ENV, GL_RGB_SCALE_ARB, 1.0f );

			pglTexEnvi( GL_TEXTURE_ENV, GL_COMBINE_ALPHA_ARB, GL_ADD );
			pglTexEnvi( GL_TEXTURE_ENV, GL_SOURCE0_ALPHA_ARB, GL_PREVIOUS_ARB );
			pglTexEnvi( GL_TEXTURE_ENV, GL_OPERAND0_ALPHA_ARB, GL_SRC_ALPHA );
			pglTexEnvi( GL_TEXTURE_ENV, GL_SOURCE1_ALPHA_ARB, GL_TEXTURE );
			pglTexEnvi( GL_TEXTURE_ENV, GL_OPERAND1_ALPHA_ARB, GL_SRC_ALPHA );			
			pglTexEnvi( GL_TEXTURE_ENV, GL_ALPHA_SCALE, 1.0f );

			glpoly_t *p = chain;

			while( p )
			{
				DrawPolyFromArray( p );
				p = p->next;
			}

			GL_SelectTexture( GL_TEXTURE0 );
			GL_TextureTarget( GL_TEXTURE_2D );
			GL_DisableAllTexGens();
			pglTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE );

			GL_SelectTexture( GL_TEXTURE1 );
			GL_DisableAllTexGens();
			pglTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE );

			// multiply light color by alpha			
			pglBlendFunc( GL_DST_ALPHA, GL_ONE ); // alpha additive
			pglColorMask( GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE ); // render RGB
			GL_SetTexEnvs( ENVSTATE_OFF ); // dont need textures, only color
			pglColor3f( color[0], color[1], color[2] );

			p = chain;
			while( p )
			{
				DrawPolyFromArray( p );
				p = p->next;
			}
		}
	}
	else
	{
		GL_SelectTexture( GL_TEXTURE2 );
		GL_TextureTarget( GL_NONE );
		GL_DisableAllTexGens();
	}
}

//============================
// LightDrawPoly
//
// binds normal map, sets texture matrix, and renders a polygon
//============================
void LightDrawPoly( msurface_t *surf )
{
	material_t *pMat = surf->texinfo->texture->material;

	GL_Bind( GL_TEXTURE1, pMat->gl_normalmap_id );
	R_SetupTexMatrix( surf, tr.modelorg );
	DrawPolyFromArray( surf );

	if( twopass )
	{
		// add poly to chain for second pass
		surf->polys->next = chain;
		chain = surf->polys;
	}
}

/*******************************
*
* Entity lighting rendering
*
********************************/
//=====================================
// DrawDynamicLightForEntity
//
// Renders dynamic lights on given entity.
// Used on 'moved' entities, who has non-zero angles or origin
//=====================================
void DrawDynamicLightForEntity( cl_entity_t *e, const Vector &mins, const Vector &maxs )
{
	pglDepthFunc( GL_EQUAL );

	// go through dynamic lights list
	float time = GET_CLIENT_TIME();
	DynamicLight *pl = cl_dlights;
	model_t *clmodel = e->model;
	Vector oldorigin = tr.modelorg;

	for( int l = 0; l < MAX_DLIGHTS; l++, pl++ )
	{
		Vector temp, forward, right, up;

		if( pl->die < time || !pl->radius )
			continue;

		RI.currentlight = pl;

		if( R_CullBoxExt( pl->frustum, mins, maxs, pl->clipflags ))
			continue;

		if( e->angles != g_vecZero )
			tr.modelorg = RI.objectMatrix.VectorITransform( pl->origin );
		else tr.modelorg = pl->origin - e->origin;

		// concat by entity matrix
		R_MergeLightProjection( pl );

		if( pl->spotlightTexture )
		{
			if( !SetupSpotLight( pl ))
				continue;
		}
		else
		{
			if( !SetupPointLight( pl ))
				continue;
		}

		// Draw faces
		msurface_t *psurf = &clmodel->surfaces[clmodel->firstmodelsurface];
		for( int i = 0; i < clmodel->nummodelsurfaces; i++, psurf++ )
		{
			if( R_CullSurfaceExt( psurf, pl->frustum, 0 ))
				continue;

			if( !( psurf->flags & SURF_DRAWTILED))
			{
				LightDrawPoly( psurf );
			}
		}

		// finalize light
		if( pl->spotlightTexture )
			FinishSpotLight( pl );
		else FinishPointLight( pl );
	}

	tr.modelorg = oldorigin;
	RI.currentlight = NULL;
}


/*******************************
*
* World and static entites lighting rendering
*
********************************/
//=====================================
// RecursiveDrawWorldLight
//
// recursively goes throug all world nodes drawing visible polygons
//=====================================
void R_RecursiveLightNode( mnode_t *node, const mplane_t frustum[6], unsigned int clipflags, unsigned int lightbits )
{
	const mplane_t	*clipplane;
	msurface_t	*surf;
	int		c;

	if( node->contents == CONTENTS_SOLID )
		return; // hit a solid leaf

	if( node->visframe != tr.visframecount )
		return;

	// buz: visible surfaces already marked
	if( node->contents < 0 ) return;

	if( clipflags )
	{
		for( int i = 0; i < 6; i++ )
		{
			clipplane = &frustum[i];

			if(!( clipflags & ( 1<<i )))
				continue;

			int clipped = BoxOnPlaneSide( node->minmaxs, node->minmaxs + 3, clipplane );
			if( clipped == 2 ) return;
			if( clipped == 1 ) clipflags &= ~(1<<i);
		}
	}

	// node is just a decision point, so go down the apropriate sides

	// find which side of the node we are on
	float dot = PlaneDiff( tr.modelorg, node->plane );
	int side = (dot >= 0) ? 0 : 1;

	// recurse down the children, front side first
	R_RecursiveLightNode( node->children[side], frustum, clipflags, lightbits );

	// draw stuff
	for( c = node->numsurfaces, surf = worldmodel->surfaces + node->firstsurface; c; c--, surf++ )
	{
		if( R_CullSurfaceExt( surf, frustum, clipflags ))
			continue;

		if( !FBitSet( surf->flags, SURF_DRAWTILED ))
		{
			if( r_newrenderer->value )
				R_AddSurfaceToDrawList( surf, lightbits );
			else LightDrawPoly( surf );
		}
	}

	// recurse down the back side
	R_RecursiveLightNode( node->children[!side], frustum, clipflags, lightbits );
}

/*
=================
R_LightStaticModel

Merge static model brushes with world lighted surfaces
=================
*/
void LightStaticModel( const DynamicLight *pl, cl_entity_t *e )
{
	model_t *clmodel = e->model;

	if( R_CullBoxExt( pl->frustum, clmodel->mins, clmodel->maxs, pl->clipflags ))
		return;

	msurface_t *psurf = &clmodel->surfaces[clmodel->firstmodelsurface];
	for( int i = 0; i < clmodel->nummodelsurfaces; i++, psurf++ )
	{
		if( R_CullSurfaceExt( psurf, pl->frustum, pl->clipflags ))
			continue;

		if( !( psurf->flags & SURF_DRAWTILED))
		{
			LightDrawPoly( psurf );
		}
	}
}

//=====================================
// DrawDynamicLights
//
// called from main renderer to draw dynamic light
// on world and static entites
//=====================================
void DrawDynamicLights( void )
{
	pglDepthFunc( GL_EQUAL );

	float time = GET_CLIENT_TIME();
	DynamicLight *pl = cl_dlights;
	Vector oldorigin = tr.modelorg;

	for( int i = 0; i < MAX_DLIGHTS; i++, pl++ )
	{
		if( pl->die < time || !pl->radius )
			continue;

		RI.currentlight = pl;
		tr.modelorg = pl->origin;

		if( pl->spotlightTexture )
		{
			// spotlight
			if( !SetupSpotLight( pl ))
				continue;
		}
		else
		{
			// pointlight
			if( !SetupPointLight( pl ))
				continue;
		}

		// Draw world
		R_RecursiveLightNode( worldmodel->nodes, pl->frustum, pl->clipflags );

		// draw static entities
		for( int j = 0; j < tr.num_static_entities; j++ )
		{
			RI.currententity = tr.static_entities[j];
			RI.currentmodel = RI.currententity->model;
	
			ASSERT( RI.currententity != NULL );
			ASSERT( RI.currententity->model != NULL );

			switch( RI.currententity->model->type )
			{
			case mod_brush:
				LightStaticModel( pl, RI.currententity );
				break;
			default:
				HOST_ERROR( "R_DrawStatics: non bsp model in static list!\n" );
				break;
			}
		}

		if( pl->spotlightTexture )
			FinishSpotLight( pl );
		else FinishPointLight( pl );
	}

	tr.modelorg = oldorigin;
	RI.currentlight = NULL;
}


/*******************************
* Other lights management -
* creation, tracking, deleting..
********************************/
//=====================================
// MY_AllocDlight
//
// Quake's func, allocates dlight
//=====================================
DynamicLight *MY_AllocDlight( int key )
{
	DynamicLight *dl;

	// first look for an exact key match
	if( key )
	{
		dl = cl_dlights;
		for( int i = 0; i < MAX_DLIGHTS; i++, dl++ )
		{
			if( dl->key == key )
			{
				// reuse this light
				return dl;
			}
		}
	}

	float time = GET_CLIENT_TIME();

	// then look for anything else
	dl = cl_dlights;
	for( int i = 0; i < MAX_DLIGHTS; i++, dl++ )
	{
		if( dl->die < time && dl->key == 0 )
		{
			memset( dl, 0, sizeof( *dl ));
			dl->key = key;
			return dl;
		}
	}

	dl = &cl_dlights[0];
	memset( dl, 0, sizeof( *dl ));
	dl->key = key;

	return dl;
}

void MY_DecayLights( void )
{
	float time = GET_CLIENT_TIME();
	DynamicLight *pl = cl_dlights;

	for( int i = 0; i < MAX_DLIGHTS; i++, pl++ )
	{
		if( !pl->radius ) continue;

		pl->radius -= RI.refdef.frametime * pl->decay;
		if( pl->radius < 0 ) pl->radius = 0;

		if( pl->die < time || !pl->radius ) 
			memset( pl, 0, sizeof( *pl ));
	}
}

void ResetDynamicLights( void )
{
	memset( cl_dlights, 0, sizeof( cl_dlights ));
}


//=====================================
// HasDynamicLights
//
// Should return TRUE if we're going to draw some dynamic lighting in this frame -
// renderer will separate lightmaps and material textures drawing
//=====================================
int HasDynamicLights( void )
{
	int numPlights = 0;

	if( !worldmodel->lightdata || !cv_dynamiclight->value )
		return numPlights;

	for( int i = 0; i < MAX_DLIGHTS; i++ )
	{
		DynamicLight *pl = &cl_dlights[i];

		if( pl->die < GET_CLIENT_TIME() || !pl->radius )
			continue;

		numPlights++;
	}

	return numPlights;
}

/*
=======================================================================

	AMBIENT & DIFFUSE LIGHTING

=======================================================================
*/
/*
=================
R_RecursiveLightPoint
=================
*/
static bool R_RecursiveLightPoint( model_t *model, mnode_t *node, const Vector &start, const Vector &end, lightinfo_t *light )
{
	float		front, back, frac;
	int		i, map, side, size, s, t;
	msurface_t	*surf;
	mtexinfo_t	*tex;
	color24		*lm;
	Vector		mid;

	// didn't hit anything
	if( !node || node->contents < 0 )
		return false;

	// calculate mid point
	front = PlaneDiff( start, node->plane );
	back = PlaneDiff( end, node->plane );

	side = front < 0;
	if(( back < 0 ) == side )
		return R_RecursiveLightPoint( model, node->children[side], start, end, light );

	frac = front / ( front - back );

	VectorLerp( start, frac, end, mid );

	// co down front side	
	if( R_RecursiveLightPoint( model, node->children[side], start, mid, light ))
		return true; // hit something

	if(( back < 0 ) == side )
		return false;// didn't hit anything

	// check for impact on this node
	surf = model->surfaces + node->firstsurface;

	for( i = 0; i < node->numsurfaces; i++, surf++ )
	{
		tex = surf->texinfo;

		if( surf->flags & ( SURF_DRAWSKY|SURF_DRAWTILED ))
			continue;	// no lightmaps

		s = DotProduct( mid, tex->vecs[0] ) + tex->vecs[0][3] - surf->texturemins[0];
		t = DotProduct( mid, tex->vecs[1] ) + tex->vecs[1][3] - surf->texturemins[1];

		if(( s < 0 || s > surf->extents[0] ) || ( t < 0 || t > surf->extents[1] ))
			continue;

		s /= LM_SAMPLE_SIZE;
		t /= LM_SAMPLE_SIZE;

		if( !surf->samples )
			return true;

		lm = surf->samples + (t * ((surf->extents[0] / LM_SAMPLE_SIZE ) + 1) + s);
		size = ((surf->extents[0] / LM_SAMPLE_SIZE) + 1) * ((surf->extents[1] / LM_SAMPLE_SIZE) + 1);

		Vector ambientcolor = g_vecZero;
		Vector diffusecolor = g_vecZero;
		Vector origcolor = g_vecZero;
		Vector lightvec = g_vecZero;
		bool bump_light = false;
		float dot = 0.0f;

		for( map = 0; map < MAXLIGHTMAPS && surf->styles[map] != 255; map++ )
		{
			if( surf->styles[map] == 0 )
			{
				// read lightmap color
				origcolor.x = (float)lm->r / 255.0f;
				origcolor.y = (float)lm->g / 255.0f;
				origcolor.z = (float)lm->b / 255.0f;
			}
			else if( surf->styles[map] == BUMP_LIGHTVECS_STYLE )
			{
				Vector vec_x, vec_y, vec_z, tmp;
				matrix3x3	mat;

				// strange gamma manipulation...
				tmp.x = (float)lm->r / 255.0f;
				tmp.y = (float)lm->g / 255.0f;
				tmp.z = (float)lm->b / 255.0f;

				vec_x = Vector( tex->vecs[0] ).Normalize();	// tangent
				vec_y = Vector( tex->vecs[1] ).Normalize();	// binormal
				vec_z = surf->plane->normal.Normalize();	// normal

				if( surf->flags & SURF_PLANEBACK )
					vec_z = -vec_z;

				Vector fromlight = (tmp * 2.0f - 1.0f);		// convert to float

				// create tangent space rotational matrix
				mat.SetForward( Vector( vec_x.x, vec_y.x, vec_z.x ));
				mat.SetRight( Vector( vec_x.y, vec_y.y, vec_z.y ));
				mat.SetUp( Vector( vec_x.z, vec_y.z, vec_z.z ));

				dot = fromlight.z;

				// rotate from tangent to model space
				// get vector *TO* light not FROM* light
				lightvec = mat.VectorIRotate( -fromlight ).Normalize();
			}
			else if( surf->styles[map] == BUMP_ADDLIGHT_STYLE )
			{
				// read diffuse color
				diffusecolor.x = (float)lm->r / 255.0f;
				diffusecolor.y = (float)lm->g / 255.0f;
				diffusecolor.z = (float)lm->b / 255.0f;
			}
			else if( surf->styles[map] == BUMP_BASELIGHT_STYLE )
			{
				// read ambient color
				ambientcolor.x = (float)lm->r / 255.0f;
				ambientcolor.y = (float)lm->g / 255.0f;
				ambientcolor.z = (float)lm->b / 255.0f;
				bump_light = true; // have all styles to build extended info
			}

			lm += size; // skip to next lightmap
		}

		// apply gamma and write to lightinfo_t struct
		if( bump_light )
		{
			Vector tmp = ((diffusecolor * dot) + ambientcolor) * 2.0f;
			Vector scale = g_vecZero;

			if( tmp.x ) scale.x = ApplyGamma( tmp.x ) / tmp.x;
			if( tmp.y ) scale.y = ApplyGamma( tmp.y ) / tmp.y;
			if( tmp.z ) scale.z = ApplyGamma( tmp.z ) / tmp.z;

			light->diffuse.x = diffusecolor.x * scale.x;
			light->diffuse.y = diffusecolor.y * scale.y;
			light->diffuse.z = diffusecolor.z * scale.z;

			light->ambient.x = ambientcolor.x * scale.x;
			light->ambient.y = ambientcolor.y * scale.y;
			light->ambient.z = ambientcolor.z * scale.z;

			light->direction = lightvec;
			light->flags |= (LIGHTING_AMBIENT|LIGHTING_DIFFUSE|LIGHTING_DIRECTION);
		}
		else
		{
			light->ambient.x = ApplyGamma( origcolor.x ) / 2.0f;
			light->ambient.y = ApplyGamma( origcolor.y ) / 2.0f;
			light->ambient.z = ApplyGamma( origcolor.z ) / 2.0f;
			light->flags |= LIGHTING_AMBIENT;
		}
		return true;
	}

	// go down back side
	return R_RecursiveLightPoint( model, node->children[!side], mid, end, light );
}

/*
=================
R_LightTraceFilter
=================
*/
int R_LightTraceFilter( physent_t *pe )
{
	if( !pe || pe->solid != SOLID_BSP )
		return 1;
	return 0;
}

/*
=================
R_LightForPoint
=================
*/
void R_LightForPoint( const Vector &point, lightinfo_t *lightinfo, bool invLight, bool secondpass )
{
	Vector	start, end, dir;

	if( !lightinfo )
	{
		ALERT( at_error, "R_LightForPoint: *lightinfo == NULL\n" );
		return;
	}

	memset( lightinfo, 0, sizeof( *lightinfo ));

	if( !RI.refdef.movevars )
	{
		lightinfo->ambient.x = 0.5f;
		lightinfo->ambient.y = 0.5f;
		lightinfo->ambient.z = 0.5f;
		lightinfo->flags |= LIGHTING_AMBIENT;
		lightinfo->origin = point;
		return;
	}

	// set to full bright if no light data
	if( !worldmodel || !worldmodel->lightdata || r_fullbright->value )
	{
		lightinfo->ambient.x = ApplyGamma( RI.refdef.movevars->skycolor_r * (1.0f/255.0f)) / 4.0f;
		lightinfo->ambient.y = ApplyGamma( RI.refdef.movevars->skycolor_g * (1.0f/255.0f)) / 4.0f;
		lightinfo->ambient.z = ApplyGamma( RI.refdef.movevars->skycolor_b * (1.0f/255.0f)) / 4.0f;
		lightinfo->direction.x = RI.refdef.movevars->skyvec_x;
		lightinfo->direction.y = RI.refdef.movevars->skyvec_y;
		lightinfo->direction.z = RI.refdef.movevars->skyvec_z;
		lightinfo->diffuse.x = lightinfo->ambient.x;
		lightinfo->diffuse.y = lightinfo->ambient.y;
		lightinfo->diffuse.z = lightinfo->ambient.z;
		lightinfo->flags |= (LIGHTING_AMBIENT|LIGHTING_DIFFUSE|LIGHTING_DIRECTION);
		lightinfo->origin = point;
		return;
	}

	// Get lighting at this point
	start = end = point;

	if( invLight )
	{
		start.z = point.z - 64.0f;
		end.z = point.z + 256.0f;
	}
	else
	{
		start.z = point.z + 64.0f;
		end.z = point.z - 256.0f;
	}

	// always have valid model
	model_t *pmodel = worldmodel;
	mnode_t *pnodes = pmodel->nodes;
	cl_entity_t *m_pGround = NULL;

	if( r_lighting_extended->value && !secondpass )
	{
		pmtrace_t	trace;

		gEngfuncs.pEventAPI->EV_SetTraceHull( 2 );
		gEngfuncs.pEventAPI->EV_PlayerTraceExt( start, end, PM_STUDIO_IGNORE, R_LightTraceFilter, &trace );
		m_pGround = GET_ENTITY( gEngfuncs.pEventAPI->EV_IndexFromTrace( &trace ));
	}

	if( m_pGround && m_pGround->model && m_pGround->model->type == mod_brush )
	{
		hull_t	*hull;
		Vector	start_l, end_l;
		Vector	offset;

		pmodel = m_pGround->model;
		pnodes = &pmodel->nodes[pmodel->hulls[0].firstclipnode];

		hull = &pmodel->hulls[0];
		offset = -hull->clip_mins;
		offset += m_pGround->origin;

		start_l = start - offset;
		end_l = end - offset;

		// rotate start and end into the models frame of reference
		if( m_pGround->angles != g_vecZero )
		{
			matrix4x4	matrix( offset, m_pGround->angles );
			start_l = matrix.VectorITransform( start );
			end_l = matrix.VectorITransform( end );
		}

		// copy transformed pos back
		start = start_l;
		end = end_l;
	}

	if( !R_RecursiveLightPoint( pmodel, pnodes, start, end, lightinfo ))
	{
		if( r_lighting_extended->value && !secondpass )
		{
			R_LightForPoint( point, lightinfo, invLight, true );
			return;	// trying get lighting from world
		}

		// Object is too high from ground (for example, helicopter). Use sky color and direction
		lightinfo->ambient.x = ApplyGamma( RI.refdef.movevars->skycolor_r * (1.0f/255.0f)) / 4.0f;
		lightinfo->ambient.y = ApplyGamma( RI.refdef.movevars->skycolor_g * (1.0f/255.0f)) / 4.0f;
		lightinfo->ambient.z = ApplyGamma( RI.refdef.movevars->skycolor_b * (1.0f/255.0f)) / 4.0f;
		lightinfo->direction.x = RI.refdef.movevars->skyvec_x;
		lightinfo->direction.y = RI.refdef.movevars->skyvec_y;
		lightinfo->direction.z = RI.refdef.movevars->skyvec_z;
		lightinfo->diffuse.x = lightinfo->ambient.x;
		lightinfo->diffuse.y = lightinfo->ambient.y;
		lightinfo->diffuse.z = lightinfo->ambient.z;
		lightinfo->flags |= (LIGHTING_AMBIENT|LIGHTING_DIFFUSE|LIGHTING_DIRECTION);
	}

	lightinfo->origin = point;
}