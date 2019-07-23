/*
gl_rsurf.cpp - surface-related code
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

#include "r_studioint.h"
#include "ref_params.h"
#include "gl_local.h"
#include "gl_decals.h"
#include <mathlib.h>

static BrushFace *faces_extradata = NULL;

// debug info collection
static int wpolytotal;
static int wpolybumped;
static int wpolynormal;
static int wpolyspecular;
static int wpolyspecular_hi;

msurface_t *specularchain_low = NULL;
msurface_t *specularchain_high = NULL;
msurface_t *specularchain_both = NULL;
msurface_t *skychain = NULL;

// for special bump passes on hardware with < 4 TU
msurface_t *lightmapchains[MAX_LIGHTMAPS];
qboolean needs_special_bump_passes = FALSE;
qboolean needs_second_pass = FALSE;
static int hasdynlights;

// =================================
// Debug tools
// =================================
void ResetCounters( void )
{
	wpolytotal = 0;
	wpolybumped = 0;
	wpolynormal = 0;
	wpolyspecular = 0;
	wpolyspecular_hi = 0;
}

void PrintBumpDebugInfo( void )
{
	if( cv_bumpdebug->value )
	{
		char tmp[256];
		sprintf( tmp, "%d wpoly total", wpolytotal );
		DrawConsoleString( XRES(10), YRES(135), tmp );

		sprintf( tmp, "%d wpoly bumped", wpolybumped );
		DrawConsoleString( XRES(10), YRES(150), tmp );

		sprintf( tmp, "%d wpoly normal", wpolynormal );
		DrawConsoleString( XRES(10), YRES(165), tmp );

		sprintf( tmp, "%d wpoly has low quality specular", wpolyspecular );
		DrawConsoleString( XRES(10), YRES(180), tmp );	

		sprintf( tmp, "%d wpoly has high quality specular", wpolyspecular_hi );
		DrawConsoleString( XRES(10), YRES(195), tmp );
	}
}

//================================
// ARRAYS AND VBOS MANAGEMENT
//
//================================
void FreeBuffer( void )
{
	if( tr.vbo_buffer )
	{
		pglDeleteBuffersARB( 1, &tr.vbo_buffer );
		tr.vbo_buffer = 0;
	}

	if( tr.vbo_buffer_data )
	{
		delete[] tr.vbo_buffer_data;
		tr.vbo_buffer_data = NULL;
	}

	if( faces_extradata )
	{
		delete[] faces_extradata;
		faces_extradata = NULL;
	}
}

void GenerateVertexArray( void )
{
	FreeBuffer();

	// clear all previous errors
	while( pglGetError() != GL_NO_ERROR );

	// calculate number of used faces and vertexes
	msurface_t *surfaces = worldmodel->surfaces;
	int i, numfaces = 0, numverts = 0;

	for( i = 0; i < worldmodel->numsurfaces; i++ )
	{
		if( !( surfaces[i].flags & ( SURF_DRAWSKY|SURF_DRAWTURB )))
		{
			glpoly_t *p = surfaces[i].polys;
			if( p->numverts > 0 )
			{
				numfaces++;
				numverts += p->numverts;
			}
		}
	}

	ALERT( at_aiconsole, "%d world polygons visible\n", numfaces );
	ALERT( at_aiconsole, "%d vertexes in all world polygons\n", numverts );

	// create vertex array
	int curvert = 0;
	int curface = 0;
	tr.vbo_buffer_data = new BrushVertex[numverts];
	faces_extradata = new BrushFace[numfaces];

	for( i = 0; i < worldmodel->numsurfaces; i++ )
	{
		if( !( surfaces[i].flags & ( SURF_DRAWSKY|SURF_DRAWTURB )))
		{
			glpoly_t *p = surfaces[i].polys;

			if( p->numverts > 0 )
			{
				float *v = p->verts[0];

				// hack: pack extradata index in unused bits of poly->flags
				int packed = (curface << 16);
				packed |= (p->flags & 0xFFFF);
				p->flags = packed;

				BrushFace* ext = &faces_extradata[curface];
				ext->start_vertex = curvert;

				// store tangent space
				ext->s_tangent = surfaces[i].texinfo->vecs[0];
				ext->t_tangent = surfaces[i].texinfo->vecs[1];

				ext->s_tangent = ext->s_tangent.Normalize();
				ext->t_tangent = ext->t_tangent.Normalize();
				ext->normal = surfaces[i].plane->normal;

				if( surfaces[i].flags & SURF_PLANEBACK )
					ext->normal = -ext->normal;

				for( int j = 0; j < p->numverts; j++, v += VERTEXSIZE )
				{
					tr.vbo_buffer_data[curvert].pos[0] = v[0];
					tr.vbo_buffer_data[curvert].pos[1] = v[1];
					tr.vbo_buffer_data[curvert].pos[2] = v[2];
					tr.vbo_buffer_data[curvert].texcoord[0] = v[3];
					tr.vbo_buffer_data[curvert].texcoord[1] = v[4];
					tr.vbo_buffer_data[curvert].lightmaptexcoord[0] = v[5];
					tr.vbo_buffer_data[curvert].lightmaptexcoord[1] = v[6];
					curvert++;
				}

				curface++;
			}
		}
	}

	ALERT( at_aiconsole, "Created Vertex Array for world polys\n" );

	if( GL_Support( R_ARB_VERTEX_BUFFER_OBJECT_EXT ))
	{
		pglGenBuffersARB( 1, &tr.vbo_buffer );

		if( tr.vbo_buffer )
		{
			pglBindBufferARB( GL_ARRAY_BUFFER_ARB, tr.vbo_buffer );
			pglBufferDataARB( GL_ARRAY_BUFFER_ARB, numverts * sizeof( BrushVertex ), tr.vbo_buffer_data, GL_STATIC_DRAW_ARB );

			if( pglGetError() != GL_OUT_OF_MEMORY )
			{
				// VBO created succesfully!!!
				pglBindBufferARB( GL_ARRAY_BUFFER_ARB, 0 ); // remove binding
				ALERT( at_aiconsole, "Created VBO for world polys\n" );
			}
			else
			{
				pglBindBufferARB( GL_ARRAY_BUFFER_ARB, 0 ); // remove binding
				pglDeleteBuffersARB( 1, &tr.vbo_buffer );
				tr.vbo_buffer = 0;
				ALERT( at_error, "glBufferDataARB failed to upload data to VBO\n" );
			}
		}
		else
		{
			ALERT( at_error, "glGenBuffersARB failed to generate buffer index\n" );	
		}
	}
}

void EnableVertexArray( void )
{
	if( !tr.vbo_buffer_data )
		return;

	if( tr.vbo_buffer )
	{
		pglBindBufferARB( GL_ARRAY_BUFFER_ARB, tr.vbo_buffer );

		// initialize pointer for VBO
		pglVertexPointer( 3, GL_FLOAT, sizeof( BrushVertex ), OFFSET( BrushVertex, pos ));
		tr.use_vertex_array = 2;
	}
	else
	{
		// initialize pointer for vertex array
		pglVertexPointer( 3, GL_FLOAT, sizeof( BrushVertex ), &tr.vbo_buffer_data[0].pos );
		tr.use_vertex_array = 1;
	}

	pglEnableClientState( GL_VERTEX_ARRAY );	
}

void DisableVertexArray( void )
{
	if( !tr.use_vertex_array )
		return;

	if( tr.use_vertex_array == 2 )
		pglBindBufferARB( GL_ARRAY_BUFFER_ARB, 0 );

	pglDisableClientState( GL_VERTEX_ARRAY );
	GL_SetTexPointer( 0, TC_OFF );
	GL_SetTexPointer( 1, TC_OFF );
	GL_SetTexPointer( 2, TC_OFF );
	GL_SetTexPointer( 3, TC_OFF );
	tr.use_vertex_array = 0;
}

//================================
// Draws poly from vertex array or VBO
//================================
void DrawPolyFromArray( glpoly_t *p )
{
	int facedataindex = (p->flags >> 16) & 0xFFFF;
	BrushFace* ext = &faces_extradata[facedataindex];
	pglDrawArrays( GL_POLYGON, ext->start_vertex, p->numverts );	
}

void DrawPolyFromArray( msurface_t *s )
{
	DrawPolyFromArray( s->polys );
}

//
// Makes code more clear..
//
void SetTextureMatrix( int unit, float xscale, float yscale )
{
	GL_SelectTexture( unit );

	if( xscale && yscale )
	{
		matrix4x4	m;
		m.CreateScale( xscale, yscale, 1.0f );
		GL_LoadTexMatrix( m );
	}
	else GL_LoadIdentityTexMatrix(); // just reset texmatrix

	pglMatrixMode( GL_MODELVIEW );
}

void AddPolyToSpecularChain( msurface_t *s )
{
	material_t *pMat = s->texinfo->texture->material;

	if( pMat->gl_specular_id != tr.blackTexture )
	{
		s->texturechain = specularchain_both;
		specularchain_both = s;
	}
	else
	{
		s->texturechain = specularchain_low;
		specularchain_low = s;
	}
}

void AddPolyToTextureChain( msurface_t *s )
{
	needs_second_pass = TRUE;
	texture_t *tex = s->texinfo->texture;
	s->texturechain = tex->texturechain;
	tex->texturechain = s;
}

void AddPolyToSkyChain( msurface_t *s )
{
	s->texturechain = skychain;
	skychain = s;
}

 // used for additional bump passes, if hardware cant draw bump lighting in one pass
void AddPolyToLightmapChain( msurface_t *s )
{
	needs_special_bump_passes = TRUE;
	s->texturechain = lightmapchains[s->lightmaptexturenum];
	lightmapchains[s->lightmaptexturenum] = s;
}

//
// merges surface chains
//
void MergeChains( msurface_t *&dst, msurface_t *src )
{
	if( !dst )
	{
		dst = src;
		return;
	}

	msurface_t *last = dst;

	while( 1 )
	{
		if( !last->texturechain )
		{
			last->texturechain = src;
			break;
		}

		last = last->texturechain;
	}
}

//
// prepare texture matrix for converting camera position to
// tangent-space vector to it
//
void R_SetupTexMatrix_Reflected( msurface_t *s, const Vector &origin )
{
	int facedataindex = (s->polys->flags >> 16) & 0xFFFF;
	BrushFace* ext = &faces_extradata[facedataindex];

	matrix4x4	m;

	m[0][0] = ext->s_tangent[0];
	m[1][0] = ext->s_tangent[1];
	m[2][0] = ext->s_tangent[2];
	m[3][0] = -DotProduct( ext->s_tangent, origin );
	m[0][1] = ext->t_tangent[0];
	m[1][1] = ext->t_tangent[1];
	m[2][1] = ext->t_tangent[2];
	m[3][1] = -DotProduct( ext->t_tangent, origin );
	m[0][2] = -ext->normal[0];
	m[1][2] = -ext->normal[1];
	m[2][2] = -ext->normal[2];
	m[3][2] = DotProduct( ext->normal, origin );
	m[0][3] = m[1][3] = m[2][3] = 0.0f;
	m[3][3] = 1.0f;

	GL_SelectTexture( GL_TEXTURE0 );
	GL_LoadTexMatrix( m );
	pglMatrixMode( GL_MODELVIEW );
}

void R_SetupTexMatrix( msurface_t *s, const Vector &origin )
{
	int facedataindex = (s->polys->flags >> 16) & 0xFFFF;
	BrushFace* ext = &faces_extradata[facedataindex];

	matrix4x4	m;

	m[0][0] = -ext->s_tangent[0];
	m[1][0] = -ext->s_tangent[1];
	m[2][0] = -ext->s_tangent[2];
	m[3][0] = DotProduct( ext->s_tangent, origin );
	m[0][1] = -ext->t_tangent[0];
	m[1][1] = -ext->t_tangent[1];
	m[2][1] = -ext->t_tangent[2];
	m[3][1] = DotProduct( ext->t_tangent, origin );
	m[0][2] = -ext->normal[0];
	m[1][2] = -ext->normal[1];
	m[2][2] = -ext->normal[2];
	m[3][2] = DotProduct( ext->normal, origin );
	m[0][3] = m[1][3] = m[2][3] = 0.0f;
	m[3][3] = 1.0f;

	GL_SelectTexture( GL_TEXTURE0 );
	GL_LoadTexMatrix( m );
	pglMatrixMode( GL_MODELVIEW );
}

//================================
// Called from main renderer for each brush entity (including world)
// Allows us to clear lists, initialize counters, etc..
//================================
void PrepareFirstPass( void )
{
	pglDisable( GL_BLEND );
	pglDepthMask( GL_TRUE );
	pglDepthFunc( GL_LEQUAL );
	specularchain_low = NULL;
	specularchain_both = NULL;
	specularchain_high = NULL;
	skychain = NULL;

	needs_special_bump_passes = FALSE;
	needs_second_pass = FALSE;

	hasdynlights = HasDynamicLights();

	for( int i = 0; i < MAX_LIGHTMAPS; i++ )
		lightmapchains[i] = NULL;

	ResetCache();
	GL_SetTexPointer( 0, TC_LIGHTMAP );
	GL_SetTexPointer( 1, TC_TEXTURE );
	GL_SetTexPointer( 3, TC_LIGHTMAP );

	// in first pass, unit 2 can be either with lightmap, either detail texture

	// reset texture chains
	texture_t** tex = (texture_t**)worldmodel->textures;

	for( i = 0; i < worldmodel->numtextures; i++ )
		tex[i]->texturechain = NULL;
}

//================================
// HELPER FUNCTIONS, STATE CACHES, ETC...
//
//================================
/*
===============
R_TextureAnimation

Returns the proper texture for a given time and base texture
===============
*/
texture_t *R_TextureAnimation( texture_t *base, int surfacenum )
{
	int reletive;
	int count;

	// random tiling textures
	if( base->anim_total < 0 )
	{
		reletive = abs( surfacenum ) % abs( base->anim_total );

		count = 0;
		while( base->anim_min > reletive || base->anim_max <= reletive )
		{
			base = base->anim_next;
			if( !base ) HOST_ERROR( "R_TextureRandomTiling: broken loop\n" );
			if( ++count > 100 ) HOST_ERROR( "R_TextureRandomTiling: infinite loop\n" );
		}
		return base;
	}

	if( RI.currententity->curstate.frame )
	{
		if( base->alternate_anims )
			base = base->alternate_anims;
	}
	
	if( !base->anim_total )
		return base;

	reletive = (int)(GET_CLIENT_TIME() * 20) % base->anim_total;

	count = 0;	
	while( base->anim_min > reletive || base->anim_max <= reletive )
	{
		base = base->anim_next;
		if( !base ) HOST_ERROR( "R_TextureAnimation: broken loop\n" );
		if( ++count > 100 ) HOST_ERROR( "R_TextureAnimation: infinite loop\n" );
	}

	return base;
}

//============================
// Renders texture chains.
// Called from main renderer after all lighting rendered.
// Specular polys added to specular chain.
//============================
void RenderSecondPass( void )
{
	if( !needs_second_pass )
		return;

	pglEnable( GL_BLEND );
	pglDepthMask( GL_FALSE );
	pglDepthFunc( GL_EQUAL );
	pglBlendFunc( GL_DST_COLOR, GL_SRC_COLOR );

	GL_SetTexPointer( 0, TC_TEXTURE );
	GL_SetTexPointer( 1, TC_TEXTURE );

	texture_t **tex = (texture_t **)worldmodel->textures;

	for( int i = 0; i < worldmodel->numtextures; i++ )
	{
		if( tex[i]->texturechain )
		{
			msurface_t *s = tex[i]->texturechain;

			while( s )
			{
				texture_t	*t = R_TextureAnimation( s->texinfo->texture, s - worldmodel->surfaces );

				bool bDrawMirror = (RP_NORMALPASS() && s->flags & ( SURF_REFLECT )) ? true : false;

				if( bDrawMirror && SURF_INFO( s, RI.currentmodel )->mirrortexturenum )
				{
					GL_Bind( GL_TEXTURE0, SURF_INFO( s, RI.currentmodel )->mirrortexturenum );
					GL_SetTexPointer( 0, TC_OFF );
				}
				else
				{
					GL_Bind( GL_TEXTURE0, t->gl_texturenum );
					GL_SetTexPointer( 0, TC_TEXTURE );
                              	}

				if( bDrawMirror ) R_BeginDrawMirror( s, GL_TEXTURE0 );

				if( r_detailtextures->value && t->dt_texturenum )
				{
					float xScale, yScale;

					// scale for detail texture given from diffuse settings
					// so one detail texture can use different scale
					// on various diffuse textures
					GET_DETAIL_SCALE( t->gl_texturenum, &xScale, &yScale );

					// has detail texture
					GL_SetTexEnvs( ENVSTATE_REPLACE, ENVSTATE_MUL_X2 );
					SetTextureMatrix( GL_TEXTURE1, xScale, yScale );
					GL_Bind( GL_TEXTURE1, t->dt_texturenum );
				}
				else
				{
					GL_SetTexEnvs( ENVSTATE_REPLACE );
				}

				DrawPolyFromArray( s );

				msurface_t *next = s->texturechain;

				if( s->flags & SURF_SPECULAR )
					AddPolyToSpecularChain( s );

				if( bDrawMirror ) R_EndDrawMirror();

				s = next;	// move to next surface
			}
		}
	}

	SetTextureMatrix( GL_TEXTURE1, 0.0f, 0.0f );
}

void DrawNormalPoly( msurface_t *s )
{
	texture_t	*t = R_TextureAnimation( s->texinfo->texture, s - worldmodel->surfaces );
	bool bIgnoreDlights = false;
	bool bDrawCinematic = false;
	bool bDrawMirror = false;
	bool bDrawTrans = false;
	wpolynormal++;

	switch( RI.currententity->curstate.rendermode )
	{
	case kRenderTransTexture:
	case kRenderTransAdd:
	case kRenderGlow:
		bIgnoreDlights = true;
		bDrawTrans = true;
		break;
	}

	if( !bDrawTrans )
	{
		// bind lightmap - it will be drawn anyway
		if( worldmodel->lightdata )
			GL_Bind( GL_TEXTURE0, tr.baselightmap[s->lightmaptexturenum] );
		else GL_Bind( GL_TEXTURE0, tr.grayTexture );
	}

	if(( RI.currententity->curstate.effects & EF_SCREENMOVIE ) && ( s->flags & SURF_MOVIE ))
		bIgnoreDlights = bDrawCinematic = true;

	if( RP_NORMALPASS() && s->flags & ( SURF_REFLECT ))
		bDrawMirror = true;

	// draw only lightmap if scene contains dynamic lights
	if( hasdynlights && !bIgnoreDlights )
	{
		if( RI.currententity->curstate.rendermode == kRenderTransAlpha )
		{
			// drawing "solid" textures in multiple passes is a bit tricky -
			// need to render only lightmap, but with holes at that places,
			// where original texture has holes

			// YES, I KNOW, this is completely stupid to switch states when
			// rendering only one polygon!
			// I should render whole entity two times, instead of
			// rendering two times each poly, but i'm too lazy
			pglColorMask( GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE );
			GL_SetTexEnvs( ENVSTATE_REPLACE, ENVSTATE_REPLACE );
			GL_Bind( GL_TEXTURE1, t->gl_texturenum );
			DrawPolyFromArray( s );
			pglColorMask( GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE );
			pglDepthFunc( GL_EQUAL );
			GL_SetTexEnvs( ENVSTATE_REPLACE );
			DrawPolyFromArray( s );
			pglDepthFunc( GL_LEQUAL );
			AddPolyToTextureChain( s );
			return;
		}
		else
		{
			GL_SetTexEnvs( ENVSTATE_REPLACE );
		}

		DrawPolyFromArray( s );
		AddPolyToTextureChain( s );
		return;
	}

	if( r_detailtextures->value && t->dt_texturenum && !bDrawTrans )
	{
		// face has a detail texture
		if( glConfig.max_texture_units == 2 )
		{
			// draw only lightmap, texture and detail will be added by second pass
			GL_SetTexEnvs( ENVSTATE_REPLACE );
			DrawPolyFromArray( s );
			AddPolyToTextureChain( s );
			return;
		}

		float xScale, yScale;

		GL_SetTexEnvs( ENVSTATE_REPLACE, ENVSTATE_MUL_X2, ENVSTATE_MUL_X2 );

		// scale for detail texture given from diffuse settings
		// so one detail texture can use different scale
		// on various diffuse textures
		GET_DETAIL_SCALE( t->gl_texturenum, &xScale, &yScale );

		GL_SetTexPointer( 2, TC_TEXTURE );
		SetTextureMatrix( GL_TEXTURE2, xScale, yScale );

		if( bDrawCinematic ) R_DrawCinematic( s, t );
		else if( bDrawMirror && SURF_INFO( s, RI.currentmodel )->mirrortexturenum )
			GL_Bind( GL_TEXTURE1, SURF_INFO( s, RI.currentmodel )->mirrortexturenum );
		else GL_Bind( GL_TEXTURE1, t->gl_texturenum );
		GL_Bind( GL_TEXTURE2, t->dt_texturenum );		
	}
	else
	{
		if( bDrawTrans )
		{
			// no detail tex, just multiply lightmap and texture
			GL_SetTexEnvs( ENVSTATE_REPLACE );
			GL_Bind( GL_TEXTURE0, t->gl_texturenum );
		}
		else
		{
			// no detail tex, just multiply lightmap and texture
			GL_SetTexEnvs( ENVSTATE_REPLACE, ENVSTATE_MUL_X2 );

			if( bDrawCinematic ) R_DrawCinematic( s, t );
			else if( bDrawMirror && SURF_INFO( s, RI.currentmodel )->mirrortexturenum )
				GL_Bind( GL_TEXTURE1, SURF_INFO( s, RI.currentmodel )->mirrortexturenum );
			else GL_Bind( GL_TEXTURE1, t->gl_texturenum );
		}
	}

	if( bDrawMirror ) R_BeginDrawMirror( s, GL_TEXTURE1 );
	DrawPolyFromArray( s );
	if( bDrawMirror ) R_EndDrawMirror();

	if( s->flags & SURF_SPECULAR )
		AddPolyToSpecularChain( s );
}

void DrawBumpedPoly( msurface_t *s )
{
	texture_t	*t = R_TextureAnimation( s->texinfo->texture, s - worldmodel->surfaces );
	wpolybumped++;

	material_t *pMat = t->material;
	GL_Bind( GL_TEXTURE0, tr.lightvecs[s->lightmaptexturenum] );
	GL_Bind( GL_TEXTURE1, pMat->gl_normalmap_id );

	if( glConfig.max_texture_units >= 4 )
	{
		GL_SetTexEnvs( ENVSTATE_REPLACE, ENVSTATE_DOT, ENVSTATE_MUL, ENVSTATE_ADD );
		GL_SetTexPointer( 2, TC_LIGHTMAP );
		SetTextureMatrix( GL_TEXTURE2, 0.0f, 0.0f );
		GL_Bind( GL_TEXTURE2, tr.addlightmap[s->lightmaptexturenum] );
		GL_Bind( GL_TEXTURE3, tr.baselightmap[s->lightmaptexturenum] );
		AddPolyToTextureChain( s );
	}
	else if( glConfig.max_texture_units == 3 )
	{
		GL_SetTexEnvs( ENVSTATE_REPLACE, ENVSTATE_DOT, ENVSTATE_MUL );
		GL_SetTexPointer( 2, TC_LIGHTMAP );
		SetTextureMatrix( GL_TEXTURE2, 0.0f, 0.0f );
		GL_Bind( GL_TEXTURE2, tr.addlightmap[s->lightmaptexturenum] );
		AddPolyToLightmapChain( s );
	}
	else if( glConfig.max_texture_units == 2 )
	{
		GL_SetTexEnvs( ENVSTATE_REPLACE, ENVSTATE_DOT );
		AddPolyToLightmapChain( s );		
	}

	DrawPolyFromArray( s );
}

void DrawPolyFirstPass( msurface_t *s )
{
	if( s->flags & SURF_DRAWTURB ) // water
		return;

	if( s->flags & SURF_DRAWSKY )
	{
		AddPolyToSkyChain( s );
		return;
	}

	if( s->flags & SURF_BUMPDATA && cv_bump->value )
		DrawBumpedPoly( s );
	else DrawNormalPoly( s );

	wpolytotal++;
}

//============================
// Renders additional bump passes for hardware with <4 TU
// Called from main renderer before dynamic light will be added.
// Polys added to texture chain for second pass
//============================
void RenderAdditionalBumpPass( void )
{
	// reset texture matrix at 3rd TU after first pass
	SetTextureMatrix( GL_TEXTURE2, 0.0f, 0.0f );

	if( !needs_special_bump_passes )
		return;

	pglEnable(GL_BLEND);
	pglDepthMask(GL_FALSE);
	pglDepthFunc(GL_EQUAL);
	GL_SetTexEnvs( ENVSTATE_REPLACE );
	GL_SetTexPointer(0, TC_LIGHTMAP);

	if( glConfig.max_texture_units == 2 )
	{
		// multiply by diffuse light
		pglBlendFunc(GL_ZERO, GL_SRC_COLOR);
		for (int i = 0; i < MAX_LIGHTMAPS; i++)
		{
			msurface_t *surf = lightmapchains[i];
			if (surf)
			{
				GL_Bind( GL_TEXTURE0, tr.addlightmap[i] );
				while (surf)
				{
					DrawPolyFromArray(surf);
					surf = surf->texturechain;
				}
			}
		}
	}

	// add ambient light
	pglBlendFunc(GL_ONE, GL_ONE);
	for (int i = 0; i < MAX_LIGHTMAPS; i++)
	{
		msurface_t *surf = lightmapchains[i];
		if (surf)
		{
			GL_Bind( GL_TEXTURE0, tr.baselightmap[i] );
			while (surf)
			{
				DrawPolyFromArray(surf);
				msurface_t *next = surf->texturechain;
				AddPolyToTextureChain(surf);
				surf = next;
			}
		}
	}
}

void PrintSpecularSwitchedMsg( int mode )
{
	static int oldmode = 0;

	if( mode != oldmode )
	{
		ALERT( at_aiconsole, "Simple specular uses mode %d\n", mode );
		oldmode = mode;
	}
}

// specular modes are:
//
// 1 - combiners, 4 TU
// 2 - combiners, 2 TU, alpha (2 pass)
// 3 - ARB shader, 4 IU
// 4 - env_dot3, 2 TU, alpha (3 pass)
int ChooseSpecularMode( void )
{
	// try to use register combiners
	if( !cv_specular_nocombiners->value )
	{
		if( GL_Support( R_NV_COMBINE_EXT ) && glConfig.max_nv_combiners >= 2 )
		{
			if( glConfig.max_texture_units >= 4 && !cv_speculartwopass->value )
			{
				PrintSpecularSwitchedMsg( 1 );
				return 1;
			}
			else
			{
				PrintSpecularSwitchedMsg( 2 );
				return 2;
			}
		}
	}

	// try to use ARB shaders
	if( glConfig.max_texture_units >= 4 && cg.specular_shader && !cv_specular_noshaders->value )
	{
		PrintSpecularSwitchedMsg( 3 );
		return 3;
	}

	// last chance: try to use 3-pass method (only alpha buffer required)
	PrintSpecularSwitchedMsg( 4 );
	return 4;
}

//===========================
// Low quality specular
//
// TODO: make this shit code more clear! hate this combiners setup code..
//===========================
void DrawLowQualitySpecularChain( int mode )
{
	GL_SetTexPointer( 0, TC_VERTEX_POSITION ); // Use vertex position as texcoords to calc vector to camera
	GL_SetTexPointer( 1, TC_LIGHTMAP );

	GL_Bind( GL_TEXTURE0, tr.normalizeCubemap );

	// ===========================================================
	if( mode == 2 )
	{
		// Draw specular in two pass using register combiners
		GL_SetTexEnvs( ENVSTATE_REPLACE, ENVSTATE_REPLACE );
		pglEnable ( GL_REGISTER_COMBINERS_NV );
		pglCombinerParameteriNV ( GL_NUM_GENERAL_COMBINERS_NV, 2 );

		// First pass - write dot^4 to alpha

		// RC 0 setup:
		// spare0 = dot(tex0, tex1)
		pglCombinerInputNV( GL_COMBINER0_NV, GL_RGB, GL_VARIABLE_A_NV, GL_TEXTURE0_ARB, GL_EXPAND_NORMAL_NV, GL_RGB );
		pglCombinerInputNV( GL_COMBINER0_NV, GL_RGB, GL_VARIABLE_B_NV, GL_TEXTURE1_ARB, GL_EXPAND_NORMAL_NV, GL_RGB );
		pglCombinerOutputNV( GL_COMBINER0_NV, GL_RGB, GL_SPARE0_NV,	GL_DISCARD_NV, GL_DISCARD_NV,	0, 0, GL_TRUE, 0, 0 );

		// RC 1 setup:
		// spare0 = spare0 ^ 2
		pglCombinerInputNV( GL_COMBINER1_NV, GL_RGB, GL_VARIABLE_A_NV, GL_SPARE0_NV, GL_UNSIGNED_IDENTITY_NV, GL_RGB );
		pglCombinerInputNV( GL_COMBINER1_NV, GL_RGB, GL_VARIABLE_B_NV, GL_SPARE0_NV, GL_UNSIGNED_IDENTITY_NV, GL_RGB );
		pglCombinerOutputNV( GL_COMBINER1_NV, GL_RGB, GL_SPARE0_NV, GL_DISCARD_NV, GL_DISCARD_NV, 0, 0, 0, 0, 0 );

		// Final RC setup:
		// out.a = spare0.b
		pglFinalCombinerInputNV( GL_VARIABLE_A_NV, GL_ZERO, GL_UNSIGNED_IDENTITY_NV, GL_RGB );
		pglFinalCombinerInputNV( GL_VARIABLE_B_NV, GL_ZERO, GL_UNSIGNED_IDENTITY_NV, GL_RGB );
		pglFinalCombinerInputNV( GL_VARIABLE_C_NV, GL_ZERO, GL_UNSIGNED_IDENTITY_NV, GL_RGB );
		pglFinalCombinerInputNV( GL_VARIABLE_D_NV, GL_ZERO, GL_UNSIGNED_IDENTITY_NV, GL_RGB );
		pglFinalCombinerInputNV( GL_VARIABLE_E_NV, GL_ZERO, GL_UNSIGNED_IDENTITY_NV, GL_RGB );
		pglFinalCombinerInputNV( GL_VARIABLE_F_NV, GL_ZERO, GL_UNSIGNED_IDENTITY_NV, GL_RGB );
		pglFinalCombinerInputNV( GL_VARIABLE_G_NV, GL_SPARE0_NV, GL_UNSIGNED_IDENTITY_NV, GL_BLUE );

		pglBlendFunc( GL_SRC_ALPHA, GL_ZERO ); // multiply alpha by itself
		pglColorMask( GL_FALSE, GL_FALSE, GL_FALSE, GL_TRUE ); // render only alpha

		msurface_t *surf = specularchain_low;

		while( surf )
		{
			GL_Bind( GL_TEXTURE1, tr.lightvecs[surf->lightmaptexturenum] );
			R_SetupTexMatrix_Reflected( surf, tr.modelorg );
			DrawPolyFromArray( surf );
			surf = surf->texturechain;
		}

		// Second pass: add gloss map and specular color
		// reset texture matrix
		GL_SelectTexture( GL_TEXTURE0 );
		GL_LoadIdentityTexMatrix();
		pglMatrixMode( GL_MODELVIEW );

		pglBlendFunc( GL_DST_ALPHA, GL_ONE ); // alpha additive
		pglColorMask( GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE ); // render RGB
		GL_SetTexPointer( 0, TC_TEXTURE );

		pglCombinerParameteriNV( GL_NUM_GENERAL_COMBINERS_NV, 1 );

		// RC 0 setup:
		// spare0 = tex0 * tex1
		pglCombinerInputNV( GL_COMBINER0_NV, GL_RGB, GL_VARIABLE_A_NV, GL_TEXTURE0_ARB, GL_UNSIGNED_IDENTITY_NV, GL_RGB );
		pglCombinerInputNV( GL_COMBINER0_NV, GL_RGB, GL_VARIABLE_B_NV, GL_TEXTURE1_ARB, GL_UNSIGNED_IDENTITY_NV, GL_RGB );
		pglCombinerOutputNV( GL_COMBINER0_NV, GL_RGB, GL_SPARE0_NV, GL_DISCARD_NV, GL_DISCARD_NV, 0, 0, 0, 0, 0 );

		// Final RC setup:
		// out = spare0
		pglFinalCombinerInputNV( GL_VARIABLE_A_NV, GL_ZERO, GL_UNSIGNED_IDENTITY_NV, GL_RGB );
		pglFinalCombinerInputNV( GL_VARIABLE_B_NV, GL_ZERO, GL_UNSIGNED_IDENTITY_NV, GL_RGB );
		pglFinalCombinerInputNV( GL_VARIABLE_C_NV, GL_ZERO, GL_UNSIGNED_IDENTITY_NV, GL_RGB );
		pglFinalCombinerInputNV( GL_VARIABLE_D_NV, GL_SPARE0_NV, GL_UNSIGNED_IDENTITY_NV, GL_RGB );
		pglFinalCombinerInputNV( GL_VARIABLE_E_NV, GL_ZERO, GL_UNSIGNED_IDENTITY_NV, GL_RGB );
		pglFinalCombinerInputNV( GL_VARIABLE_F_NV, GL_ZERO, GL_UNSIGNED_IDENTITY_NV, GL_RGB );
		pglFinalCombinerInputNV( GL_VARIABLE_G_NV, GL_ZERO, GL_UNSIGNED_IDENTITY_NV, GL_ALPHA );

		surf = specularchain_low;

		while( surf )
		{
			material_t *pMat = surf->texinfo->texture->material;
			GL_Bind( GL_TEXTURE0, pMat->gl_specular_id );
			GL_Bind( GL_TEXTURE1, tr.addlightmap[surf->lightmaptexturenum] );
			DrawPolyFromArray( surf );
			surf = surf->texturechain;
			wpolyspecular++;
		}

		pglDisable( GL_REGISTER_COMBINERS_NV );
		return;
	}

	// ===========================================================
	if( mode == 4 )
	{
		// draw specular in three pass using only env_dot3

		// first pass - write dot^2 to alpha
		pglBlendFunc( GL_SRC_ALPHA, GL_ZERO ); // multiply alpha by itself
		pglColorMask( GL_FALSE, GL_FALSE, GL_FALSE, GL_TRUE ); // render only alpha
		GL_SetTexEnvs( ENVSTATE_REPLACE, ENVSTATE_DOT );

		msurface_t *surf = specularchain_low;

		while( surf )
		{
			GL_Bind( GL_TEXTURE1, tr.lightvecs[surf->lightmaptexturenum] );
			R_SetupTexMatrix_Reflected( surf, tr.modelorg );
			DrawPolyFromArray( surf );
			surf = surf->texturechain;
		}

		// Second pass: square alpha values in alpha buffer
	
		// reset texture matrix
		GL_SelectTexture( GL_TEXTURE0 );
		GL_LoadIdentityTexMatrix();
		pglMatrixMode( GL_MODELVIEW );

		pglBlendFunc( GL_ZERO, GL_DST_ALPHA );
		GL_SetTexEnvs( ENVSTATE_OFF ); // dont need textures at all
		GL_SetTexPointer( 0, TC_OFF );
		GL_SetTexPointer( 1, TC_OFF );

		surf = specularchain_low;

		while( surf )
		{
			DrawPolyFromArray( surf );
			surf = surf->texturechain;
		}

		// third pass: add gloss map and specular color
		pglBlendFunc( GL_DST_ALPHA, GL_ONE ); // alpha additive
		pglColorMask( GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE ); // render RGB
		GL_SetTexEnvs( ENVSTATE_REPLACE, ENVSTATE_MUL );
		GL_SetTexPointer( 0, TC_TEXTURE );
		GL_SetTexPointer( 1, TC_LIGHTMAP );

		surf = specularchain_low;

		while( surf )
		{
			material_t *pMat = surf->texinfo->texture->material;
			GL_Bind( GL_TEXTURE0, pMat->gl_specular_id );
			GL_Bind( GL_TEXTURE1, tr.addlightmap[surf->lightmaptexturenum] );
			DrawPolyFromArray( surf );
			surf = surf->texturechain;
			wpolyspecular++;
		}
		return;
	}

	// ===========================================================
	if( mode == 1 || mode == 3 )
	{
		// Draw specular in one pass
		GL_SetTexEnvs( ENVSTATE_REPLACE, ENVSTATE_REPLACE, ENVSTATE_REPLACE, ENVSTATE_REPLACE );

		// texture 0 - normalization cubemap / normal map (for HQ specular)
		// texture 1 - lightmap with light vectors
		// texture 2 - lightmap with specular color (diffuse light)
		// texture 3 - gloss map

		GL_SetTexPointer( 2, TC_LIGHTMAP );
		GL_SetTexPointer( 3, TC_TEXTURE );

		pglBlendFunc( GL_ONE, GL_ONE );

		if( mode == 1 )
		{
			// setup register combiners	
			pglEnable( GL_REGISTER_COMBINERS_NV );
			pglCombinerParameteriNV( GL_NUM_GENERAL_COMBINERS_NV, 2 );

			// RC 0 setup:
			// spare0 = dot(tex0, tex1)
			pglCombinerInputNV( GL_COMBINER0_NV, GL_RGB, GL_VARIABLE_A_NV, GL_TEXTURE0_ARB, GL_EXPAND_NORMAL_NV, GL_RGB );
			pglCombinerInputNV( GL_COMBINER0_NV, GL_RGB, GL_VARIABLE_B_NV, GL_TEXTURE1_ARB, GL_EXPAND_NORMAL_NV, GL_RGB );
			pglCombinerOutputNV( GL_COMBINER0_NV, GL_RGB, GL_SPARE0_NV, GL_DISCARD_NV, GL_DISCARD_NV, 0, 0, GL_TRUE, 0, 0 );

			// RC 1 setup:
			// spare0 = spare0 ^ 2
			// spare1 = tex2 * tex3
			pglCombinerInputNV( GL_COMBINER1_NV, GL_RGB, GL_VARIABLE_A_NV, GL_SPARE0_NV, GL_UNSIGNED_IDENTITY_NV, GL_RGB );
			pglCombinerInputNV( GL_COMBINER1_NV, GL_RGB, GL_VARIABLE_B_NV, GL_SPARE0_NV, GL_UNSIGNED_IDENTITY_NV, GL_RGB );
			pglCombinerInputNV( GL_COMBINER1_NV, GL_RGB, GL_VARIABLE_C_NV, GL_TEXTURE2_ARB, GL_UNSIGNED_IDENTITY_NV, GL_RGB );
			pglCombinerInputNV( GL_COMBINER1_NV, GL_RGB, GL_VARIABLE_D_NV, GL_TEXTURE3_ARB, GL_UNSIGNED_IDENTITY_NV, GL_RGB );
			pglCombinerOutputNV( GL_COMBINER1_NV, GL_RGB, GL_SPARE0_NV, GL_SPARE1_NV, GL_DISCARD_NV, 0, 0, 0, 0, 0 );

			// Final RC setup:
			// out = (spare0 ^ 2) * spare1
			pglFinalCombinerInputNV( GL_VARIABLE_A_NV, GL_E_TIMES_F_NV, GL_UNSIGNED_IDENTITY_NV, GL_RGB );
			pglFinalCombinerInputNV( GL_VARIABLE_B_NV, GL_SPARE1_NV, GL_UNSIGNED_IDENTITY_NV, GL_RGB );
			pglFinalCombinerInputNV( GL_VARIABLE_C_NV, GL_ZERO, GL_UNSIGNED_IDENTITY_NV, GL_RGB );
			pglFinalCombinerInputNV( GL_VARIABLE_D_NV, GL_ZERO, GL_UNSIGNED_IDENTITY_NV, GL_RGB );
			pglFinalCombinerInputNV( GL_VARIABLE_E_NV, GL_SPARE0_NV, GL_UNSIGNED_IDENTITY_NV, GL_RGB );
			pglFinalCombinerInputNV( GL_VARIABLE_F_NV, GL_SPARE0_NV, GL_UNSIGNED_IDENTITY_NV, GL_RGB );
			pglFinalCombinerInputNV( GL_VARIABLE_G_NV, GL_ZERO, GL_UNSIGNED_INVERT_NV, GL_ALPHA );
		}
		else
		{
			// setup ARB shader
			pglEnable( GL_FRAGMENT_PROGRAM_ARB );
			pglBindProgramARB( GL_FRAGMENT_PROGRAM_ARB, cg.specular_shader );
		}

		msurface_t *surf = specularchain_low;

		while( surf )
		{
			material_t *pMat = surf->texinfo->texture->material;

			GL_Bind( GL_TEXTURE1, tr.lightvecs[surf->lightmaptexturenum] );
			GL_Bind( GL_TEXTURE2, tr.addlightmap[surf->lightmaptexturenum] );
			GL_Bind( GL_TEXTURE3, pMat->gl_specular_id );
			R_SetupTexMatrix_Reflected( surf, tr.modelorg );
			DrawPolyFromArray( surf );
			surf = surf->texturechain;
			wpolyspecular++;
		}

		GL_SelectTexture( GL_TEXTURE0 );

		if( mode == 1 ) pglDisable( GL_REGISTER_COMBINERS_NV );
		else pglDisable( GL_FRAGMENT_PROGRAM_ARB );
	}
}

//===========================
// High quality specular
//===========================
void DrawHighQualitySpecularChain( void )
{
	// setup texture coords for shader..
	GL_SetTexPointer( 0, TC_VERTEX_POSITION ); // Use vertex position as texcoords to calc vector to camera
	GL_SetTexPointer( 1, TC_LIGHTMAP );
	GL_SetTexPointer( 2, TC_OFF );
	GL_SetTexPointer( 3, TC_TEXTURE );

	pglBlendFunc( GL_ONE, GL_ONE );
	GL_SetTexEnvs( ENVSTATE_REPLACE, ENVSTATE_REPLACE, ENVSTATE_REPLACE, ENVSTATE_REPLACE );

	pglEnable( GL_FRAGMENT_PROGRAM_ARB );
	pglBindProgramARB( GL_FRAGMENT_PROGRAM_ARB, cg.specular_high_shader );

	msurface_t* surf = specularchain_high;

	while( surf )
	{
		material_t *pMat = surf->texinfo->texture->material;

		GL_Bind( GL_TEXTURE0, pMat->gl_normalmap_id );
		GL_Bind( GL_TEXTURE1, tr.lightvecs[surf->lightmaptexturenum] );
		GL_Bind( GL_TEXTURE2, tr.addlightmap[surf->lightmaptexturenum] );
		GL_Bind( GL_TEXTURE3, pMat->gl_specular_id );
		R_SetupTexMatrix( surf, tr.modelorg );
		DrawPolyFromArray( surf );
		surf = surf->texturechain;
		wpolyspecular_hi++;
	}

	pglDisable( GL_FRAGMENT_PROGRAM_ARB );
}

//===========================
// Specular rendering
//===========================
void RenderSpecular( void )
{
	if( !cv_specular->value )
		return;

	// load low quality specular
	int mode = ChooseSpecularMode();

	if( !mode )
	{
		CVAR_SET_FLOAT( "gl_specular", 0 );
		return;
	}

	// load high quality specular
	if( cv_highspecular->value )
	{
		if( !cg.specular_high_shader || glConfig.max_texture_units < 4 )
		{
			ALERT( at_error, "High quality specular is not supported by your videocard\n" );
			CVAR_SET_FLOAT( "gl_highspecular", 0 );
		}
	}

	if( !specularchain_low && !specularchain_high && !specularchain_both )
		return;

	// surfaces that can be drawn with low and high quality specular
	// needs to be attached to one of those specular chains
	if( specularchain_both )
	{
		if( cv_highspecular->value )
			MergeChains( specularchain_high, specularchain_both );
		else MergeChains( specularchain_low, specularchain_both );
	}

	pglEnable( GL_BLEND );	
	pglDepthMask( GL_FALSE );
	pglDepthFunc( GL_EQUAL );

	if( specularchain_low )
		DrawLowQualitySpecularChain( mode );

	if( specularchain_high && cv_highspecular->value )
		DrawHighQualitySpecularChain();

	// reset texture matrix
	GL_SelectTexture( GL_TEXTURE0 );
	GL_LoadIdentityTexMatrix();
	pglMatrixMode( GL_MODELVIEW );

	pglDisable( GL_BLEND );
	pglDepthMask( GL_TRUE );
}

void R_DrawBrushModel( cl_entity_t *e, qboolean onlyfirstpass )
{
	Vector		mins, maxs;
	msurface_t	*psurf;
	model_t		*clmodel;
	qboolean		rotated;
	Vector		trans;

	if( r_newrenderer->value )
		return;

	clmodel = e->model;

	// don't reflect this entity in mirrors
	if( e->curstate.effects & EF_NOREFLECT && RI.params & RP_MIRRORVIEW )
		return;

	// draw only in mirrors
	if( e->curstate.effects & EF_REFLECTONLY && !( RI.params & RP_MIRRORVIEW ))
		return;

	// skybox entity
	if( e->curstate.renderfx == SKYBOX_ENTITY )
	{
		trans = RI.vieworg - tr.sky_origin;

		if( tr.sky_speed )
		{
			trans = trans - (RI.vieworg - tr.sky_world_origin) / tr.sky_speed;
			Vector skypos = tr.sky_origin + (RI.vieworg - tr.sky_world_origin) / tr.sky_speed;
			tr.modelorg = skypos - e->origin;
		}
		else
		{
			tr.modelorg = tr.sky_origin - e->origin;
                    }

		if( e->angles != g_vecZero )
		{
			for( int i = 0; i < 3; i++ )
			{
				mins[i] = e->origin[i] + trans[i] - clmodel->radius;
				maxs[i] = e->origin[i] + trans[i] + clmodel->radius;
			}
			rotated = true;
		}
		else
		{
			mins = e->origin + trans + clmodel->mins;
			maxs = e->origin + trans + clmodel->maxs;
			rotated = false;
		}

		if( R_CullBox( mins, maxs, RI.clipFlags ))
			return;

		// compute modelview
		RI.objectMatrix = matrix4x4( e->origin + trans, g_vecZero, 1.0f );
		RI.modelviewMatrix = RI.worldviewMatrix.ConcatTransforms( RI.objectMatrix );

		pglMatrixMode( GL_MODELVIEW );
		GL_LoadMatrix( RI.modelviewMatrix );
		tr.modelviewIdentity = false;

		pglDepthRange( 0.8f, 0.9f );		
	}
	else
	{
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
	}

	if( !onlyfirstpass )
	{
		EnableVertexArray();
		PrepareFirstPass();	
          }

	if( e->curstate.rendermode == kRenderTransAlpha )
	{
		pglEnable( GL_ALPHA_TEST );
		pglAlphaFunc( GL_GREATER, 0.25f );
	}
	else if( e->curstate.rendermode == kRenderTransTexture )
	{
		pglDepthMask( GL_FALSE );
		pglEnable( GL_BLEND );
		pglBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
		pglColor4ub( 255, 255, 255, e->curstate.renderamt );
	}
	else if( e->curstate.rendermode == kRenderTransAdd )
	{
		pglDepthMask( GL_FALSE );
		pglEnable( GL_BLEND );
		pglBlendFunc( GL_ONE, GL_ONE );
		pglColor4ub( 255, 255, 255, e->curstate.renderamt );
	}	

	// accumulate surfaces, build the lightmaps
	psurf = &clmodel->surfaces[clmodel->firstmodelsurface];
	for( int i = 0; i < clmodel->nummodelsurfaces; i++, psurf++ )
	{
		if( R_CullSurface( psurf, 0 ))
			continue;

		psurf->visframe = tr.framecount;
		DrawPolyFirstPass( psurf );
	}

	if( e->curstate.rendermode == kRenderTransAlpha )
	{
		pglDisable( GL_ALPHA_TEST );
	}
	else if( e->curstate.rendermode == kRenderTransTexture || e->curstate.rendermode == kRenderTransAdd )
	{
		pglDepthMask( GL_TRUE );
		pglDisable( GL_BLEND );
	}

	if( !onlyfirstpass )
	{
		RenderAdditionalBumpPass();

		if( e->curstate.renderfx != SKYBOX_ENTITY && cv_dynamiclight->value )
			DrawDynamicLightForEntity( e, mins, maxs );

		RenderSecondPass();
		RenderSpecular();
		DisableVertexArray();
		DrawDecals();
		ResetRenderState();
		R_LoadIdentity();
	}

	if( e->curstate.renderfx == SKYBOX_ENTITY )
	{
		// restore normal depth range
		pglDepthRange( gldepthmin, gldepthmax );
	}
}

/*
=================
R_DrawWaterBrushModel
=================
*/
void R_DrawWaterBrushModel( cl_entity_t *e )
{
	int		i, num_surfaces;
	Vector		mins, maxs;
	msurface_t	*psurf;
	model_t		*clmodel;
	qboolean		rotated;

	if( r_newrenderer->value )
		return;

	// no water in skybox
	if( !glsl.pWaterShader || e->curstate.renderfx == SKYBOX_ENTITY )
		return;

	if( RI.currententity == tr.mirror_entity )
		return;

	clmodel = e->model;

	// don't reflect this entity in mirrors
	if( e->curstate.effects & EF_NOREFLECT && RI.params & RP_MIRRORVIEW )
		return;

	// draw only in mirrors
	if( e->curstate.effects & EF_REFLECTONLY && !( RI.params & RP_MIRRORVIEW ))
		return;

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

	if( rotated ) R_RotateForEntity( e );
	else R_TranslateForEntity( e );

	e->visframe = tr.framecount; // visible

	if( rotated ) tr.modelorg = RI.objectMatrix.VectorITransform( RI.vieworg );
	else tr.modelorg = RI.vieworg - e->origin;

	float r, g, b, a, scale, rtw, rth, angle, dir[2], speed = 0.0f;

	angle = anglemod( RI.currententity->curstate.colormap );
	dir[0] = sinf( angle / 180.0f * M_PI );
	dir[1] = cosf( angle / 180.0f * M_PI );
		
	if( RI.currententity->curstate.frame )
		speed = 1.0f / RI.currententity->curstate.frame * 64.0f;
	scale = RI.currententity->curstate.scale * 8.0f * ( RI.viewport[2] / 640.0f );
		
	r = RI.currententity->curstate.rendercolor.r / 255.0f;
	g = RI.currententity->curstate.rendercolor.g / 255.0f;
	b = RI.currententity->curstate.rendercolor.b / 255.0f;
		
	if( RI.vieworg[2] < RI.currentmodel->maxs[2] || RI.currententity->curstate.renderamt <= 0.0f || !r_allow_mirrors->value )
		a = 2.0;
	else
		a = ( ( 255.0f - RI.currententity->curstate.renderamt ) / 255.0f ) * 2.0f - 1.0f;
		
	if( GL_Support( R_ARB_TEXTURE_NPOT_EXT ))
	{
		// allow screen size
		rtw = bound( 96, RI.viewport[2], 1024 );
		rth = bound( 72, RI.viewport[3], 768 );
	}
	else
	{
		rtw = NearestPOW( RI.viewport[2], true );
		rth = NearestPOW( RI.viewport[3], true );
		rtw = bound( 128, RI.viewport[2], 1024 );
		rth = bound( 64, RI.viewport[3], 512 );
	}

	GL_Bind( GL_TEXTURE2, tr.refractionTexture );

	if( tr.scrcpywaterframe != tr.framecount )
	{
		tr.scrcpywaterframe = tr.framecount;
		pglCopyTexSubImage2D( GL_TEXTURE_RECTANGLE_NV, 0, 0, 0, 0, 0, glState.width, glState.height );
	}

	GL_Cull( GL_NONE );
	glsl.pWaterShader->Bind();
	glsl.pWaterShader->SetParameter4f( 0, RI.vieworg[0], RI.vieworg[1], RI.vieworg[2], 0.0f );
	glsl.pWaterShader->SetParameter4f( 1, rtw, rth, scale, 0.0f );
	glsl.pWaterShader->SetParameter4f( 2, r, g, b, a );

	psurf = &clmodel->surfaces[clmodel->firstmodelsurface];
	for( num_surfaces = i = 0; i < clmodel->nummodelsurfaces; i++, psurf++ )
	{
		if( !( psurf->flags & SURF_DRAWTURB ) || R_CullSurface( psurf, 0 ) )
			continue;

		mextrasurf_t *es = SURF_INFO( psurf, RI.currentmodel );
		msurfmesh_t *mesh = es->mesh;

		GL_Bind( GL_TEXTURE0, es->mirrortexturenum );
		GL_Bind( GL_TEXTURE1, tr.waterTextures[(int)( RI.refdef.time * WATER_ANIMTIME ) % WATER_TEXTURES] );

		pglBegin( GL_POLYGON );

		for( int i = 0; mesh != NULL && i < mesh->numVerts; i++ )
		{
			glvert_t	*v = &mesh->verts[i];

			if( speed )
			{
				float s = v->stcoord[0] + RI.refdef.time / speed * dir[0];
				float t = v->stcoord[1] + RI.refdef.time / speed * dir[1];
				pglTexCoord2f( s, t );
			}
			else pglTexCoord2f( v->stcoord[0], v->stcoord[1] );

			pglVertex3fv( v->vertex );
		}

		pglEnd();
	}

	glsl.pWaterShader->Unbind();
	GL_SelectTexture( 2 );
	GL_CleanUpTextureUnits( 0 );

	R_LoadIdentity();
}

/*
=================
R_DrawGlassBrushModel
=================
*/
void R_DrawGlassBrushModel( cl_entity_t *e )
{
	int		i, num_surfaces;
	vec3_t		mins, maxs;
	msurface_t	*psurf;
	model_t		*clmodel;
	qboolean		rotated;

	if( r_newrenderer->value )
		return;

	// no glass in skybox
	if( !glsl.pGlassShader || e->curstate.renderfx == SKYBOX_ENTITY )
		return;

	clmodel = e->model;

	// don't reflect this entity in mirrors
	if( e->curstate.effects & EF_NOREFLECT && RI.params & RP_MIRRORVIEW )
		return;

	// draw only in mirrors
	if( e->curstate.effects & EF_REFLECTONLY && !( RI.params & RP_MIRRORVIEW ))
		return;

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

	if( rotated ) R_RotateForEntity( e );
	else R_TranslateForEntity( e );

	e->visframe = tr.framecount; // visible

	if( rotated ) tr.modelorg = RI.objectMatrix.VectorITransform( RI.vieworg );
	else tr.modelorg = RI.vieworg - e->origin;

	GL_Bind( GL_TEXTURE2, tr.refractionTexture );

	if( tr.scrcpyframe != tr.framecount )
	{
		tr.scrcpyframe = tr.framecount;
		pglCopyTexSubImage2D( GL_TEXTURE_RECTANGLE_NV, 0, 0, 0, 0, 0, glState.width, glState.height );
	}

	glsl.pGlassShader->Bind();
	glsl.pGlassShader->SetParameter4f( 0, RI.currententity->curstate.scale * ( RI.viewport[2] / 640.0f ), 0.0f, 0.0f, 0.0f );
	psurf = &clmodel->surfaces[clmodel->firstmodelsurface];

	ResetCache();
	EnableVertexArray();
	GL_SetTexPointer( 0, TC_TEXTURE );
	GL_SetTexPointer( 1, TC_OFF );
	GL_SetTexPointer( 2, TC_OFF );

	for( num_surfaces = i = 0; i < clmodel->nummodelsurfaces; i++, psurf++ )
	{
		if( R_CullSurface( psurf, 0 ))
			continue;
			
		texture_t *t = R_TextureAnimation( psurf->texinfo->texture, psurf - RI.currententity->model->surfaces );

		psurf->visframe = tr.framecount;

		GL_Bind( GL_TEXTURE0, t->gl_texturenum );
		GL_Bind( GL_TEXTURE1, t->material->gl_normalmap_id );

		DrawPolyFromArray( psurf );
	}

	DisableVertexArray();
	glsl.pGlassShader->Unbind();
	DrawDecals();
	ResetRenderState();

	R_LoadIdentity();
}

/*
=============================================================

	WORLD MODEL

=============================================================
*/
/*
================
R_RecursiveWorldNode
================
*/
void R_RecursiveWorldNode( mnode_t *node, const mplane_t frustum[6], unsigned int clipflags )
{
	const mplane_t	*clipplane;
	int		i, clipped;
	msurface_t	*surf, **mark;
	mleaf_t		*pleaf;
	int		c, side;
	double		dot;

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

		// deal with model fragments in this leaf
		if( pleaf->efrags )
			STORE_EFRAGS( &pleaf->efrags, tr.framecount );
		return;
	}

	// node is just a decision point, so go down the apropriate sides

	// find which side of the node we are on
	dot = PlaneDiff( tr.modelorg, node->plane );
	side = (dot >= 0.0f) ? 0 : 1;

	// recurse down the children, front side first
	R_RecursiveWorldNode( node->children[side], frustum, clipflags );

	// draw stuff
	for( c = node->numsurfaces, surf = worldmodel->surfaces + node->firstsurface; c; c--, surf++ )
	{
		if( R_CullSurfaceExt( surf, frustum, clipflags ))
			continue;

		if( r_newrenderer->value )
			R_AddSurfaceToDrawList( surf );
		else DrawPolyFirstPass( surf );
	}

	// recurse down the back side
	R_RecursiveWorldNode( node->children[!side], frustum, clipflags );
}

/*
=================
R_DrawStaticBrushes

=================
*/
void R_DrawStaticBrushes( void )
{
	// draw static entities
	for( int i = 0; i < tr.num_static_entities; i++ )
	{
		RI.currententity = tr.static_entities[i];
		RI.currentmodel = RI.currententity->model;
	
		ASSERT( RI.currententity != NULL );
		ASSERT( RI.currententity->model != NULL );

		switch( RI.currententity->model->type )
		{
		case mod_brush:
			R_DrawBrushModel( RI.currententity, TRUE );
			break;
		default:
			HOST_ERROR( "R_DrawStatics: non bsp model in static list!\n" );
			break;
		}
	}
}

/*
=============
R_DrawWorld
=============
*/
void R_DrawWorld( void )
{
	RI.currententity = GET_ENTITY( 0 );
	RI.currentmodel = RI.currententity->model;

	if( RI.refdef.onlyClientDraw )
		return;

	tr.modelorg = RI.vieworg;

	R_LoadIdentity();
	EnableVertexArray();
	PrepareFirstPass();

	R_RecursiveWorldNode( worldmodel->nodes, RI.frustum, RI.clipFlags );

	R_DrawStaticBrushes();

	RenderAdditionalBumpPass();

	if( cv_dynamiclight->value )
		DrawDynamicLights();

	RenderSecondPass();
	RenderSpecular();

	if( skychain )
		RI.params |= RP_SKYVISIBLE;

	pglDepthFunc( GL_LEQUAL );

	DisableVertexArray();
	DrawDecals();
	ResetRenderState();

	// clear texture chains
	texture_t** tex = (texture_t **)worldmodel->textures;
	for( int i = 0; i < worldmodel->numtextures; i++ )
		tex[i]->texturechain = NULL;

	DrawSky();
}

/*
===============
R_MarkLeaves

Mark the leaves and nodes that are in the PVS for the current leaf
===============
*/
void R_MarkLeaves( void )
{
	byte	*vis;
	mnode_t	*node;
	int	i;

	gDecalsRendered = 0;

	if( !RI.drawWorld ) return;

	if( r_novis->value || tr.fResetVis )
	{
		// force recalc viewleaf
		tr.fResetVis = false;
		r_viewleaf = NULL;
	}

	if( r_viewleaf == r_oldviewleaf && r_viewleaf2 == r_oldviewleaf2 && !r_novis->value && r_viewleaf != NULL )
		return;

	// development aid to let you run around
	// and see exactly where the pvs ends
	if( r_lockpvs->value ) return;

	if( !FBitSet( RI.params, RP_MERGEVISIBILITY ))
		tr.visframecount++;

	r_oldviewleaf = r_viewleaf;
	r_oldviewleaf2 = r_viewleaf2;

	int longs = ( worldmodel->numleafs + 31 ) >> 5;
		
	if( r_novis->value || !r_viewleaf || !worldmodel->visdata )
	{
		// mark everything
		for( i = 0; i < worldmodel->numleafs; i++ )
			worldmodel->leafs[i+1].visframe = tr.visframecount;
		for( i = 0; i < worldmodel->numnodes; i++ )
			worldmodel->nodes[i].visframe = tr.visframecount;
		memset( RI.visbytes, 0xFF, longs << 2 );
		return;
	}

	if( RI.params & RP_MERGEVISIBILITY )
	{
		// merge main visibility with additional pass
		vis = Mod_LeafPVS( r_viewleaf, worldmodel );

		for( i = 0; i < longs; i++ )
			((int *)RI.visbytes)[i] |= ((int *)vis)[i];
	}
	else
	{
		// set primary vis info
		memcpy( RI.visbytes, Mod_LeafPVS( r_viewleaf, worldmodel ), longs << 2 );
          }

	// may have to combine two clusters
	// because of solid water boundaries
	if( r_viewleaf != r_viewleaf2 )
	{
		vis = Mod_LeafPVS( r_viewleaf2, worldmodel );

		for( i = 0; i < longs; i++ )
			((int *)RI.visbytes)[i] |= ((int *)vis)[i];
	}

	for( i = 0; i < worldmodel->numleafs; i++ )
	{
		if( RI.visbytes[i>>3] & ( 1<<( i & 7 )))
		{
			node = (mnode_t *)&worldmodel->leafs[i+1];
			do
			{
				if( node->visframe == tr.visframecount )
					break;
				node->visframe = tr.visframecount;
				node = node->parent;
			} while( node );
		}
	}
}

/*
==================
GL_BuildLightmaps

Builds the lightmap texture on a new level
or when gamma is changed
==================
*/
void HUD_BuildLightmaps( void )
{
	if( !g_fRenderInitialized ) return;

	// set the worldmodel
	worldmodel = GET_ENTITY( 0 )->model;
	if( !worldmodel ) return; // wait for worldmodel

	if( RENDER_GET_PARM( PARM_WORLD_VERSION, 0 ) == 31 )
		tr.lm_sample_size = 8;
	else tr.lm_sample_size = 16;

	if( RENDER_GET_PARM( PARM_FEATURES, 0 ) & ENGINE_LARGE_LIGHTMAPS )
		glConfig.block_size = BLOCK_SIZE_MAX;
	else glConfig.block_size = BLOCK_SIZE_DEFAULT;

	UpdateWorldExtradata( worldmodel->name );

	UpdateLightmaps();
}