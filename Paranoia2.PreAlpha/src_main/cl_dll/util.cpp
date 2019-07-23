/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
*   Use, distribution, and modification of this source code and/or resulting
*   object code is restricted to non-commercial enhancements to products from
*   Valve LLC.  All other use, distribution, or modification is prohibited
*   without written permission from Valve LLC.
*
****/
//
// util.cpp
//
// implementation of class-less helper functions
//

#include "STDIO.H"
#include "STDLIB.H"
#include "MATH.H"

#include "hud.h"
#include "cl_util.h"
#include <stringlib.h>
#include "com_model.h"
#include <mathlib.h>
#include "gl_local.h"
#include "event_api.h"
#include "r_efx.h"

/*
=============
pfnAlertMessage

=============
*/
void ALERT( ALERT_TYPE level, char *szFmt, ... )
{
	char	buffer[2048];	// must support > 1k messages
	va_list	args;

	// check message for pass
	switch( level )
	{
	case at_notice:
		break;	// passed always
	case at_console:
		if( developer_level < 1 )	// "-dev 1"
			return;
		break;
	case at_warning:
		if( developer_level < 2 )	// "-dev 2"
			return;
		break;
	case at_error:
		if( developer_level < 3 )	// "-dev 3"
			return;
		break;
	case at_aiconsole:
		if( developer_level < 4 )	// "-dev 4"
			return;
		break;
	}

	va_start( args, szFmt );
	Q_vsnprintf( buffer, 2048, szFmt, args );
	va_end( args );

	if( level == at_warning )
	{
		gEngfuncs.Con_Printf( va( "^3Warning:^7 %s", buffer ));
	}
	else if( level == at_error  )
	{
		gEngfuncs.Con_Printf( va( "^1Error:^7 %s", buffer ));
	} 
	else
	{
		gEngfuncs.Con_Printf( buffer );
	}
}

/*
====================
Sys LoadGameDLL

====================
*/
bool Sys_LoadLibrary( const char* dllname, dllhandle_t* handle, const dllfunc_t *fcts )
{
	if( !handle ) return false;

	const dllfunc_t *gamefunc;

	// Initializations
	for( gamefunc = fcts; gamefunc && gamefunc->name != NULL; gamefunc++ )
		*gamefunc->func = NULL;

	char dllpath[128];

	// is direct path used ?
	if( dllname[0] == '*' ) Q_strncpy( dllpath, dllname + 1, sizeof( dllpath ));
	else Q_snprintf( dllpath, sizeof( dllpath ), "%s/cl_dlls/%s", gEngfuncs.pfnGetGameDirectory(), dllname );

	dllhandle_t dllhandle = LoadLibrary( dllpath );
        
	// No DLL found
	if( !dllhandle ) return false;

	// Get the function adresses
	for( gamefunc = fcts; gamefunc && gamefunc->name != NULL; gamefunc++ )
	{
		if( !( *gamefunc->func = (void *)Sys_GetProcAddress( dllhandle, gamefunc->name )))
		{
			Sys_FreeLibrary( &dllhandle );
			return false;
		}
	}          

	ALERT( at_aiconsole, "%s loaded succesfully!\n", (dllname[0] == '*') ? (dllname+1) : (dllname));
	*handle = dllhandle;

	return true;
}

void *Sys_GetProcAddress( dllhandle_t handle, const char *name )
{
	return (void *)GetProcAddress( handle, name );
}

void Sys_FreeLibrary( dllhandle_t *handle )
{
	if( !handle || !*handle )
		return;

	FreeLibrary( *handle );
	*handle = NULL;
}

/*
=============
WorldToScreen

convert world coordinates (x,y,z) into screen (x, y)
=============
*/
int WorldToScreen( const Vector &world, Vector &screen )
{
	int retval = R_WorldToScreen( world, screen );

	screen[0] =  0.5f * screen[0] * (float)RI.refdef.viewport[2];
	screen[1] = -0.5f * screen[1] * (float)RI.refdef.viewport[3];
	screen[0] += 0.5f * (float)RI.refdef.viewport[2];
	screen[1] += 0.5f * (float)RI.refdef.viewport[3];

	return retval;
}

//=================
//   UTIL_IsPlayer
//=================
bool UTIL_IsPlayer( int idx )
{
	if ( idx >= 1 && idx <= gEngfuncs.GetMaxClients() )
		return true;
	return false;
}


//=================
//     UTIL_IsLocal
//=================
bool UTIL_IsLocal( int idx )
{
	return gEngfuncs.pEventAPI->EV_IsLocal( idx - 1 ) ? true : false;
}

/*
======================================================================

LEAF LISTING

NOTE: this code ripped out from Xash3D
======================================================================
*/
static void Mod_BoxLeafnums_r( leaflist_t *ll, mnode_t *node, model_t *worldmodel )
{
	mplane_t	*plane;
	int	s;

	while( 1 )
	{
		if( node->contents == CONTENTS_SOLID )
			return;

		if( node->contents < 0 )
		{
			mleaf_t	*leaf = (mleaf_t *)node;

			// it's a leaf!
			if( ll->count >= ll->maxcount )
			{
				ll->overflowed = true;
				return;
			}

			ll->list[ll->count++] = leaf - worldmodel->leafs - 1;
			return;
		}
	
		plane = node->plane;
		s = BOX_ON_PLANE_SIDE( ll->mins, ll->maxs, plane );

		if( s == 1 )
		{
			node = node->children[0];
		}
		else if( s == 2 )
		{
			node = node->children[1];
		}
		else
		{
			// go down both
			if( ll->topnode == -1 )
				ll->topnode = node - worldmodel->nodes;
			Mod_BoxLeafnums_r( ll, node->children[0], worldmodel );
			node = node->children[1];
		}
	}
}

/*
==================
Mod_BoxLeafnums
==================
*/
int Mod_BoxLeafnums( const Vector &mins, const Vector &maxs, short *list, int listsize, int *topnode )
{
	leaflist_t ll;
	model_t	*worldmodel;

	worldmodel = gEngfuncs.GetEntityByIndex( 0 )->model;

	if( !worldmodel )
		return 0;

	ll.mins = mins;
	ll.maxs = maxs;
	ll.count = 0;
	ll.maxcount = listsize;
	ll.list = list;
	ll.topnode = -1;
	ll.overflowed = false;

	Mod_BoxLeafnums_r( &ll, worldmodel->nodes, worldmodel );

	if( topnode ) *topnode = ll.topnode;
	return ll.count;
}

/*
=============
Mod_BoxVisible

Returns true if any leaf in boxspace
is potentially visible
=============
*/
bool Mod_BoxVisible( const Vector &mins, const Vector &maxs, const byte *visbits )
{
	short	leafList[256];
	int	i, count;

	if( !visbits || !mins || !maxs )
		return true;

	count = Mod_BoxLeafnums( mins, maxs, leafList, 256, NULL );

	for( i = 0; i < count; i++ )
	{
		int leafnum = leafList[i];

		if( leafnum != -1 && visbits[leafnum>>3] & (1<<( leafnum & 7 )))
			return true;
	}
	return false;
}

/*
===================
Mod_DecompressVis
===================
*/
byte *Mod_DecompressVis( const byte *in, model_t *model )
{
	static byte	decompressed[MAX_MAP_LEAFS/8];
	int		c;
	byte		*out;
	int		row;

	row = (model->numleafs+7)>>3;	
	out = decompressed;

	if( !in )
	{	// no vis info, so make all visible
		while( row )
		{
			*out++ = 0xff;
			row--;
		}
		return decompressed;
	}

	do
	{
		if( *in )
		{
			*out++ = *in++;
			continue;
		}

		c = in[1];
		in += 2;
		while( c )
		{
			*out++ = 0;
			c--;
		}
	} while( out - decompressed < row );
	
	return decompressed;
}

byte *Mod_LeafPVS( mleaf_t *leaf, model_t *model )
{
	if( !model || !leaf || leaf == model->leafs || !model->visdata )
		return Mod_DecompressVis( NULL, model );

	return Mod_DecompressVis( leaf->compressed_vis, model );
}

/*
==================
Mod_PointInLeaf

==================
*/
mleaf_t *Mod_PointInLeaf( const Vector &p, mnode_t *node )
{
	while( 1 )
	{
		if( node->contents < 0 )
			return (mleaf_t *)node;
		node = node->children[PlaneDiff( p, node->plane ) < 0];
	}

	// never reached
	return NULL;
}

byte *Mod_GetCurrentVis( void )
{
	return RI.visbytes;
}

bool Mod_CheckEntityPVS( cl_entity_t *ent )
{
	if( !ent || !ent->index )
		return false;	// not exist on the client

	if( ent->curstate.messagenum != r_currentMessageNum )
		return false;	// already culled by server

	Vector mins = ent->curstate.origin + ent->curstate.mins;
	Vector maxs = ent->curstate.origin + ent->curstate.maxs;

	return Mod_BoxVisible( mins, maxs, Mod_GetCurrentVis( ));
}

bool Mod_CheckEntityLeafPVS( const Vector &absmin, const Vector &absmax, mleaf_t *leaf )
{
	return Mod_BoxVisible( absmin, absmax, Mod_LeafPVS( leaf, worldmodel ));
}

bool Mod_CheckTempEntityPVS( TEMPENTITY *pTemp )
{
	if( !pTemp ) return false; // not exist on the client

	Vector mins = pTemp->entity.curstate.origin + pTemp->entity.curstate.mins;
	Vector maxs = pTemp->entity.curstate.origin + pTemp->entity.curstate.maxs;

	return Mod_BoxVisible( mins, maxs, Mod_GetCurrentVis( ));
}

model_t *Mod_Handle( int modelIndex )
{
	return IEngineStudio.GetModelByIndex( modelIndex );
}

int Mod_GetType( int modelIndex )
{
	model_t 	*m_pModel;

	m_pModel = IEngineStudio.GetModelByIndex( modelIndex );
	if( m_pModel == NULL )
		return mod_bad;

	return m_pModel->type;
}

/*
===================
Mod_GetFrames
===================
*/
void Mod_GetFrames( int modelIndex, int &numFrames )
{
	model_t 	*m_pModel;

	m_pModel = IEngineStudio.GetModelByIndex( modelIndex );

	if( !m_pModel )
	{
		numFrames = 1;
		return;
	}

	numFrames = m_pModel->numframes;
	if( numFrames < 1 ) numFrames = 1;
}

/*
====================
V_CalcFov
====================
*/
float V_CalcFov( float &fov_x, float width, float height )
{
	float x, half_fov_y;

	if( fov_x < 1 || fov_x > 170 )
	{
		ALERT( at_error, "V_CalcFov: bad fov %g!\n", fov_x );
		fov_x = 90;
	}

	x = width / tan( DEG2RAD( fov_x ) * 0.5f );
	half_fov_y = atan( height / x );

	return RAD2DEG( half_fov_y ) * 2;
}

/*
====================
V_AdjustFov
====================
*/
void V_AdjustFov( float &fov_x, float &fov_y, float width, float height, bool lock_x )
{
	float x, y;

	if(( width * 3 ) == ( 4 * height ) || ( width * 4 ) == ( height * 5 ))
	{
		// 4:3 or 5:4 ratio
		return;
	}

	if( lock_x )
	{
		fov_y = 2 * atan(( width * 3 ) / ( height * 4 ) * tan( fov_y * M_PI / 360.0 * 0.5 )) * 360 / M_PI;
		return;
	}

	y = V_CalcFov( fov_x, 640, 480 );
	x = fov_x;

	fov_x = V_CalcFov( y, height, width );

	if( fov_x < x )
		fov_x = x;
	else fov_y = y;
}

/*
===============================================================================

	LEGACY STUFF

  moved here from studio_util.cpp
===============================================================================
*/
/*
====================
AngleMatrix

====================
*/
void AngleMatrix (const float *angles, float (*matrix)[4] )
{
	float		angle;
	float		sr, sp, sy, cr, cp, cy;
	
	angle = angles[YAW] * (M_PI*2 / 360);
	sy = sin(angle);
	cy = cos(angle);
	angle = angles[PITCH] * (M_PI*2 / 360);
	sp = sin(angle);
	cp = cos(angle);
	angle = angles[ROLL] * (M_PI*2 / 360);
	sr = sin(angle);
	cr = cos(angle);

	// matrix = (YAW * PITCH) * ROLL
	matrix[0][0] = cp*cy;
	matrix[1][0] = cp*sy;
	matrix[2][0] = -sp;
	matrix[0][1] = sr*sp*cy+cr*-sy;
	matrix[1][1] = sr*sp*sy+cr*cy;
	matrix[2][1] = sr*cp;
	matrix[0][2] = (cr*sp*cy+-sr*-sy);
	matrix[1][2] = (cr*sp*sy+-sr*cy);
	matrix[2][2] = cr*cp;
	matrix[0][3] = 0.0;
	matrix[1][3] = 0.0;
	matrix[2][3] = 0.0;
}

/*
====================
VectorTransform

====================
*/
void VectorTransform (const float *in1, float in2[3][4], float *out)
{
	out[0] = DotProduct(in1, in2[0]) + in2[0][3];
	out[1] = DotProduct(in1, in2[1]) + in2[1][3];
	out[2] = DotProduct(in1, in2[2]) + in2[2][3];
}

/*
===============================================================================

	NOISE STUFF

  pasted from www.steps3d.narod.ru
===============================================================================
*/
static int	p[512];
static bool	noise_init = false;

inline float fade( float t )
{
	return t * t * t * (t * (t * 6 - 15) + 10); 
}

inline float lerp( float t, float a, float b )
{ 
	return a + t * (b - a); 
}

inline float grad( int hash, float x, float y, float z )
{
	int h = hash & 15;						// CONVERT LO 4 BITS OF HASH CODE
	float u = h < 8 ? x : y, v = h < 4 ? y : h == 12 || h == 14 ? x : z;	// INTO 12 GRADIENT DIRECTIONS.
	return ((h & 1) == 0 ? u : -u) + ((h & 2) == 0 ? v : -v);
}

const int permutation[256] = 
{ 
151,160,137,91,90,15,131,13,201,95,96,53,194,233,7,225,140,36,103,30,69,142,
8,99,37,240,21,10,23,190, 6,148,247,120,234,75,0,26,197,62,94,252,219,203,
117,35,11,32,57,177,33,88,237,149,56,87,174,20,125,136,171,168, 68,175,74,
165,71,134,139,48,27,166,77,146,158,231,83,111,229,122,60,211,133,230,220,
105,92,41,55,46,245,40,244,102,143,54, 65,25,63,161, 1,216,80,73,209,76,132,
187,208, 89,18,169,200,196,135,130,116,188,159,86,164,100,109,198,173,186,
3,64,52,217,226,250,124,123,5,202,38,147,118,126,255,82,85,212,207,206,59,
227,47,16,58,17,182,189,28,42,223,183,170,213,119,248,152,2,44,154,163,70,
221,153,101,155,167, 43,172,9,129,22,39,253, 19,98,108,110,79,113,224,232,
178,185,112,104,218,246,97,228,251,34,242,193,238,210,144,12,191,179,162,
241, 81,51,145,235,249,14,239,107,49,192,214, 31,181,199,106,157,184, 84,204,
176,115,121,50,45,127,4,150,254,138,236,205,93,222,114,67,29,24,72,243,141,
128,195,78,66,215,61,156,180
};

void init_noise( void )
{
	if( noise_init ) return;

	for( int i = 0; i < 256; i++ ) 
		p[256 + i] = p[i] = permutation[i];
	noise_init = true;
}

float noise( float vx, float vy, float vz ) 
{
	float fx = floor( vx );	// get floor values of coords
  	float fy = floor( vy );
   	float fz = floor( vz );
   		
	int X = ((int)fx) & 255;	// FIND UNIT CUBE THAT
	int Y = ((int)fy) & 255;	// CONTAINS POINT.
	int Z = ((int)fz) & 255;
          
	float x = vx - fx;		// FIND RELATIVE X,Y,Z
	float y = vy - fy;		// OF POINT IN CUBE.
	float z = vz - fz;
      	
	float u = fade( x );	// COMPUTE FADE CURVES
	float v = fade( y );	// FOR EACH OF X,Y,Z.
	float w = fade( z );
            
	int a = p[X+0]+Y, aa = p[a]+Z, ab = p[a+1]+Z;	// HASH COORDINATES OF
 	int b = p[X+1]+Y, ba = p[b]+Z, bb = p[b+1]+Z;	// THE 8 CUBE CORNERS,

	return lerp( w, lerp( v, lerp( u, grad( p[aa], x, y, z ),// AND ADD
                                   grad(p[ba+0], x-1, y  , z   )), // BLENDED
                           lerp(u, grad(p[ab+0], x  , y-1, z   ),  // RESULTS
                                   grad(p[bb+0], x-1, y-1, z   ))),// FROM  8
                   lerp(v, lerp(u, grad(p[aa+1], x  , y  , z-1 ),  // CORNERS
                                   grad(p[ba+1], x-1, y  , z-1 )), // OF CUBE
                           lerp(u, grad(p[ab+1], x  , y-1, z-1 ),
                                   grad(p[bb+1], x-1, y-1, z-1 ))));
}

#ifndef M_PI
#define M_PI		3.14159265358979323846	// matches value in gcc v2 math.h
#endif

vec3_t vec3_origin( 0, 0, 0 );

double sqrt(double x);

float Length(const float *v)
{
	int		i;
	float	length;
	
	length = 0;
	for (i=0 ; i< 3 ; i++)
		length += v[i]*v[i];
	length = sqrt (length);		// FIXME

	return length;
}

void VectorAngles( const float *forward, float *angles )
{
	float	tmp, yaw, pitch;
	
	if (forward[1] == 0 && forward[0] == 0)
	{
		yaw = 0;
		if (forward[2] > 0)
			pitch = 90;
		else
			pitch = 270;
	}
	else
	{
		yaw = (atan2(forward[1], forward[0]) * 180 / M_PI);
		if (yaw < 0)
			yaw += 360;

		tmp = sqrt (forward[0]*forward[0] + forward[1]*forward[1]);
		pitch = (atan2(forward[2], tmp) * 180 / M_PI);
		if (pitch < 0)
			pitch += 360;
	}
	
	angles[0] = pitch;
	angles[1] = yaw;
	angles[2] = 0;
}

float VectorNormalize (float *v)
{
	float	length, ilength;

	length = v[0]*v[0] + v[1]*v[1] + v[2]*v[2];
	length = sqrt (length);		// FIXME

	if (length)
	{
		ilength = 1/length;
		v[0] *= ilength;
		v[1] *= ilength;
		v[2] *= ilength;
	}
		
	return length;

}

void VectorInverse ( float *v )
{
	v[0] = -v[0];
	v[1] = -v[1];
	v[2] = -v[2];
}

void VectorScale (const float *in, float scale, float *out)
{
	out[0] = in[0]*scale;
	out[1] = in[1]*scale;
	out[2] = in[2]*scale;
}

void VectorMA (const float *veca, float scale, const float *vecb, float *vecc)
{
	vecc[0] = veca[0] + scale*vecb[0];
	vecc[1] = veca[1] + scale*vecb[1];
	vecc[2] = veca[2] + scale*vecb[2];
}

HSPRITE LoadSprite(const char *pszName)
{
	int i;
	char sz[256]; 

	if (ScreenWidth < 640)
		i = 320;
	else
		i = 640;

	sprintf(sz, pszName, i);

	return SPR_Load(sz);
}
