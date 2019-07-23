//
// written by BUzer for HL: Paranoia modification
//
//		2006

#include "hud.h"
#include "cl_util.h"
#include "const.h"
#include "entity_state.h"
#include "cl_entity.h"
#include "triangleapi.h"
#include "com_model.h"
#include "gl_local.h"
#include "mathlib.h"

extern int g_iGunMode;

#define DEAD_GRAYSCALE_TIME		5.0f

cvar_t *v_posteffects;
cvar_t *v_grayscale;
cvar_t *v_hdr;
cvar_t *v_hdr_white;
cvar_t *v_hdr_intensity;

float GetGrayscaleFactor()
{
	float grayscale = v_grayscale->value;

	if( gHUD.m_flDeadTime )
	{
		float fact = (GET_CLIENT_TIME() - gHUD.m_flDeadTime) / DEAD_GRAYSCALE_TIME;

		if( fact > 1.0f ) fact = 1.0f;
		if( fact > grayscale ) grayscale = fact;
	}

	return grayscale;
}

void InitPostEffects( void )
{
	v_posteffects = CVAR_REGISTER( "gl_posteffects", "1", FCVAR_ARCHIVE );
	v_grayscale = CVAR_REGISTER( "gl_grayscale", "0", 0 );
	v_hdr = CVAR_REGISTER( "r_hdr", "1", FCVAR_ARCHIVE );
	v_hdr_white = CVAR_REGISTER( "r_hdr_white", "0.1", FCVAR_ARCHIVE );
	v_hdr_intensity = CVAR_REGISTER( "r_hdr_intensity", "0.5", FCVAR_ARCHIVE );
}

void InitPostTextures( void )
{
	if( tr.screen_depth )
	{
		FREE_TEXTURE( tr.screen_depth );
		tr.screen_depth = 0;
          }

	if( tr.screen_rgba )
	{
		FREE_TEXTURE( tr.screen_rgba );
		tr.screen_rgba = 0;
          }

	if( tr.screen_rgba1 )
	{
		FREE_TEXTURE( tr.screen_rgba1 );
		tr.screen_rgba1 = 0;
          }
}

void RenderFSQ( void )
{
	pglBegin( GL_QUADS );
		pglTexCoord2f( 0, 1 );
		pglVertex2f( 0, 0 );
		pglTexCoord2f( 1, 1 );
		pglVertex2f( 1, 0 );
		pglTexCoord2f( 1, 0 );
		pglVertex2f( 1, 1 );
		pglTexCoord2f( 0, 0 );
		pglVertex2f( 0, 1 );
	pglEnd();
}

void RenderFSQ( int wide, int tall )
{
	float screenWidth = (float)wide;
	float screenHeight = (float)tall;

	pglBegin( GL_QUADS );
		pglTexCoord2f( 0, 1 );
		pglVertex2f( 0, 0 );
		pglTexCoord2f( 1, 1 );
		pglVertex2f( screenWidth, 0 );
		pglTexCoord2f( 1, 0 );
		pglVertex2f( screenWidth, screenHeight );
		pglTexCoord2f( 0, 0 );
		pglVertex2f( 0, screenHeight );
	pglEnd();
}

void RenderDOF( const ref_params_t *fd )
{
	// disabled or missed shader
	if( !v_posteffects->value || !r_dof->value || !glsl.dofShader )
		return;

	if( g_iGunMode == 0 ) // uninitialized?
		return;

	int screenWidth = RI.viewport[2];
	int screenHeight = RI.viewport[3];
	float zNear = 4.0f;	// fixed
	float zFar = max( 256.0f, RI.farClip );
	float depthValue = 0.0f;

	static float m_flCachedDepth = 0.0f;
	static float m_flLastDepth = 0.0f;
	static float m_flStartDepth = 0.0f;
	static float m_flOffsetDepth = 0.0f;
	static float m_flStartTime = 0.0f;
	static float m_flDelayTime = 0.0f;
	static int g_iGunLastMode = 1;

	static float m_flStartLength = 0.0f;
	static float m_flOffsetLength = 0.0f;
	static float m_flLastLength = 0.0f;
	static float m_flDOFStartTime = 0.0f;

	if( g_iGunMode != g_iGunLastMode )
	{
		if( g_iGunMode == 1 )
		{
			// disable iron sight
			m_flStartLength = m_flLastLength;
			m_flOffsetLength = -m_flStartLength;
			m_flDOFStartTime = fd->time;
		}
		else
		{
			// enable iron sight
			m_flStartLength = m_flLastLength;
			m_flOffsetLength = r_dof_focal_length->value;
			m_flDOFStartTime = fd->time;
		}


//		ALERT( at_console, "Iron sight changed( %i )\n", g_iGunMode );
		g_iGunLastMode = g_iGunMode;
	}

	if( g_iGunLastMode == 1 && m_flDOFStartTime == 0.0f )
		return; // iron sight disabled

	if( m_flDOFStartTime != 0.0f )
	{
		float flDegree = (fd->time - m_flDOFStartTime) / 0.3f;

		if( flDegree >= 1.0f )
		{
			// all done. holds the final value
			m_flLastLength = m_flStartLength + m_flOffsetLength;
			m_flDOFStartTime = 0.0f; // done
		}
		else
		{
			// evaluate focal length
			m_flLastLength = m_flStartLength + m_flOffsetLength * flDegree;
		}
	}

	// set up full screen workspace
	pglMatrixMode( GL_PROJECTION );
	pglLoadIdentity();

	pglOrtho( 0, glState.width, glState.height, 0, -99999, 99999 );

	pglMatrixMode( GL_MODELVIEW );
	pglLoadIdentity();

	pglDisable( GL_DEPTH_TEST );
	pglDisable( GL_ALPHA_TEST );
	pglDepthMask( GL_FALSE );
	pglDisable( GL_BLEND );

	GL_Cull( GL_FRONT );
	pglColor4f( 1.0f, 1.0f, 1.0f, 1.0f );
	pglViewport( 0, 0, glState.width, glState.height );

	// get current depth value
	pglReadPixels( screenWidth >> 1, screenHeight >> 1, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &depthValue );
	depthValue = -zFar * zNear / ( depthValue * ( zFar - zNear ) - zFar );

	float holdTime = bound( 0.01f, r_dof_hold_time->value, 0.5f );
	float changeTime = bound( 0.1f, r_dof_change_time->value, 2.0f );

	if( round( m_flCachedDepth, 100 ) != round( depthValue, 100 ))
	{
		m_flStartTime = 0.0f;		// cancelling changes
		m_flDelayTime = fd->time;		// make sure what focal point is not changed more than 0.5 secs
		m_flStartDepth = m_flLastDepth;	// get last valid depth
		m_flOffsetDepth = depthValue - m_flStartDepth;
		m_flCachedDepth = depthValue;
	}

	if(( fd->time - m_flDelayTime ) > holdTime && m_flStartTime == 0.0f && m_flDelayTime != 0.0f )
	{
		// begin the change depth
		m_flStartTime = fd->time;
	}

	if( m_flStartTime != 0.0f )
	{
		float flDegree = (fd->time - m_flStartTime) / changeTime;

		if( flDegree >= 1.0f )
		{
			// all done. holds the final value
			m_flLastDepth = m_flStartDepth + m_flOffsetDepth;
			m_flStartTime = m_flDelayTime = 0.0f;
		}
		else
		{
			// evaluate focal depth
			m_flLastDepth = m_flStartDepth + m_flOffsetDepth * flDegree;
		}
	}

	if( !tr.screen_depth )
		tr.screen_depth = CREATE_TEXTURE( "*screendepth", glState.width, glState.height, NULL, TF_DEPTH ); 

	if( !tr.screen_rgba )
		tr.screen_rgba = CREATE_TEXTURE( "*screenrgba", glState.width, glState.height, NULL, TF_IMAGE|TF_NEAREST );

	// capture depth, if not captured previously in SSAO
	GL_Bind( GL_TEXTURE0, tr.screen_depth );
	pglCopyTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, 0, 0, RI.viewport[2], RI.viewport[3] );

	// capture screen
	GL_Bind( GL_TEXTURE0, tr.screen_rgba );
	pglCopyTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, 0, 0, RI.viewport[2], RI.viewport[3] );

	// combine normal and blurred scenes
	GL_BindShader( glsl.dofShader );

	// setup uniforms
	pglUniform1fARB( glsl.dofShader->u_ScreenWidth, screenWidth );
	pglUniform1fARB( glsl.dofShader->u_ScreenHeight, screenHeight );
	pglUniform1iARB( glsl.dofShader->u_GenericCondition, r_dof_debug->value ? 1 : 0 );
	pglUniform1fARB( glsl.dofShader->u_FocalDepth, m_flLastDepth );
	pglUniform1fARB( glsl.dofShader->u_FocalLength, m_flLastLength );
	pglUniform1fARB( glsl.dofShader->u_FStop, r_dof_fstop->value );
	pglUniform1fARB( glsl.dofShader->u_zFar, zFar );

	GL_Bind( GL_TEXTURE0, tr.screen_rgba );
	GL_Bind( GL_TEXTURE1, tr.screen_depth );
	RenderFSQ( screenWidth, screenHeight );

	// unbind shader
	GL_BindShader( NULL );
	GL_CleanUpTextureUnits( 0 );

	pglMatrixMode( GL_PROJECTION );
	GL_LoadMatrix( RI.projectionMatrix );

	pglMatrixMode( GL_MODELVIEW );
	GL_LoadMatrix( RI.worldviewMatrix );

	pglEnable( GL_DEPTH_TEST );
	pglDepthMask( GL_TRUE );
	pglDisable( GL_BLEND );
	GL_Cull( GL_FRONT );
}

void RenderUnderwaterBlur( void )
{
	if( !v_posteffects->value || !cv_water->value || RI.refdef.waterlevel < 3 || !glsl.blurShader )
		return;

	// set up full screen workspace
	pglMatrixMode( GL_PROJECTION );
	pglLoadIdentity();

	pglOrtho( 0, glState.width, glState.height, 0, -99999, 99999 );

	pglMatrixMode( GL_MODELVIEW );
	pglLoadIdentity();

	pglDisable( GL_DEPTH_TEST );
	pglDisable( GL_ALPHA_TEST );
	pglDepthMask( GL_FALSE );
	pglDisable( GL_BLEND );

	GL_Cull( 0 );
	pglColor4f( 1.0f, 1.0f, 1.0f, 1.0f );
	pglViewport( 0, 0, glState.width, glState.height );

	if( !tr.screen_rgba )
		tr.screen_rgba = CREATE_TEXTURE( "*screenrgba", glState.width, glState.height, NULL, TF_IMAGE|TF_NEAREST );

	// capture screen
	GL_Bind( GL_TEXTURE0, tr.screen_rgba );
	pglCopyTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, 0, 0, RI.viewport[2], RI.viewport[3] );

	// combine normal and blurred scenes
	GL_BindShader( glsl.blurShader );

	float factor = sin( RI.refdef.time * 0.1f * ( M_PI * 2.7f ));
	float blurX = RemapVal( factor, -1.0f, 1.0f, 0.08f, 0.13f );
	float blurY = RemapVal( factor, -1.0f, 1.0f, 0.05f, 0.14f );

	pglUniform1fARB( glsl.blurShader->u_BlurFactor, blurX );	// set blur factor
	pglUniform1iARB( glsl.blurShader->u_GenericCondition, 0 );
	RenderFSQ( RI.viewport[2], RI.viewport[3] );
	GL_Bind( GL_TEXTURE0, tr.screen_rgba );
	pglCopyTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, 0, 0, RI.viewport[2], RI.viewport[3] );

	pglUniform1fARB( glsl.blurShader->u_BlurFactor, blurY );	// set blur factor
	pglUniform1iARB( glsl.blurShader->u_GenericCondition, 1 );
	RenderFSQ( RI.viewport[2], RI.viewport[3] );
	pglCopyTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, 0, 0, RI.viewport[2], RI.viewport[3] );

	// unbind shader
	GL_BindShader( NULL );
	GL_CleanUpTextureUnits( 0 );

	pglMatrixMode( GL_PROJECTION );
	GL_LoadMatrix( RI.projectionMatrix );

	pglMatrixMode( GL_MODELVIEW );
	GL_LoadMatrix( RI.worldviewMatrix );

	pglEnable( GL_DEPTH_TEST );
	pglDepthMask( GL_TRUE );
	pglDisable( GL_BLEND );
	GL_Cull( GL_FRONT );
}

void RenderMonochrome( void )
{
	if( !v_posteffects->value || !glsl.monoShader )
		return;

	float grayscale = GetGrayscaleFactor();
	if( grayscale <= 0.0f ) return;

	// set up full screen workspace
	pglMatrixMode( GL_PROJECTION );
	pglLoadIdentity();

	pglOrtho( 0, glState.width, glState.height, 0, -99999, 99999 );

	pglMatrixMode( GL_MODELVIEW );
	pglLoadIdentity();

	pglDisable( GL_DEPTH_TEST );
	pglDisable( GL_ALPHA_TEST );
	pglDepthMask( GL_FALSE );
	pglDisable( GL_BLEND );

	GL_Cull( 0 );
	pglColor4f( 1.0f, 1.0f, 1.0f, 1.0f );
	pglViewport( 0, 0, glState.width, glState.height );

	if( !tr.screen_rgba )
		tr.screen_rgba = CREATE_TEXTURE( "*screenrgba", glState.width, glState.height, NULL, TF_IMAGE|TF_NEAREST );

	// capture screen
	GL_Bind( GL_TEXTURE0, tr.screen_rgba );
	pglCopyTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, 0, 0, glState.width, glState.height );

	// combine normal and blurred scenes
	GL_BindShader( glsl.monoShader );

	pglUniform1fARB( glsl.monoShader->u_BlurFactor, min( grayscale, 1.0f ));
	RenderFSQ( glState.width, glState.height );

	// unbind shader
	GL_BindShader( NULL );
	GL_CleanUpTextureUnits( 0 );

	pglMatrixMode( GL_PROJECTION );
	GL_LoadMatrix( RI.projectionMatrix );

	pglMatrixMode( GL_MODELVIEW );
	GL_LoadMatrix( RI.worldviewMatrix );

	pglEnable( GL_DEPTH_TEST );
	pglDepthMask( GL_TRUE );
	pglDisable( GL_BLEND );
	GL_Cull( GL_FRONT );
}

void RenderHDRL( const ref_params_t *fd )
{
	if( !v_posteffects->value || !v_hdr->value )
		return;

	// set up full screen workspace
	pglMatrixMode( GL_MODELVIEW );
	pglLoadIdentity();

	pglMatrixMode( GL_PROJECTION );
	pglLoadIdentity();
	pglOrtho( 0, 1, 1, 0, -99999, 99999 );

	pglDisable( GL_DEPTH_TEST );
	pglDisable( GL_ALPHA_TEST );
	pglDepthMask( GL_FALSE );
	pglDisable( GL_BLEND );

	GL_Cull( 0 );
	pglColor4f( 1.0f, 1.0f, 1.0f, 1.0f );
	pglViewport( 0, 0, glState.width, glState.height );

	int width = glState.width / 4;
	int height = glState.height / 4;

	if( !tr.screen_rgba )
		tr.screen_rgba = CREATE_TEXTURE( "*screenrgba", glState.width, glState.height, NULL, TF_IMAGE|TF_NEAREST );

	if( !tr.screen_rgba1 )
		tr.screen_rgba1 = CREATE_TEXTURE( "*screenrgba1", width, height, NULL, TF_IMAGE|TF_FLOAT );

	// capture screen
	GL_Bind( GL_TEXTURE0, tr.screen_rgba );
	pglCopyTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, 0, 0, RI.viewport[2], RI.viewport[3] );

	pglViewport( 0, 0, width, height );
	RenderFSQ();

	// save scaled texture
	GL_Bind( GL_TEXTURE0, tr.screen_rgba1 );
	pglCopyTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, 0, 0, width, height );

	// do brightpass
	GL_BindShader( glsl.filterShader );
	RenderFSQ();

	pglCopyTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, 0, 0, width, height );

	GL_BindShader( glsl.blurShader );
	pglUniform1iARB( glsl.blurShader->u_GenericCondition, 0 );
	pglUniform1fARB( glsl.blurShader->u_BlurFactor, 0.125f );	// set blur factor

	RenderFSQ();

	pglCopyTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, 0, 0, width, height );

	pglUniform1iARB( glsl.blurShader->u_GenericCondition, 1 );

	RenderFSQ();

	pglCopyTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, 0, 0, width, height );

	// do final pass
	pglViewport( 0, 0, glState.width, glState.height );

	GL_BindShader( glsl.tonemapShader );
	pglUniform1fARB( glsl.tonemapShader->u_WhiteFactor, bound( 0.001f, v_hdr_white->value, 10.0f )); // set white factor
	pglUniform1fARB( glsl.tonemapShader->u_BlurFactor, bound( 0.001f, v_hdr_intensity->value, 10.0f )); // set white factor

	GL_Bind( GL_TEXTURE0, tr.screen_rgba );
	GL_Bind( GL_TEXTURE1, tr.screen_rgba1 );
	RenderFSQ();

	// unbind shader
	GL_BindShader( NULL );
	GL_CleanUpTextureUnits( 0 );

	pglMatrixMode( GL_PROJECTION );
	GL_LoadMatrix( RI.projectionMatrix );

	pglMatrixMode( GL_MODELVIEW );
	GL_LoadMatrix( RI.worldviewMatrix );

	pglEnable( GL_DEPTH_TEST );
	pglDepthMask( GL_TRUE );
	pglDisable( GL_BLEND );
	GL_Cull( GL_FRONT );
}