//
// written by BUzer for HL: Paranoia modification
//
//		2006

#include "hud.h"
#include "cl_util.h"
#include "const.h"
#include "com_model.h"
#include "r_studioint.h"
#include "ref_params.h"
#include "pm_defs.h"
#include "pmtrace.h"
#include "parsemsg.h"
#include "gl_local.h"
#include "gl_sprite.h"
#include "pm_movevars.h"

#define SKY_DISTANCE		10.0f	// BUzer says what is number from ceiling :-)
#define SKYFLAG_USE_AMBLIGHT_HACK	1
#define SKYFLAG_USE_SHADELIGHT_HACK	2

/*
remap from
{ "lf", "bk", "rt", "ft", "up", "dn" };	// Xash3D loading order
to
{ "rt", "bk", "lf", "ft", "up", "dn" };	// Paranoia loading order
*/

static const int	r_skyTexOrder[6] = { 2, 1, 0, 3, 4, 5 };

Vector		skyangles;
Vector		sky_forward, sky_right, sky_up;

void MakeSkyVectors( const Vector &angles )
{
	// update vectors
	AngleVectors( angles, sky_forward, sky_right, sky_up );

	sky_forward = sky_forward * SKY_DISTANCE;
	sky_right = sky_right * -SKY_DISTANCE;
	sky_up = sky_up * SKY_DISTANCE;
}

void SetSkyAngles( void )
{
	if( gEngfuncs.Cmd_Argc() <= 3 )
	{
		ALERT( at_console, "usage: skyangles <pitch> <yaw> <roll>\n" );
		ALERT( at_console, "( default skyangles is 0 -90 90 )\n" );
		return;
	}

	skyangles[0] = atof( CMD_ARGV( 1 ));
	skyangles[1] = atof( CMD_ARGV( 2 ));
	skyangles[2] = atof( CMD_ARGV( 3 ));

	MakeSkyVectors( skyangles );
}

// 3d skybox
int MsgSkyMarker_Sky( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );

	tr.sky_origin.x = READ_COORD();
	tr.sky_origin.y = READ_COORD();
	tr.sky_origin.z = READ_COORD();

	return 1;
}

// 3d skybox
int MsgSkyMarker_World( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );

	tr.sky_world_origin.x = READ_COORD();
	tr.sky_world_origin.y = READ_COORD();
	tr.sky_world_origin.z = READ_COORD();
	tr.sky_speed = READ_COORD();

	return 1;
}

void ResetSky( void )
{
	skyangles = Vector( 0, -90, 90 );
	tr.sky_origin = tr.sky_world_origin = g_vecZero;
	MakeSkyVectors( skyangles );
	tr.sky_speed = 0;
}

void InitSky( void )
{
	ADD_COMMAND( "skyangles", SetSkyAngles );

	gEngfuncs.pfnHookUserMsg( "skymark_sky", MsgSkyMarker_Sky );
	gEngfuncs.pfnHookUserMsg( "skymark_w", MsgSkyMarker_World );
}

/*
===============
DrawSky

NOTE: skybox is preload by engine and
always have valid textures
===============
*/
void DrawSky( void )
{
	if( !( RI.params & RP_SKYVISIBLE ))
		return;

	pglDepthRange( 0.9f, 1.0f );
	pglDisable( GL_BLEND );
	pglDepthMask( GL_FALSE );

	// disable texturing on all units except first
	GL_SetTexEnvs( ENVSTATE_REPLACE );

	// clipplane cut the sky if drawing through mirror. Disable it
	GL_ClipPlane( false );

	Vector points[8];

	points[0] = RI.vieworg + sky_forward - sky_up + sky_right;
	points[1] = RI.vieworg + sky_forward + sky_up + sky_right;
	points[2] = RI.vieworg - sky_forward + sky_up + sky_right;
	points[3] = RI.vieworg - sky_forward - sky_up + sky_right;
	points[4] = RI.vieworg + sky_forward - sky_up - sky_right;
	points[5] = RI.vieworg + sky_forward + sky_up - sky_right;
	points[6] = RI.vieworg - sky_forward + sky_up - sky_right;
	points[7] = RI.vieworg - sky_forward - sky_up - sky_right;

	static const int idx[6][4] =
	{
	{ 1, 2, 6, 5 },
	{ 2, 3, 7, 6 },
	{ 3, 0, 4, 7 },
	{ 0, 1, 5, 4 },
	{ 2, 1, 0, 3 },
	{ 7, 4, 5, 6 },
	};

	movevars_t *mv = RI.refdef.movevars;

	GL_BindShader( glsl.skyboxShader );

	pglUniform3fARB( RI.currentshader->u_LightDir, mv->skyvec_x, mv->skyvec_y, mv->skyvec_z );
	pglUniform3fARB( RI.currentshader->u_LightDiffuse, mv->skycolor_r / 255.0f, mv->skycolor_g / 255.0f, mv->skycolor_b / 255.0f );
	pglUniform3fARB( RI.currentshader->u_ViewOrigin, RI.vieworg.x, RI.vieworg.y, RI.vieworg.z );

	for( int i = 0; i < 6; i++ )
	{
		GL_Bind( GL_TEXTURE0, tr.skyboxTextures[r_skyTexOrder[i]] );

		pglBegin( GL_POLYGON );
			pglTexCoord2f( 0.001f, 0.001f );
			pglVertex3fv( points[idx[i][0]] );
			pglTexCoord2f( 0.999f, 0.001f );
			pglVertex3fv( points[idx[i][1]] );
			pglTexCoord2f( 0.999f, 0.999f );
			pglVertex3fv( points[idx[i][2]] );
			pglTexCoord2f( 0.001f, 0.999f );
			pglVertex3fv( points[idx[i][3]] );
		pglEnd ();
	}

	GL_BindShader( NULL );
	GL_ClipPlane( true );

	// back to normal depth range
	pglDepthRange( gldepthmin, gldepthmax );
}