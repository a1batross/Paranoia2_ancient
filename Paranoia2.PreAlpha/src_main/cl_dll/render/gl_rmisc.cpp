/*
gl_rmisc.cpp - renderer miscellaneous
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
#include "triapiobjects.h"
#include "r_studioint.h"
#include "ref_params.h"
#include "gl_local.h"
#include <mathlib.h>
#include <stringlib.h>
#include "gl_studio.h"
#include "gl_decals.h"
#include "gl_aurora.h"
#include "gl_rpart.h"
#include "rain.h"

#define PROJ_SIZE		64
#define ATTN_SIZE		128
#define HALF_ATTN_SIZE	(ATTN_SIZE>>1)
#define ATTN_3D_SIZE	32
#define HALF_ATTN_3D_SIZE	(ATTN_3D_SIZE>>1)

void R_InitMaterials( void )
{
	ALERT( at_aiconsole, "loading matdesc.txt\n" );

	char *afile = (char *)gEngfuncs.COM_LoadFile( "textures/matdesc.txt", 5, NULL );

	if( !afile )
	{
		ALERT( at_error, "Cannot open file \"textures/matdesc.txt\"\n" );
		return;
	}

	char *pfile = afile;
	char token[256];
	int depth = 0;
	tr.matcount = 0;

	// count materials
	while( pfile != NULL )
	{
		pfile = COM_ParseFile( pfile, token );
		if( !pfile ) break;

		if( token[0] == '{' )
		{
			depth++;
		}
		else if( token[0] == '}' )
		{
			tr.matcount++;
			depth--;
		}
	}

	if( depth > 0 ) ALERT( at_warning, "matdesc.txt: EOF reached without closing brace\n" );
	if( depth < 0 ) ALERT( at_warning, "matdesc.txt: EOF reached without opening brace\n" );

	tr.materials = (matdesc_t *)Mem_Alloc( sizeof( matdesc_t ) * tr.matcount );
	pfile = afile; // start real parsing

	int current = 0;

	while( pfile != NULL )
	{
		pfile = COM_ParseFile( pfile, token );
		if( !pfile ) break;

		if( current >= tr.matcount )
		{
			ALERT ( at_error, "material parse is overrun %d > %d\n", current, tr.matcount );
			break;
		}

		matdesc_t	*mat = &tr.materials[current];

		// read the material name
		Q_strncpy( mat->name, token, sizeof( mat->name ));

		COM_StripExtension( mat->name );

		// read opening brace
		pfile = COM_ParseFile( pfile, token );
		if( !pfile ) break;

		if( token[0] != '{' )
		{
			ALERT( at_error, "found %s when expecting {\n", token );
			break;
		}

		while( pfile != NULL )
		{
			pfile = COM_ParseFile( pfile, token );
			if( !pfile )
			{
				ALERT( at_error, "EOF without closing brace\n" );
				goto getout;
			}

			// description end goto next material
			if( token[0] == '}' )
			{
				current++;
				break;
			}
			else if( !Q_stricmp( token, "glossExp" ))
			{
				pfile = COM_ParseFile( pfile, token );
				if( !pfile )
				{
					ALERT( at_error, "hit EOF while parsing 'glossExp'\n" );
					goto getout;
				}

				mat->glossExp = Q_atof( token );
				mat->glossExp = bound( 1.0f, mat->glossExp, 256.0f );
			}
			else if( !Q_stricmp( token, "POMScale" ))
			{
				pfile = COM_ParseFile( pfile, token );
				if( !pfile )
				{
					ALERT( at_error, "hit EOF while parsing 'POMScale'\n" );
					goto getout;
				}

				mat->parallaxScale = Q_atof( token );
				mat->parallaxScale = bound( 0.1f, mat->parallaxScale, 50.0f );
			}
			else if( !Q_stricmp( token, "POMSteps" ))
			{
				pfile = COM_ParseFile( pfile, token );
				if( !pfile )
				{
					ALERT( at_error, "hit EOF while parsing 'POMSteps'\n" );
					goto getout;
				}

				mat->parallaxSteps = Q_atoi( token );
				mat->parallaxSteps = bound( 1, mat->parallaxSteps, 100 );
			}
			else if( !Q_stricmp( token, "LightRemap" ))
			{
				for( int i = 0; i < 4; i++ )
				{
					pfile = COM_ParseFile( pfile, token );
					if( !pfile )
					{
						ALERT( at_error, "hit EOF while parsing 'LightRemap'\n" );
						goto getout;
					}

					mat->lightRemap[i] = Q_atof( token );
					mat->lightRemap[i] = bound( -2.0f, mat->lightRemap[i], 2.0f );
				}
			}
			else if( !Q_stricmp( token, "ReflectScale" ))
			{
				pfile = COM_ParseFile( pfile, token );
				if( !pfile )
				{
					ALERT( at_error, "hit EOF while parsing 'ReflectScale'\n" );
					goto getout;
				}

				mat->reflectScale = Q_atof( token );
				mat->reflectScale = bound( 0.001f, mat->reflectScale, 10.0f );
			}
			else if( !Q_stricmp( token, "RefractScale" ))
			{
				pfile = COM_ParseFile( pfile, token );
				if( !pfile )
				{
					ALERT( at_error, "hit EOF while parsing 'RefractScale'\n" );
					goto getout;
				}

				mat->refractScale = Q_atof( token );
				mat->refractScale = bound( 0.001f, mat->refractScale, 10.0f );
			}
			else ALERT( at_warning, "Unknown material token %s\n", token );
		}

		// apply default values
		if( !mat->glossExp ) mat->glossExp = 32.0f;
		if( !mat->parallaxScale ) mat->parallaxScale = 0.5f;
		if( !mat->parallaxSteps ) mat->parallaxSteps = 10;
		if( !mat->reflectScale ) mat->reflectScale = 0.0f; // > 0.0f enables reflective bump-mapping
		if( !mat->refractScale ) mat->refractScale = 1.0f;
	}
getout:
	gEngfuncs.COM_FreeFile( afile );
	ALERT( at_aiconsole, "%d materials parsed\n", current );
}

/*
==================
R_FindMaterial

This function never failed
==================
*/
matdesc_t *R_FindMaterial( const char *name )
{
	static matdesc_t	defmat;

	if( !defmat.name[0] )
	{
		// initialize default material
		Q_strncpy( defmat.name, "*default", sizeof( defmat.name ));
		defmat.glossExp = 32.0f;
		defmat.parallaxScale = 0.5f;
		defmat.parallaxSteps = 10;
		defmat.lightRemap[0] = 0.0f;
		defmat.lightRemap[1] = 0.0f;
		defmat.lightRemap[2] = 0.0f;
		defmat.lightRemap[3] = 0.0f;
		defmat.reflectScale = 0.0f;
		defmat.refractScale = 1.0f;
	}

	for( int i = 0; i < tr.matcount; i++ )
	{
		if( !Q_stricmp( name, tr.materials[i].name ))
			return &tr.materials[i];
	}

	return &defmat;
}

void R_Create1DAttenuation( void )
{
	if( tr.atten_point_1d ) return;

	byte data[ATTN_SIZE*4];

	for( int x = 0; x < ATTN_SIZE; x++ )
	{
		float result;

		if( x == 0 || x == (ATTN_SIZE-1))
		{
			result = 255.0f;
		}
		else
		{
			float xf = ((float)x - HALF_ATTN_SIZE) / (float)HALF_ATTN_SIZE;
			result = (xf * xf) * 255;
			if( result > 255 ) result = 255;
		}

		data[x*4+0] = 0;
		data[x*4+1] = 0;
		data[x*4+2] = 0;
		data[x*4+3] = (byte)result;
	}

	// 1d attenutaion texture for old cards
	tr.atten_point_1d = CREATE_TEXTURE( "*attenPoint1D", ATTN_SIZE, 1, data, TF_HAS_ALPHA|TF_NOMIPMAP|TF_CLAMP|TF_TEXTURE_1D ); 
}

void R_Create2DAttenuation( void )
{
	if( tr.atten_point_2d ) return;

	byte data[ATTN_SIZE*ATTN_SIZE*4];

	for( int x = 0; x < ATTN_SIZE; x++ )
	{
		for( int y = 0; y < ATTN_SIZE; y++ )
		{
			float	result;

			if( x == 0 || x == (ATTN_SIZE-1) || y == 0 || y == (ATTN_SIZE-1))
			{
				result = 255.0f;
			}
			else
			{
				float xf = ((float)x - HALF_ATTN_SIZE) / (float)HALF_ATTN_SIZE;
				float yf = ((float)y - HALF_ATTN_SIZE) / (float)HALF_ATTN_SIZE;
				result = ((xf * xf) + (yf * yf)) * 255;
				if( result > 255 ) result = 255;
			}
			
			data[(y*ATTN_SIZE+x)*4+0] = 0;
			data[(y*ATTN_SIZE+x)*4+1] = 0;
			data[(y*ATTN_SIZE+x)*4+2] = 0;
			data[(y*ATTN_SIZE+x)*4+3] = (byte)result;
		}
	}

	// 2d attenutaion texture for old cards
	tr.atten_point_2d = CREATE_TEXTURE( "*attenPoint2D", ATTN_SIZE, ATTN_SIZE, data, TF_HAS_ALPHA|TF_NOMIPMAP|TF_CLAMP ); 
}

void R_Create3DAttenuation( void )
{
	if( tr.atten_point_3d ) return;

	byte data[ATTN_3D_SIZE*ATTN_3D_SIZE*ATTN_3D_SIZE*4];
	float f = HALF_ATTN_3D_SIZE * HALF_ATTN_3D_SIZE;

	for( int x = 0; x < ATTN_3D_SIZE; x++ )
	{
		for( int y = 0; y < ATTN_3D_SIZE; y++ )
		{
			for( int z = 0; z < ATTN_3D_SIZE; z++ )
			{
				Vector vec;
				vec.x = (float)x - HALF_ATTN_3D_SIZE;
				vec.y = (float)y - HALF_ATTN_3D_SIZE;
				vec.z = (float)z - HALF_ATTN_3D_SIZE;

				float dist = vec.Length();
				if( dist > HALF_ATTN_3D_SIZE )
					dist = HALF_ATTN_3D_SIZE;

				float att;
				if( x == 0 || y == 0 || z == 0 || x == (ATTN_3D_SIZE-1) || y == (ATTN_3D_SIZE-1) || z == (ATTN_3D_SIZE-1))
				{
					att = 0.0f;
				}
				else
				{
					att = (((dist * dist) / f) - 1.0f ) * -255.0f;
                                        }

				data[(x*ATTN_3D_SIZE*ATTN_3D_SIZE+y*ATTN_3D_SIZE+z)*4+0] = (byte)att;
				data[(x*ATTN_3D_SIZE*ATTN_3D_SIZE+y*ATTN_3D_SIZE+z)*4+1] = (byte)att;
				data[(x*ATTN_3D_SIZE*ATTN_3D_SIZE+y*ATTN_3D_SIZE+z)*4+2] = (byte)att;
				data[(x*ATTN_3D_SIZE*ATTN_3D_SIZE+y*ATTN_3D_SIZE+z)*4+3] = 0;
			}
		}
	}

	// 3d attenutaion texture
	tr.atten_point_3d = CREATE_TEXTURE( "*attenPoint3D", ATTN_3D_SIZE, ATTN_3D_SIZE, data, TF_NOMIPMAP|TF_CLAMP|TF_TEXTURE_3D ); 
}

void R_CreateSpotLightTexture( void )
{
	if( tr.defaultProjTexture ) return;

	byte data[PROJ_SIZE*PROJ_SIZE*4];
	byte *p = data;

	for( int i = 0; i < PROJ_SIZE; i++ )
	{
		float dy = (PROJ_SIZE * 0.5f - i + 0.5f) / (PROJ_SIZE * 0.5f);

		for( int j = 0; j < PROJ_SIZE; j++ )
		{
			float dx = (PROJ_SIZE * 0.5f - j + 0.5f) / (PROJ_SIZE * 0.5f);
			float r = cos( M_PI / 2.0f * sqrt(dx * dx + dy * dy));
			float c;

			r = (r < 0) ? 0 : r * r;
			c = 0xFF * r;
			p[0] = (c <= 0xFF) ? c : 0xFF;
			p[1] = (c <= 0xFF) ? c : 0xFF;
			p[2] = (c <= 0xFF) ? c : 0xFF;
			p[3] = (c <= 0xff) ? c : 0xFF;
			p += 4;
		}
	}

	tr.defaultProjTexture = CREATE_TEXTURE( "*spotlight", PROJ_SIZE, PROJ_SIZE, data, TF_SPOTLIGHT ); 
}

void R_CreateNoiseTexture( void )
{
	if( tr.noiseTexture ) return;

	int noise_map_size = NOISE_SIZE * NOISE_SIZE * NOISE_SIZE;
	int noise_size = noise_map_size * 4;
	float *data = new float[noise_size];
	float *p = data;

	init_noise();

	for( int i = 0; i < noise_map_size; i += 4 )
	{
		for( int j = 0; j < 3; j++ )
		{
			float x = i+j + sin( i+j );
			float y = i+j + cos( i+j );
			*p++ = noise( x, y, i+j );
		}
	}

	// water ripples texture
	tr.noiseTexture = CREATE_TEXTURE( "*noise3D", NOISE_SIZE, NOISE_SIZE, data, TF_TEXTURE_3D|TF_NOMIPMAP ); 

	delete [] data;
}

void R_CreateRefractionTexture( void )
{
	if( tr.refractionTexture  )
		FREE_TEXTURE( tr.refractionTexture );
	tr.refractionTexture = CREATE_TEXTURE( "*screenrefract", glState.width, glState.height, NULL, TF_SCREEN|TF_RECTANGLE );
}

/*
==================
R_NewMap

Called always when map is changed or restarted
==================
*/
void R_NewMap( void )
{
	int	i;

	tr.world_has_mirrors = RENDER_GET_PARM( PARM_MAP_HAS_MIRRORS, 0 ) ? true : false;
	tr.world_has_movies = false;
	tr.world_has_skybox = false;

	// Engine already released entity array so we can't release
	// model instance for each entity pesronally 
	g_StudioRenderer.DestroyAllModelInstances();

	// invalidate model handles
	for( i = 1; i < RENDER_GET_PARM( PARM_MAX_ENTITIES, 0 ); i++ )
	{
		cl_entity_t *e = GET_ENTITY( i );
		if( !e ) break;

		e->modelhandle = INVALID_HANDLE;
	}

	GET_VIEWMODEL()->modelhandle = INVALID_HANDLE;

	gHUD.m_pHeadShieldEnt->modelhandle = INVALID_HANDLE;

	for( i = 0; i < MAX_SHADOWS; i++ )
	{
		if( !tr.shadowTextures[i] ) break;
		FREE_TEXTURE( tr.shadowTextures[i] );
	}

	for( i = 0; i < MAX_MIRRORS; i++ )
	{
		if( !tr.mirrorTextures[i].texture ) break;
		FREE_TEXTURE( tr.mirrorTextures[i].texture );
	}

	// setup special flags
	for( i = 0; i < worldmodel->numsurfaces; i++ )
	{
		msurface_t *surf = &worldmodel->surfaces[i];
		mextrasurf_t *info = SURF_INFO( surf, worldmodel );

		// clear previous data
		surf->flags &= ~SURF_REFLECT;
		surf->flags &= ~SURF_REFLECT_PUDDLE;
		surf->flags &= ~SURF_REFLECT_FRESNEL;

		info->mirrortexturenum = 0;
		info->checkcount = -1;

		if( surf->flags & SURF_DRAWSKY )
			tr.world_has_skybox = true;

		if( surf->flags & SURF_DRAWTURB )
		{
			surf->flags |= SURF_REFLECT;
			tr.world_has_mirrors = true;
		}

		if( !Q_strncmp( surf->texinfo->texture->name, "movie", 5 ))
		{
			surf->flags |= SURF_MOVIE;
			tr.world_has_movies = true;
		}

		if( !Q_strncmp( surf->texinfo->texture->name, "mirror", 6 ) || !Q_strncmp( surf->texinfo->texture->name, "reflect", 7 ))
		{
			surf->flags |= SURF_REFLECT;
			tr.world_has_mirrors = true;
		}

		if( surf->texinfo->texture->material->flags & BRUSH_REFLECT_SPEC && ( i < worldmodel->nummodelsurfaces ))
		{
			// only floor affected
			if( !FBitSet( surf->flags, SURF_PLANEBACK ) && surf->plane->normal == Vector( 0.0f, 0.0f, 1.0f ))
			{
				if( FBitSet( surf->flags, SURF_BUMPDATA ))
				{
					// a special reflect
					surf->flags |= SURF_REFLECT_FRESNEL;
					tr.world_has_mirrors = true;
				}
			}
		}
	}

	// get the actual screen size
	glState.width = RENDER_GET_PARM( PARM_SCREEN_WIDTH, 0 );
	glState.height = RENDER_GET_PARM( PARM_SCREEN_HEIGHT, 0 );

	r_viewleaf = r_viewleaf2 = NULL;
	tr.framecount = tr.visframecount = 1;	// no dlight cache

	// load water animation
	for( i = 0; i < WATER_TEXTURES; i++ )
	{
		char path[256];
		Q_snprintf( path, sizeof( path ), "gfx/water/water_normal_%i.tga", i );
		tr.waterTextures[i] = LOAD_TEXTURE( path, NULL, 0, 0 );
	}

	// setup the skybox sides
	for( i = 0; i < 6; i++ )
		tr.skyboxTextures[i] = RENDER_GET_PARM( PARM_TEX_SKYBOX, i );

	memset( tr.mirrorTextures, 0, sizeof( tr.mirrorTextures ));
	memset( tr.shadowTextures, 0, sizeof( tr.shadowTextures ));

	tr.fbo_mirror.Init( FBO_COLOR, glState.width, glState.height, FBO_NOTEXTURE );
	tr.fbo_shadow.Init( FBO_DEPTH, glState.width, glState.height, FBO_NOTEXTURE );

	if( r_sunshadows->value == 1.0f )
		tr.fbo_sunshadow.Init( FBO_DEPTH, 1024, 1024 );
	else if( r_sunshadows->value == 2.0f )
		tr.fbo_sunshadow.Init( FBO_DEPTH, 2048, 2048 );
	else if( r_sunshadows->value == 3.0f )
		tr.fbo_sunshadow.Init( FBO_DEPTH, 4096, 4096 );
	else tr.fbo_sunshadow.Free();

	tr.num_cin_used = tr.num_mirrors_used = 0;

	R_FreeCinematics(); // free old cinematics

	R_Create1DAttenuation ();
	R_Create2DAttenuation ();
	R_Create3DAttenuation ();
	R_CreateSpotLightTexture ();

	// initialize spotlights
	if( !tr.spotlightTexture[0] )
	{
		char path[256];

		tr.spotlightTexture[0] = tr.defaultProjTexture;	// always present
		tr.flashlightTexture = LOAD_TEXTURE( "gfx/flashlight.tga", NULL, 0, TF_SPOTLIGHT );
		if( !tr.flashlightTexture ) tr.flashlightTexture = tr.defaultProjTexture;

		// 7 custom textures allowed
		for( int i = 1; i < 8; i++ )
		{
			Q_snprintf( path, sizeof( path ), "gfx/spotlight%i.tga", i );

			if( FILE_EXISTS( path ))
				tr.spotlightTexture[i] = LOAD_TEXTURE( path, NULL, 0, TF_SPOTLIGHT );

			if( !tr.spotlightTexture[i] )
				tr.spotlightTexture[i] = tr.defaultProjTexture; // make default if missed
		}
	}

	ClearDecals();
	ResetDynamicLights();
	ResetSky();
	GrassVidInit();
	g_objmanager.Reset();  
	ResetRain();

	g_pParticleSystems.ClearSystems(); // buz

	g_pParticles.Clear();
	
	R_CreateRefractionTexture();
	R_InitCinematics();

	R_InitBloomTextures();
	InitPostTextures();
}