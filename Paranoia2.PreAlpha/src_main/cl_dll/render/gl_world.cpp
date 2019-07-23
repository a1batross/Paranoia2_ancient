/*
gl_world.cpp - world and bmodel rendering
Copyright (C) 2014 Uncle Mike

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
#include "gl_local.h"
#include "gl_decals.h"
#include <mathlib.h>
#include "gl_studio.h"

unsigned int tempElems[MAX_MAP_VERTS];
unsigned int numTempElems;

#define QSORT_MAX_STACKDEPTH		2048

#define R_FaceCopy( in, out ) \
	( \
	( out ).surface = ( in ).surface, \
	( out ).hProgram = ( in ).hProgram, \
	( out ).parent = ( in ).parent, \
	( out ).dist = ( in ).dist \
	)

#define R_FaceCmp( mb1, mb2 ) \
	( \
	( mb1 ).hProgram > ( mb2 ).hProgram ? true : \
	( mb1 ).hProgram < ( mb2 ).hProgram ? false : \
	( mb1 ).surface->texinfo->texture->gl_texturenum > ( mb2 ).surface->texinfo->texture->gl_texturenum ? true : \
	( mb1 ).surface->texinfo->texture->gl_texturenum < ( mb2 ).surface->texinfo->texture->gl_texturenum ? false : \
	( mb1 ).surface->lightmaptexturenum > ( mb2 ).surface->lightmaptexturenum ? true : \
	( mb1 ).surface->lightmaptexturenum < ( mb2 ).surface->lightmaptexturenum ? false : \
	( mb1 ).parent->hCachedMatrix > ( mb2 ).parent->hCachedMatrix ? true : \
	( mb1 ).parent->hCachedMatrix < ( mb2 ).parent->hCachedMatrix \
	)

/*
================
R_QSortDrawFaces

Quicksort
================
*/
static void R_QSortDrawFaces( gl_bmodelface_t *faces, int Li, int Ri )
{
	int li, ri, stackdepth = 0, total = Ri + 1;
	int lstack[QSORT_MAX_STACKDEPTH], rstack[QSORT_MAX_STACKDEPTH];
	gl_bmodelface_t median, tempbuf;

mark0:
	if( Ri - Li > 8 )
	{
		li = Li;
		ri = Ri;

		R_FaceCopy( faces[( Li+Ri ) >> 1], median );

		if( R_FaceCmp( faces[Li], median ) )
		{
			if( R_FaceCmp( faces[Ri], faces[Li] ) )
				R_FaceCopy( faces[Li], median );
		}
		else if( R_FaceCmp( median, faces[Ri] ) )
		{
			R_FaceCopy( faces[Ri], median );
		}

		do
		{
			while( R_FaceCmp( median, faces[li] ) ) li++;
			while( R_FaceCmp( faces[ri], median ) ) ri--;

			if( li <= ri )
			{
				R_FaceCopy( faces[ri], tempbuf );
				R_FaceCopy( faces[li], faces[ri] );
				R_FaceCopy( tempbuf, faces[li] );

				li++;
				ri--;
			}
		}
		while( li < ri );

		if( ( Li < ri ) && ( stackdepth < QSORT_MAX_STACKDEPTH ) )
		{
			lstack[stackdepth] = li;
			rstack[stackdepth] = Ri;
			stackdepth++;
			li = Li;
			Ri = ri;
			goto mark0;
		}

		if( li < Ri )
		{
			Li = li;
			goto mark0;
		}
	}
	if( stackdepth )
	{
		--stackdepth;
		Ri = ri = rstack[stackdepth];
		Li = li = lstack[stackdepth];
		goto mark0;
	}

	for( li = 1; li < total; li++ )
	{
		R_FaceCopy( faces[li], tempbuf );
		ri = li - 1;

		while( ( ri >= 0 ) && ( R_FaceCmp( faces[ri], tempbuf ) ) )
		{
			R_FaceCopy( faces[ri], faces[ri+1] );
			ri--;
		}
		if( li != ri+1 )
			R_FaceCopy( tempbuf, faces[ri+1] );
	}
}

/*
===============
CreateWorldMeshCache

Single VBO for all map surfaces
===============
*/
void CreateWorldMeshCache( void )
{
	if( !FBitSet( RENDER_GET_PARM( PARM_FEATURES, 0 ), ENGINE_BUILD_SURFMESHES ))
	{
		ALERT( at_error, "world has no surfmeshes\n" );
		return;
	}

	if( tr.world_vbo ) return;	// already created

	// calculate number of used faces and vertexes
	msurface_t *surf = worldmodel->surfaces;
	int i, j, curVert = 0, numVerts = 0;

	// count vertices
	for( i = 0; i < worldmodel->numsurfaces; i++, surf++ )
	{
		if( FBitSet( surf->flags, SURF_DRAWSKY ))
			continue;

		numVerts += ( SURF_INFO( surf, worldmodel )->mesh->numVerts );
	}

	// temporary array will be removed at end of this function
	bvert_t *arrayverts = (bvert_t *)Mem_Alloc( sizeof( bvert_t ) * numVerts );
	surf = worldmodel->surfaces;

	// create VBO-optimized vertex array (single for world and all brush-models)
	for( i = 0; i < worldmodel->numsurfaces; i++, surf++ )
	{
		mextrasurf_t *es = SURF_INFO( surf, worldmodel );

		if( FBitSet( surf->flags, SURF_DRAWSKY ))
			continue;	// ignore sky polys it was never be drawed

		es->mesh->firstVert = curVert; // store the vertex offset

		for( j = 0; j < es->mesh->numVerts; j++, curVert++ )
		{
			arrayverts[curVert].vertex = es->mesh->verts[j].vertex;
			arrayverts[curVert].normal = es->mesh->verts[j].normal;
			arrayverts[curVert].tangent = es->mesh->verts[j].tangent;
			arrayverts[curVert].binormal = -es->mesh->verts[j].binormal; // intentionally inverted. Don't touch.
			arrayverts[curVert].stcoord[0] = es->mesh->verts[j].stcoord[0];
			arrayverts[curVert].stcoord[1] = es->mesh->verts[j].stcoord[1];
			arrayverts[curVert].lmcoord[0] = es->mesh->verts[j].lmcoord[0];
			arrayverts[curVert].lmcoord[1] = es->mesh->verts[j].lmcoord[1];
		}
	}

	// create GPU static buffer
	pglGenBuffersARB( 1, &tr.world_vbo );
	pglGenVertexArrays( 1, &tr.world_vao );

	// create vertex array object
	pglBindVertexArray( tr.world_vao );

	pglBindBufferARB( GL_ARRAY_BUFFER_ARB, tr.world_vbo );
	pglBufferDataARB( GL_ARRAY_BUFFER_ARB, numVerts * sizeof( bvert_t ), &arrayverts[0], GL_STATIC_DRAW_ARB );

	pglVertexAttribPointerARB( ATTR_INDEX_POSITION, 3, GL_FLOAT, GL_FALSE, sizeof( bvert_t ), (void *)offsetof( bvert_t, vertex ));
	pglEnableVertexAttribArrayARB( ATTR_INDEX_POSITION );

	pglVertexAttribPointerARB( ATTR_INDEX_TEXCOORD0, 2, GL_FLOAT, GL_FALSE, sizeof( bvert_t ), (void *)offsetof( bvert_t, stcoord ));
	pglEnableVertexAttribArrayARB( ATTR_INDEX_TEXCOORD0 );

	pglVertexAttribPointerARB( ATTR_INDEX_TEXCOORD1, 2, GL_FLOAT, GL_FALSE, sizeof( bvert_t ), (void *)offsetof( bvert_t, lmcoord ));
	pglEnableVertexAttribArrayARB( ATTR_INDEX_TEXCOORD1 );

	pglVertexAttribPointerARB( ATTR_INDEX_NORMAL, 3, GL_FLOAT, GL_FALSE, sizeof( bvert_t ), (void *)offsetof( bvert_t, normal ));
	pglEnableVertexAttribArrayARB( ATTR_INDEX_NORMAL );

	pglVertexAttribPointerARB( ATTR_INDEX_BINORMAL, 3, GL_FLOAT, GL_FALSE, sizeof( bvert_t ), (void *)offsetof( bvert_t, binormal ));
	pglEnableVertexAttribArrayARB( ATTR_INDEX_BINORMAL );

	pglVertexAttribPointerARB( ATTR_INDEX_TANGENT, 3, GL_FLOAT, GL_FALSE, sizeof( bvert_t ), (void *)offsetof( bvert_t, tangent ));
	pglEnableVertexAttribArrayARB( ATTR_INDEX_TANGENT );

	// don't forget to unbind them
	pglBindVertexArray( GL_FALSE );
	pglBindBufferARB( GL_ARRAY_BUFFER_ARB, 0 );

	// no reason to keep this data
	Mem_Free( arrayverts );
}

/*
===============
DestroyWorldMeshCache

Release world VBO
===============
*/
void DestroyWorldMeshCache( void )
{
	if( tr.world_vao ) pglDeleteVertexArrays( 1, &tr.world_vao );
	if( tr.world_vbo ) pglDeleteBuffersARB( 1, &tr.world_vbo );

	tr.world_vao = tr.world_vbo = 0;
}

/*
===============
R_ChooseBmodelProgram

Select the program for surface (diffuse\bump\parallax\debug)
===============
*/
word R_ChooseBmodelProgram( const msurface_t *s, unsigned int lightbit )
{
	material_t *mat = R_TextureAnimation( s->texinfo->texture, ( s - worldmodel->surfaces ))->material;

	if( FBitSet( RI.params, RP_SHADOWVIEW ))
		return (glsl.depthFillGeneric - tr.glsl_programs);

	if( lightbit != 0 )
	{
		if( FBitSet( s->flags, ( SURF_REFLECT|SURF_MOVIE )))
			return (glsl.bmodelDynLight - tr.glsl_programs);
		if( cv_bump->value && FBitSet( mat->flags, BRUSH_HAS_BUMP ))
			return (glsl.bmodelBumpDynLight - tr.glsl_programs);
		return (glsl.bmodelDynLight - tr.glsl_programs);
	}
	else
	{
		if( RI.currentmodel->flags & BIT( 2 ))
			return (glsl.bmodelWater - tr.glsl_programs);
		if( RI.currententity->curstate.rendermode == kRenderTransTexture )
			return (glsl.bmodelGlass - tr.glsl_programs);
		if( RI.currententity->curstate.rendermode == kRenderTransAdd )
			return (glsl.bmodelAdditive - tr.glsl_programs);
		if( r_lightmap->value )
			return (glsl.debugLightmapShader - tr.glsl_programs);
		if( !FBitSet( mat->flags, MAT_ALL_EFFECTS ) && !FBitSet( s->flags, SURF_BUMPDATA ))
			return (glsl.bmodelAmbient - tr.glsl_programs); // no any effects required
		if( FBitSet( s->flags, ( SURF_REFLECT|SURF_MOVIE )) || !FBitSet( s->flags, SURF_BUMPDATA ))
			return (glsl.bmodelDiffuse - tr.glsl_programs);
		if( cv_bump->value > 1.0f && FBitSet( s->flags, SURF_REFLECT_FRESNEL ))
			return (glsl.bmodelReflectBump - tr.glsl_programs);
		if( cv_parallax->value && FBitSet( mat->flags, BRUSH_HAS_PARALLAX ))
			return (glsl.bmodelParallax - tr.glsl_programs);
		if( cv_bump->value && FBitSet( mat->flags, BRUSH_HAS_BUMP ))
			return (glsl.bmodelRealBump - tr.glsl_programs);
		return (glsl.bmodelFakeBump - tr.glsl_programs);
	}
}

/*
===============
R_AddSurfaceToDrawList

Single list for world and solid bmodels
===============
*/
void R_AddSurfaceToDrawList( msurface_t *s, unsigned int lightbit )
{
	if( tr.num_draw_surfaces >= MAX_SORTED_FACES )
	{
		ALERT( at_error, "R_AddSurfaceToDrawList: surface list is full\n" );
		return;
	}

	if( FBitSet( s->flags, SURF_DRAWSKY ))
	{
		RI.params |= RP_SKYVISIBLE;
		return; // don't add sky faces
          }

	if( lightbit && RI.currententity->curstate.renderfx == SKYBOX_ENTITY )
		return; // skybox entities can't lighting by dynlights

	if( lightbit && RI.currententity->curstate.rendermode == kRenderTransTexture )
		return; // transparent surfaces can't be lighted by any lights

	if( FBitSet( RI.params, RP_SHADOWVIEW ) && FBitSet( s->flags, SURF_DRAWTILED ))
		return;

	if( FBitSet( s->flags, SURF_MOVIE ))
		R_UpdateCinematic( s ); // upload frames

	if( s->dlightframe != tr.dlightframecount )
	{
		s->dlightframe = tr.dlightframecount;
		s->dlightbits = 0;
	}
	SetBits( s->dlightbits, lightbit );

	gl_bmodelface_t *entry = &tr.draw_surfaces[tr.num_draw_surfaces++];

	entry->surface = (msurface_t *)s;
	entry->hProgram = R_ChooseBmodelProgram( s, lightbit );
	entry->parent = RI.currententity;

	if( lightbit ) r_stats.c_light_polys++;
	else r_stats.c_world_polys++;
}

/*
===============
R_AddSurfaceToLightList

Single list for world and solid bmodels
===============
*/
void R_AddSurfaceToLightList( gl_bmodelface_t *in )
{
	if( tr.num_light_surfaces >= MAX_SORTED_FACES )
	{
		ALERT( at_error, "R_AddSurfaceToLightList: surface list is full\n" );
		return;
	}

	if( in->parent->curstate.renderfx == SKYBOX_ENTITY )
		return; // skybox entities can't lighting by dynlights

	if( in->parent->curstate.rendermode == kRenderTransTexture )
		return; // transparent surfaces can't be lighted by any lights

	gl_bmodelface_t *entry = &tr.light_surfaces[tr.num_light_surfaces++];

	*entry = *in;
	entry->hProgram = R_ChooseBmodelProgram( in->surface, true );

	r_stats.c_light_polys++;
}

/*
=============================================================

	BRUSH MODEL

=============================================================
*/
/*
===============
R_AddBmodelToDrawList

Cull out model and compute modelviewprojection
===============
*/
void R_AddBmodelToDrawList( cl_entity_t *e )
{
	RI.currententity = e;
	RI.currentmodel = RI.currententity->model;

	// don't reflect this entity in mirrors
	if( FBitSet( e->curstate.effects, EF_NOREFLECT ) && FBitSet( RI.params, RP_MIRRORVIEW ))
		return;

	// draw only in mirrors
	if( FBitSet( e->curstate.effects, EF_REFLECTONLY ) && !FBitSet( RI.params, RP_MIRRORVIEW ))
		return;

	if( RI.currententity == tr.mirror_entity )
		return;

	// static entities can be culled as world surfaces
	qboolean	worldpos = (e->origin == g_vecZero && e->angles == g_vecZero) ? true : false;
	uint	clipFlags = (worldpos) ? RI.clipFlags : 0;
	GLfloat	modelviewProjectionMatrix[16];
	GLfloat	texofs[2], *texptr = NULL;
	GLfloat	modelviewMatrix[16];
	model_t	*clmodel = e->model;
	Vector	mins, maxs;

	// skybox entity
	if( e->curstate.renderfx == SKYBOX_ENTITY )
	{
		if( FBitSet( RI.params, RP_SHADOWVIEW ))
			return; // exclude from shadowpass

		Vector	trans = RI.vieworg - tr.sky_origin;

		if( tr.sky_speed )
		{
			trans = trans - (RI.vieworg - tr.sky_world_origin) / tr.sky_speed;
			Vector skypos = tr.sky_origin + (RI.vieworg - tr.sky_world_origin) / tr.sky_speed;
			tr.modelorg = skypos - e->origin;
		}
		else tr.modelorg = tr.sky_origin - e->origin;

		mins = e->origin + trans + clmodel->mins;
		maxs = e->origin + trans + clmodel->maxs;

		if( R_CullBox( mins, maxs, RI.clipFlags ))
			return;

		e->visframe = tr.framecount; // visible
		clipFlags = 0;

		// compute modelviewprojection
		matrix4x4 modelview = RI.worldviewMatrix.ConcatTransforms( matrix4x4( e->origin + trans, g_vecZero, 1.0f ));
		matrix4x4 modelviewProjection = RI.projectionMatrix.Concat( modelview );
		modelviewProjection.CopyToArray( modelviewProjectionMatrix );
		modelview.CopyToArray( modelviewMatrix );
	}
	else
	{
		if( e->angles != g_vecZero )
		{
			mins = e->origin - clmodel->radius;
			maxs = e->origin + clmodel->radius;
		}
		else
		{
			mins = e->origin + clmodel->mins;
			maxs = e->origin + clmodel->maxs;
		}

		if( R_CullBox( mins, maxs, RI.clipFlags ))
			return;

		e->visframe = tr.framecount; // visible

		// a bit optimization
		if( !worldpos )
		{
			// compute modelviewprojection
			matrix4x4 object = matrix4x4( e->origin, e->angles, 1.0f );
			matrix4x4 modelview = RI.worldviewMatrix.ConcatTransforms( object );
			matrix4x4 modelviewProjection = RI.projectionMatrix.Concat( modelview );
			modelviewProjection.CopyToArray( modelviewProjectionMatrix );
			modelview.CopyToArray( modelviewMatrix );

			if( e->angles != g_vecZero )
				tr.modelorg = object.VectorITransform( RI.vieworg );
			else tr.modelorg = RI.vieworg - e->origin;
		}
		else
		{
			memcpy( modelviewProjectionMatrix, RI.gl_modelviewProjectionMatrix, sizeof( float ) * 16 );
			memcpy( modelviewMatrix, RI.gl_modelviewMatrix, sizeof( float ) * 16 );
			tr.modelorg = RI.vieworg;
		}
	}

	if( clmodel->flags & BIT( 0 )) // MODEL_CONVEYOR
	{
		float	flConveyorSpeed, sy, cy;
		float	flRate, flAngle, flWidth = 0.0f;

		flConveyorSpeed = (e->curstate.rendercolor.g<<8|e->curstate.rendercolor.b) / 16.0f;
		if( e->curstate.rendercolor.r ) flConveyorSpeed = -flConveyorSpeed;
		msurface_t *s = &clmodel->surfaces[clmodel->firstmodelsurface];
		for( int i = 0; i < clmodel->nummodelsurfaces; i++, s++ )
		{
			if( s->flags & SURF_CONVEYOR )
			{
				flWidth = (float)RENDER_GET_PARM( PARM_TEX_SRC_WIDTH, s->texinfo->texture->gl_texturenum );
				break;
			}
		}

		if( flWidth != 0.0f )
		{
			flRate = abs( flConveyorSpeed ) / (float)flWidth;
			flAngle = ( flConveyorSpeed >= 0 ) ? 180 : 0;

			SinCos( flAngle * ( M_PI / 180.0f ), &sy, &cy );
			texofs[0] = RI.refdef.time * cy * flRate;
			texofs[1] = RI.refdef.time * sy * flRate;
	
			// make sure that we are positive
			if( texofs[0] < 0.0f ) texofs[0] += 1.0f + -(int)texofs[0];
			if( texofs[1] < 0.0f ) texofs[1] += 1.0f + -(int)texofs[1];

			// make sure that we are in a [0,1] range
			texofs[0] = texofs[0] - (int)texofs[0];
			texofs[1] = texofs[1] - (int)texofs[1];

			texptr = texofs;
		}
	}

	// store matrix in cache
	RI.currententity->hCachedMatrix = GL_RegisterCachedMatrix( modelviewProjectionMatrix, modelviewMatrix, tr.modelorg, texptr );

	// accumulate surfaces, build the lightmaps
	msurface_t *s = &clmodel->surfaces[clmodel->firstmodelsurface];
	for( int i = 0; i < clmodel->nummodelsurfaces; i++, s++ )
	{
		if( R_CullSurface( s, clipFlags ))
			continue;

		s->visframe = tr.framecount;
		R_AddSurfaceToDrawList( s );
	}
}

/*
===============
R_AddBmodelToLightList

Draw lighting for bmodel
===============
*/
void R_AddBmodelToLightList( cl_entity_t *e, DynamicLight *pl )
{
	RI.currententity = e;
	RI.currentmodel = RI.currententity->model;

	// don't reflect this entity in mirrors
	if( FBitSet( e->curstate.effects, EF_NOREFLECT ) && FBitSet( RI.params, RP_MIRRORVIEW ))
		return;

	// draw only in mirrors
	if( FBitSet( e->curstate.effects, EF_REFLECTONLY ) && !FBitSet( RI.params, RP_MIRRORVIEW ))
		return;

	// if entity not visible from client point no reason to lighting it
	if( e->curstate.renderfx == SKYBOX_ENTITY || e->visframe != tr.framecount )
		return; // fast reject

	// static entities can be culled as world surfaces
	qboolean	worldpos = (e->origin == g_vecZero && e->angles == g_vecZero) ? true : false;
	uint	clipFlags = (worldpos) ? pl->clipflags : 0;
	model_t	*clmodel = e->model;
	Vector	mins, maxs;

	if( e->angles != g_vecZero )
	{
		mins = e->origin - clmodel->radius;
		maxs = e->origin + clmodel->radius;
	}
	else
	{
		mins = e->origin + clmodel->mins;
		maxs = e->origin + clmodel->maxs;
	}

	if( R_CullBoxExt( pl->frustum, mins, maxs, pl->clipflags ))
		return;

	if( e->angles != g_vecZero )
	{
		matrix4x4 object = matrix4x4( e->origin, e->angles, 1.0f );
		tr.modelorg = object.VectorITransform( pl->origin );
	}
	else tr.modelorg = pl->origin - e->origin;

	// accumulate surfaces, build the lightmaps
	msurface_t *s = &clmodel->surfaces[clmodel->firstmodelsurface];
	for( int i = 0; i < clmodel->nummodelsurfaces; i++, s++ )
	{
		if( R_CullSurfaceExt( s, pl->frustum, clipFlags ))
			continue;

		R_AddSurfaceToDrawList( s, BIT( pl - cl_dlights ));
	}
}

/*
================
R_RenderDynLightList

================
*/
void R_BuildFaceListForLight( DynamicLight *pl )
{
	RI.currententity = GET_ENTITY( 0 );
	RI.currentmodel = RI.currententity->model;
	tr.modelorg = pl->origin;

	if( r_test->value )
	{
		tr.num_light_surfaces = 0;

		for( int i = 0; i < tr.num_draw_surfaces; i++ )
		{
			gl_bmodelface_t *entry = &tr.draw_surfaces[i];

			RI.currententity = entry->parent;
			RI.currentmodel = RI.currententity->model;
			qboolean worldpos = (entry->parent->origin == g_vecZero && entry->parent->angles == g_vecZero) ? true : false;
			uint clipFlags = (worldpos) ? pl->clipflags : 0;

			if( worldpos ) tr.modelorg = pl->origin;
			else if( entry->parent->angles != g_vecZero )
			{
				matrix4x4 object = matrix4x4( entry->parent->origin, entry->parent->angles, 1.0f );
				tr.modelorg = object.VectorITransform( pl->origin );
			}
			else tr.modelorg = pl->origin - entry->parent->origin;

			if( R_CullSurfaceExt( entry->surface, pl->frustum, clipFlags ))
				continue;

			// move from main list into light list
			R_AddSurfaceToLightList( entry );
		}
	}
	else
	{
		tr.num_draw_surfaces = 0;

		// draw world from light position
		R_RecursiveLightNode( worldmodel->nodes, pl->frustum, pl->clipflags, BIT( pl - cl_dlights ));

		// add all solid bmodels
		for( int i = 0; i < tr.num_solid_bmodel_ents; i++ )
			R_AddBmodelToLightList( tr.solid_bmodel_ents[i], pl );
	}
}

/*
================
R_DrawLightForSurfList

setup light projection for each 
================
*/
void R_DrawLightForSurfList( DynamicLight *pl, gl_bmodelface_t *surfaces, int surfcount )
{
	int		cached_matrix = -1;
	material_t	*cached_material = NULL;
	qboolean		flush_buffer = false;
	int		lightmode = LIGHT_DIRECTIONAL;
	int		startv, endv;

	if( !pl->spotlightTexture )
		lightmode = LIGHT_OMNIDIRECTIONAL;
	else if( pl->key == SUNLIGHT_KEY )
		lightmode = LIGHT_SUN;

	if( pl->key == SUNLIGHT_KEY )
		pglBlendFunc( GL_ZERO, GL_SRC_COLOR );
	else pglBlendFunc( GL_ONE, GL_ONE );

	glState.drawProjection = true;
	startv = MAX_MAP_VERTS;
	numTempElems = 0;
	endv = 0;

	// sorting list to reduce shader switches
	if( !cv_nosort->value ) R_QSortDrawFaces( surfaces, 0, surfcount - 1 );

	for( int i = 0; i < surfcount; i++ )
	{
		gl_bmodelface_t *entry = &surfaces[i];
		mextrasurf_t *es = SURF_INFO( entry->surface, entry->parent->model );
		RI.currentmodel = entry->parent->model;
		RI.currententity = entry->parent;
		msurface_t *s = entry->surface;
		Vector lightpos, lightdir;
		float texofs[2];

		material_t *mat = R_TextureAnimation( s->texinfo->texture, ( s - RI.currentmodel->surfaces ))->material;

		if(( i == 0 ) || ( RI.currentshader != &tr.glsl_programs[entry->hProgram] ))
			flush_buffer = true;

		if( cached_matrix != entry->parent->hCachedMatrix )
			flush_buffer = true;

		if( cached_material != mat )
			flush_buffer = true;

		if( flush_buffer )
		{
			if( numTempElems )
			{
				pglDrawRangeElementsEXT( GL_TRIANGLES, startv, endv - 1, numTempElems, GL_UNSIGNED_INT, tempElems );
				r_stats.num_flushes++;
				startv = MAX_MAP_VERTS;
				numTempElems = 0;
				endv = 0;
			}
			flush_buffer = false;
		}

		bool worldpos = ( RI.currententity->origin == g_vecZero && RI.currententity->angles == g_vecZero ) ? true : false;

		if( worldpos )
		{
			lightpos = pl->origin;
			lightdir = pl->frustum[5].normal;
		}
		else
		{
			RI.objectMatrix = matrix4x4( RI.currententity->origin, RI.currententity->angles, 1.0f );
			lightdir = RI.objectMatrix.VectorIRotate( pl->frustum[5].normal );
			lightpos = RI.objectMatrix.VectorITransform( pl->origin );
			if( lightmode != LIGHT_OMNIDIRECTIONAL )
				R_MergeLightProjection( pl );
			tr.modelviewIdentity = false;
		}

		// begin draw the sorted list
		if(( i == 0 ) || ( RI.currentshader != &tr.glsl_programs[entry->hProgram] ))
		{
			GL_BindShader( &tr.glsl_programs[entry->hProgram] );			

			ASSERT( RI.currentshader != NULL );

			// write constants
			pglUniform3fARB( RI.currentshader->u_LightDiffuse, pl->color.x, pl->color.y, pl->color.z );
			pglUniform1iARB( RI.currentshader->u_MirrorMode, bound( 0, (int)r_allow_mirrors->value, 1 ));
			pglUniform1iARB( RI.currentshader->u_DetailMode, bound( 0, (int)r_detailtextures->value, 1 ));
			pglUniform1iARB( RI.currentshader->u_ShadowMode, (int)( bound( 0.0f, r_shadows->value, 3.0f )));
			pglUniform1fARB( RI.currentshader->u_ScreenWidth, (float)RENDER_GET_PARM( PARM_TEX_WIDTH, pl->shadowTexture ));
			pglUniform1fARB( RI.currentshader->u_ScreenHeight, (float)RENDER_GET_PARM( PARM_TEX_HEIGHT, pl->shadowTexture ));
			pglUniform1iARB( RI.currentshader->u_ParallaxMode, bound( 0, (int)cv_parallax->value, 2 ));
			pglUniform1iARB( RI.currentshader->u_SpecularMode, bound( 0, (int)cv_specular->value, 1 ));
			pglUniform1iARB( RI.currentshader->u_ClipPlane, ( RI.params & RP_CLIPPLANE ));
			pglUniform1iARB( RI.currentshader->u_GenericCondition, lightmode );
			texofs[0] = texofs[1] = 0.0f;

			// reset cache
			cached_material = NULL;
			cached_matrix = -1;
		}

		if( cached_matrix != entry->parent->hCachedMatrix )
		{
			gl_cachedmatrix_t *view = &tr.cached_matrices[entry->parent->hCachedMatrix];
			pglUniformMatrix4fvARB( RI.currentshader->u_ModelViewProjectionMatrix, 1, GL_FALSE, &view->modelviewProjectionMatrix[0] );
			pglUniformMatrix4fvARB( RI.currentshader->u_ModelViewMatrix, 1, GL_FALSE, &view->modelviewMatrix[0] );
			pglUniform3fARB( RI.currentshader->u_ViewOrigin, view->modelorg.x, view->modelorg.y, view->modelorg.z );
			pglUniform3fARB( RI.currentshader->u_LightDir, -lightdir.x, -lightdir.y, -lightdir.z );
			pglUniform3fARB( RI.currentshader->u_LightOrigin, lightpos.x, lightpos.y, lightpos.z );
			pglUniform1fARB( RI.currentshader->u_LightRadius, pl->radius );

			texofs[0] = view->texofs[0];
			texofs[1] = view->texofs[1];

			if( lightmode == 1 )
			{
				GL_Bind( GL_TEXTURE0, tr.atten_point_2d );
				GL_LoadIdentityTexMatrix();

				GL_Bind( GL_TEXTURE1, tr.atten_point_1d );
				GL_LoadIdentityTexMatrix();

				GL_Bind( GL_TEXTURE2, tr.blackTexture ); // stub
				GL_LoadIdentityTexMatrix();
			}
			else
			{
				GL_Bind( GL_TEXTURE0, pl->spotlightTexture );
				GL_LoadTexMatrix( (worldpos) ? pl->textureMatrix : pl->textureMatrix2 );

				GL_Bind( GL_TEXTURE1, tr.attenuation_1d );
				GL_LoadIdentityTexMatrix();

				GL_Bind( GL_TEXTURE2, pl->shadowTexture );
				GL_LoadTexMatrix( (worldpos) ? pl->shadowMatrix : pl->shadowMatrix2 );
			}

			cached_matrix = entry->parent->hCachedMatrix;
		}

		if( cached_material != mat )
		{
			if( FBitSet( mat->flags, BRUSH_REFLECT ) && r_allow_mirrors->value && !FBitSet( RI.params, RP_MIRRORVIEW ))
			{
				mextrasurf_t *es = SURF_INFO( s, RI.currentmodel );
				GL_Bind( GL_TEXTURE3, es->mirrortexturenum );
				GL_LoadTexMatrix( es->mirrormatrix );
			}
			else if( FBitSet( s->flags, SURF_MOVIE ) && RI.currententity->curstate.body )
			{ 
				mextrasurf_t *es = SURF_INFO( s, RI.currentmodel );
				GL_Bind( GL_TEXTURE3, tr.cinTextures[es->mirrortexturenum-1] );
				GL_LoadIdentityTexMatrix();
			}
			else
			{
				GL_Bind( GL_TEXTURE3, mat->gl_diffuse_id );
				GL_LoadIdentityTexMatrix();
			}

			// setup textures
			GL_Bind( GL_TEXTURE4, mat->gl_detail_id );
			GL_Bind( GL_TEXTURE5, mat->gl_normalmap_id );
			GL_Bind( GL_TEXTURE6, mat->gl_specular_id );
			GL_Bind( GL_TEXTURE7, mat->gl_heightmap_id );

			// update material flags
			pglUniform1iARB( RI.currentshader->u_FaceFlags, mat->flags );
			pglUniform2fARB( RI.currentshader->u_DetailScale, mat->detailScale[0], mat->detailScale[1] );
			pglUniform1fARB( RI.currentshader->u_GlossExponent, mat->glossExp );

			float scale_x = mat->parallaxScale / (float)RENDER_GET_PARM( PARM_TEX_WIDTH, mat->gl_diffuse_id );
			float scale_y = mat->parallaxScale / (float)RENDER_GET_PARM( PARM_TEX_HEIGHT, mat->gl_diffuse_id );
			float *lr = mat->lightRemap;

			if( FBitSet( s->flags, SURF_CONVEYOR ))
				pglUniform2fARB( RI.currentshader->u_TexOffset, texofs[0], texofs[1] );
			else pglUniform2fARB( RI.currentshader->u_TexOffset, 0.0f, 0.0f );

			pglUniform2fARB( RI.currentshader->u_ParallaxScale, scale_x, scale_y );
			pglUniform1iARB( RI.currentshader->u_ParallaxSteps, mat->parallaxSteps );
			pglUniform4fARB( RI.currentshader->u_RemapParms, lr[0], lr[1], lr[2], lr[3] );
			pglUniform1fARB( RI.currentshader->u_ReflectScale, bound( 0.0f, mat->reflectScale, 1.0f ));
			pglUniform1fARB( RI.currentshader->u_RefractScale, mat->refractScale );
			cached_material = mat;
		}

		if( es->mesh->firstVert < startv )
			startv = es->mesh->firstVert;

		if(( es->mesh->firstVert + es->mesh->numVerts ) > endv )
			endv = es->mesh->firstVert + es->mesh->numVerts;

		// accumulate the indices
		for( int j = 0; j < es->mesh->numElems; j += 3 )
		{
			ASSERT( numTempElems < ( MAX_MAP_VERTS - 3 ));

			tempElems[numTempElems++] = es->mesh->firstVert + es->mesh->elems[j+0];
			tempElems[numTempElems++] = es->mesh->firstVert + es->mesh->elems[j+1];
			tempElems[numTempElems++] = es->mesh->firstVert + es->mesh->elems[j+2];
		}
	}

	if( numTempElems )
	{
		pglDrawRangeElementsEXT( GL_TRIANGLES, startv, endv - 1, numTempElems, GL_UNSIGNED_INT, tempElems );
		r_stats.num_flushes++;
		startv = MAX_MAP_VERTS;
		numTempElems = 0;
		endv = 0;
	}

	glState.drawProjection = false;
	RI.currentlight = NULL;
}

/*
================
R_RenderDynLightList

draw dynamic lights for world and bmodels
================
*/
void R_RenderDynLightList( void )
{
	if( !FBitSet( RI.params, RP_HASDYNLIGHTS ))
		return;

	pglEnable( GL_BLEND );
	pglDepthMask( GL_FALSE );

	float time = GET_CLIENT_TIME();
	DynamicLight *pl = cl_dlights;

	for( int i = 0; i < MAX_DLIGHTS; i++, pl++ )
	{
		if( pl->die < time || !pl->radius )
			continue;

		RI.currentlight = pl;

		// draw world from light position
		R_BuildFaceListForLight( pl );

		if( r_test->value )
		{
			if( !tr.num_light_surfaces )
				continue;	// no interaction with this light?

			R_DrawLightForSurfList( pl, tr.light_surfaces, tr.num_light_surfaces );
		}
		else
		{
			if( !tr.num_draw_surfaces )
				continue;	// no interaction with this light?

			R_DrawLightForSurfList( pl, tr.draw_surfaces, tr.num_draw_surfaces );
		}
	}

	pglDisable( GL_BLEND );
	GL_SelectTexture( glConfig.max_texture_units - 1 ); // force to cleanup all the units
	GL_CleanUpTextureUnits( 0 );
}

/*
================
R_RenderSolidBrushList
================
*/
void R_RenderSolidBrushList( void )
{
	int		cached_matrix = -1;
	int		cached_lightmap = -1;
	material_t	*cached_material = NULL;
	qboolean		flush_buffer = false;
	int		startv, endv;

	if( !tr.num_draw_surfaces ) return;

	R_LoadIdentity();
	pglDisable( GL_BLEND );
	pglDepthMask( GL_TRUE );
	pglDepthFunc( GL_LEQUAL );
	startv = MAX_MAP_VERTS;
	numTempElems = 0;
	endv = 0;

	// sorting list to reduce shader switches
	if( !cv_nosort->value ) R_QSortDrawFaces( tr.draw_surfaces, 0, tr.num_draw_surfaces - 1 );

	pglBindVertexArray( tr.world_vao );

	for( int i = 0; i < tr.num_draw_surfaces; i++ )
	{
		gl_bmodelface_t *entry = &tr.draw_surfaces[i];
		mextrasurf_t *es = SURF_INFO( entry->surface, entry->parent->model );
		RI.currentmodel = entry->parent->model;
		RI.currententity = entry->parent;
		msurface_t *s = entry->surface;
		float texofs[2];

		material_t *mat = R_TextureAnimation( s->texinfo->texture, ( s - worldmodel->surfaces ))->material;

		if(( i == 0 ) || ( RI.currentshader != &tr.glsl_programs[entry->hProgram] ))
			flush_buffer = true;

		if( cached_matrix != entry->parent->hCachedMatrix )
			flush_buffer = true;

		if(( cached_lightmap != s->lightmaptexturenum ))
			flush_buffer = true;

		if( cached_material != mat )
			flush_buffer = true;

		if( flush_buffer )
		{
			if( numTempElems )
			{
				pglDrawRangeElementsEXT( GL_TRIANGLES, startv, endv - 1, numTempElems, GL_UNSIGNED_INT, tempElems );
				r_stats.num_flushes++;
				startv = MAX_MAP_VERTS;
				numTempElems = 0;
				endv = 0;
			}
			flush_buffer = false;
		}

		// begin draw the sorted list
		if(( i == 0 ) || ( RI.currentshader != &tr.glsl_programs[entry->hProgram] ))
		{
			GL_BindShader( &tr.glsl_programs[entry->hProgram] );			

			ASSERT( RI.currentshader != NULL );

			// write constants
			pglUniform1iARB( RI.currentshader->u_MirrorMode, bound( 0, (int)r_allow_mirrors->value, 1 ));
			pglUniform1iARB( RI.currentshader->u_DetailMode, bound( 0, (int)r_detailtextures->value, 1 ));
			pglUniform1iARB( RI.currentshader->u_ParallaxMode, bound( 0, (int)cv_parallax->value, 2 ));
			pglUniform1iARB( RI.currentshader->u_LightmapDebug, bound( 0, (int)r_lightmap->value, 3 ));
			pglUniform1iARB( RI.currentshader->u_SpecularMode, bound( 0, (int)cv_specular->value, 1 ));
			pglUniform1iARB( RI.currentshader->u_ClipPlane, ( RI.params & RP_CLIPPLANE ));
			texofs[0] = texofs[1] = 0.0f;

			// reset cache
			cached_material = NULL;
			cached_lightmap = -1;
			cached_matrix = -1;
		}

		if( cached_matrix != entry->parent->hCachedMatrix )
		{
			gl_cachedmatrix_t *view = &tr.cached_matrices[entry->parent->hCachedMatrix];
			pglUniformMatrix4fvARB( RI.currentshader->u_ModelViewProjectionMatrix, 1, GL_FALSE, &view->modelviewProjectionMatrix[0] );
			pglUniformMatrix4fvARB( RI.currentshader->u_ModelViewMatrix, 1, GL_FALSE, &view->modelviewMatrix[0] );
			pglUniform3fARB( RI.currentshader->u_ViewOrigin, view->modelorg.x, view->modelorg.y, view->modelorg.z );
			cached_matrix = entry->parent->hCachedMatrix;
			texofs[0] = view->texofs[0];
			texofs[1] = view->texofs[1];
		}

		if( RI.currententity->curstate.renderfx == SKYBOX_ENTITY )
		{
			pglDepthRange( 0.8f, 0.9f );
			GL_ClipPlane( false );
		}
		else
		{
			pglDepthRange( gldepthmin, gldepthmax );
			GL_ClipPlane( true );
		}

		if( cached_lightmap != s->lightmaptexturenum )
		{
			if( worldmodel->lightdata )
			{
				// bind real data
				GL_Bind( GL_TEXTURE0, tr.baselightmap[s->lightmaptexturenum] );
				GL_Bind( GL_TEXTURE1, tr.addlightmap[s->lightmaptexturenum] );
				GL_Bind( GL_TEXTURE2, tr.lightvecs[s->lightmaptexturenum] );
			}
			else
			{
				// bind stubs (helper to reduce conditions in shader)
				GL_Bind( GL_TEXTURE0, tr.grayTexture );
				GL_Bind( GL_TEXTURE1, tr.blackTexture );
				GL_Bind( GL_TEXTURE2, tr.deluxemapTexture );
			}
			cached_lightmap = s->lightmaptexturenum;
		}

		if( cached_material != mat )
		{
			if( FBitSet( s->flags, ( SURF_REFLECT|SURF_REFLECT_FRESNEL )) && r_allow_mirrors->value && !FBitSet( RI.params, RP_MIRRORVIEW ))
			{
				if( !FBitSet( s->flags, SURF_REFLECT_FRESNEL ) || cv_bump->value > 1.0f )				
				{
					GL_Bind( GL_TEXTURE3, es->mirrortexturenum );
					GL_LoadTexMatrix( es->mirrormatrix );
				}
				else
				{
					GL_Bind( GL_TEXTURE3, mat->gl_diffuse_id );
					GL_LoadIdentityTexMatrix();
				}
			}
			else if( FBitSet( s->flags, SURF_MOVIE ) && RI.currententity->curstate.body )
			{ 
				GL_Bind( GL_TEXTURE3, tr.cinTextures[es->mirrortexturenum-1] );
				GL_LoadIdentityTexMatrix();
			}
			else
			{
				GL_Bind( GL_TEXTURE3, mat->gl_diffuse_id );
				GL_LoadIdentityTexMatrix();
			}

			// setup textures
			GL_Bind( GL_TEXTURE4, mat->gl_detail_id );
			GL_Bind( GL_TEXTURE5, mat->gl_normalmap_id );
			GL_Bind( GL_TEXTURE6, mat->gl_specular_id );

			// HACKHACK: store diffuse instead of heightmap
			if( FBitSet( s->flags, SURF_REFLECT_FRESNEL ) && cv_bump->value > 1.0f )
				GL_Bind( GL_TEXTURE7, mat->gl_diffuse_id );
			else GL_Bind( GL_TEXTURE7, mat->gl_heightmap_id );

			// update material flags
			pglUniform1iARB( RI.currentshader->u_FaceFlags, mat->flags );
			pglUniform2fARB( RI.currentshader->u_DetailScale, mat->detailScale[0], mat->detailScale[1] );
			pglUniform1fARB( RI.currentshader->u_GlossExponent, mat->glossExp );

			float scale_x = mat->parallaxScale / (float)RENDER_GET_PARM( PARM_TEX_WIDTH, mat->gl_diffuse_id );
			float scale_y = mat->parallaxScale / (float)RENDER_GET_PARM( PARM_TEX_HEIGHT, mat->gl_diffuse_id );
			float *lr = mat->lightRemap;

			if( FBitSet( s->flags, SURF_CONVEYOR ))
				pglUniform2fARB( RI.currentshader->u_TexOffset, texofs[0], texofs[1] );
			else pglUniform2fARB( RI.currentshader->u_TexOffset, 0.0f, 0.0f );

			pglUniform2fARB( RI.currentshader->u_ParallaxScale, scale_x, scale_y );
			pglUniform1iARB( RI.currentshader->u_ParallaxSteps, mat->parallaxSteps );
			pglUniform4fARB( RI.currentshader->u_RemapParms, lr[0], lr[1], lr[2], lr[3] );
			pglUniform1fARB( RI.currentshader->u_ReflectScale, bound( 0.0f, mat->reflectScale, 1.0f ));
			pglUniform1fARB( RI.currentshader->u_RefractScale, mat->refractScale );
			cached_material = mat;
		}

		if( es->mesh->firstVert < startv )
			startv = es->mesh->firstVert;

		if(( es->mesh->firstVert + es->mesh->numVerts ) > endv )
			endv = es->mesh->firstVert + es->mesh->numVerts;

		// accumulate the indices
		for( int j = 0; j < es->mesh->numElems; j += 3 )
		{
			ASSERT( numTempElems < ( MAX_MAP_VERTS - 3 ));

			tempElems[numTempElems++] = es->mesh->firstVert + es->mesh->elems[j+0];
			tempElems[numTempElems++] = es->mesh->firstVert + es->mesh->elems[j+1];
			tempElems[numTempElems++] = es->mesh->firstVert + es->mesh->elems[j+2];
		}
	}

	if( numTempElems )
	{
		pglDrawRangeElementsEXT( GL_TRIANGLES, startv, endv - 1, numTempElems, GL_UNSIGNED_INT, tempElems );
		r_stats.num_flushes++;
		startv = MAX_MAP_VERTS;
		numTempElems = 0;
		endv = 0;
	}

	pglDepthRange( gldepthmin, gldepthmax );
	GL_SelectTexture( glConfig.max_texture_units - 1 ); // force to cleanup all the units
	GL_CleanUpTextureUnits( 0 );
	GL_ClipPlane( true );

	// draw dynamic lighting for world and bmodels
	R_RenderDynLightList();

	pglBindVertexArray( GL_FALSE );
	tr.num_draw_surfaces = 0;

	// render all decals on world and opaque bmodels
	R_DrawDecalList( true );
	GL_BindShader( NULL );
}

/*
================
R_RenderTransBrushList
================
*/
void R_RenderTransBrushList( void )
{
	int		cached_matrix = -1;
	cl_entity_t	*cached_entity = NULL;
	material_t	*cached_material = NULL;
	qboolean		flush_buffer = false;
	int		startv, endv;

	if( !tr.num_draw_surfaces ) return;

	R_LoadIdentity();
	pglEnable( GL_BLEND );
	pglDepthMask( GL_FALSE );
	startv = MAX_MAP_VERTS;
	numTempElems = 0;
	endv = 0;

	// sorting list to reduce shader switches
	if( !cv_nosort->value ) R_QSortDrawFaces( tr.draw_surfaces, 0, tr.num_draw_surfaces - 1 );

	// keep screencopy an actual
	if( tr.scrcpyframe != tr.framecount )
	{
		GL_Bind( GL_TEXTURE0, tr.refractionTexture );
		pglCopyTexSubImage2D( GL_TEXTURE_RECTANGLE_NV, 0, 0, 0, 0, 0, glState.width, glState.height );
		tr.scrcpyframe = tr.framecount;
	}

	pglBindVertexArray( tr.world_vao );

	for( int i = 0; i < tr.num_draw_surfaces; i++ )
	{
		gl_bmodelface_t *entry = &tr.draw_surfaces[i];
		mextrasurf_t *es = SURF_INFO( entry->surface, entry->parent->model );
		RI.currentmodel = entry->parent->model;
		RI.currententity = entry->parent;
		msurface_t *s = entry->surface;
		float r, g, b, a;

		material_t *mat = R_TextureAnimation( s->texinfo->texture, ( s - worldmodel->surfaces ))->material;

		if(( i == 0 ) || ( RI.currentshader != &tr.glsl_programs[entry->hProgram] ))
			flush_buffer = true;

		if( cached_matrix != entry->parent->hCachedMatrix )
			flush_buffer = true;

		if( cached_material != mat )
			flush_buffer = true;

		if( cached_entity != RI.currententity )
			flush_buffer = true;

		if( flush_buffer )
		{
			if( numTempElems )
			{
				pglDrawRangeElementsEXT( GL_TRIANGLES, startv, endv - 1, numTempElems, GL_UNSIGNED_INT, tempElems );
				r_stats.num_flushes++;
				startv = MAX_MAP_VERTS;
				numTempElems = 0;
				endv = 0;
			}
			flush_buffer = false;
		}

		if( RI.currentmodel->flags & BIT( 2 ) && RI.refdef.waterlevel >= 3 )
			GL_Cull( GL_NONE );
		else GL_Cull( GL_FRONT );

		// begin draw the sorted list
		if(( i == 0 ) || ( RI.currentshader != &tr.glsl_programs[entry->hProgram] ))
		{
			GL_BindShader( &tr.glsl_programs[entry->hProgram] );			

			ASSERT( RI.currentshader != NULL );

			// write constants
			pglUniform1iARB( RI.currentshader->u_MirrorMode, bound( 0, (int)r_allow_mirrors->value, 1 ));
			pglUniform1iARB( RI.currentshader->u_DetailMode, bound( 0, (int)r_detailtextures->value, 1 ));
			pglUniform1iARB( RI.currentshader->u_ClipPlane, ( RI.params & RP_CLIPPLANE ));

			// reset cache
			cached_material = NULL;
			cached_entity = NULL;
			cached_matrix = -1;
		}

		if(( cached_matrix != entry->parent->hCachedMatrix ) || ( cached_entity != RI.currententity ))
		{
			gl_cachedmatrix_t *view = &tr.cached_matrices[entry->parent->hCachedMatrix];
			pglUniformMatrix4fvARB( RI.currentshader->u_ModelViewProjectionMatrix, 1, GL_FALSE, &view->modelviewProjectionMatrix[0] );
			pglUniformMatrix4fvARB( RI.currentshader->u_ModelViewMatrix, 1, GL_FALSE, &view->modelviewMatrix[0] );
			pglUniform3fARB( RI.currentshader->u_ViewOrigin, view->modelorg.x, view->modelorg.y, view->modelorg.z );
			pglUniform2fARB( RI.currentshader->u_TexOffset, view->texofs[0], view->texofs[1] );

			if( !FBitSet( s->flags, SURF_CONVEYOR ))
			{
				r = RI.currententity->curstate.rendercolor.r / 255.0f;
				g = RI.currententity->curstate.rendercolor.g / 255.0f;
				b = RI.currententity->curstate.rendercolor.b / 255.0f;
			}		
			else r = g = b = 1.0f;

			if( RI.currentmodel->flags & BIT( 2 ))
			{
				if(( view->modelorg.z < RI.currentmodel->maxs.z ) || RI.currententity->curstate.renderamt <= 0.0f )
					a = 2.0f;
				else a = (( 255.0f - RI.currententity->curstate.renderamt ) / 255.0f ) * 2.0f - 1.0f;
			}
			else a = RI.currententity->curstate.renderamt / 255.0f;

			pglUniform4fARB( RI.currentshader->u_RenderColor, r, g, b, a );
			cached_matrix = entry->parent->hCachedMatrix;
			cached_entity = RI.currententity;
		}

		if( RI.currententity->curstate.rendermode == kRenderTransAdd )
			pglBlendFunc( GL_SRC_ALPHA, GL_ONE );
		else pglBlendFunc( GL_ONE, GL_ZERO );

		if( RI.currententity->curstate.renderfx == SKYBOX_ENTITY )
		{
			pglDepthRange( 0.8f, 0.9f );
			GL_ClipPlane( false );
		}
		else
		{
			pglDepthRange( gldepthmin, gldepthmax );
			GL_ClipPlane( true );
		}

		if( cached_material != mat )
		{
			if( RI.currentmodel->flags & BIT( 2 ) && !FBitSet( RI.params, RP_MIRRORVIEW ))
			{
				GL_Bind( GL_TEXTURE0, es->mirrortexturenum );
				GL_Bind( GL_TEXTURE1, mat->gl_detail_id );
				GL_Bind( GL_TEXTURE2, tr.waterTextures[(int)( RI.refdef.time * WATER_ANIMTIME ) % WATER_TEXTURES] );
				GL_Bind( GL_TEXTURE3, tr.refractionTexture );
				GL_LoadTexMatrix( es->mirrormatrix ); // shader waits texMatrix on a third unit
			}
			else
			{
				GL_Bind( GL_TEXTURE0, mat->gl_diffuse_id );
				GL_Bind( GL_TEXTURE1, mat->gl_detail_id );
				GL_Bind( GL_TEXTURE2, mat->gl_normalmap_id );
				GL_Bind( GL_TEXTURE3, tr.refractionTexture );
				GL_LoadIdentityTexMatrix();
			}

			// update material flags
			pglUniform1iARB( RI.currentshader->u_FaceFlags, mat->flags );
			pglUniform2fARB( RI.currentshader->u_DetailScale, mat->detailScale[0], mat->detailScale[1] );
			pglUniform1fARB( RI.currentshader->u_ReflectScale, mat->reflectScale * ( RI.viewport[2] / 640.0f ));
			pglUniform1fARB( RI.currentshader->u_RefractScale, mat->refractScale * ( RI.viewport[2] / 640.0f ));
			cached_material = mat;
		}

		if( es->mesh->firstVert < startv )
			startv = es->mesh->firstVert;

		if(( es->mesh->firstVert + es->mesh->numVerts ) > endv )
			endv = es->mesh->firstVert + es->mesh->numVerts;

		// accumulate the indices
		for( int j = 0; j < es->mesh->numElems; j += 3 )
		{
			ASSERT( numTempElems < ( MAX_MAP_VERTS - 3 ));

			tempElems[numTempElems++] = es->mesh->firstVert + es->mesh->elems[j+0];
			tempElems[numTempElems++] = es->mesh->firstVert + es->mesh->elems[j+1];
			tempElems[numTempElems++] = es->mesh->firstVert + es->mesh->elems[j+2];
		}
	}

	if( numTempElems )
	{
		pglDrawRangeElementsEXT( GL_TRIANGLES, startv, endv - 1, numTempElems, GL_UNSIGNED_INT, tempElems );
		r_stats.num_flushes++;
		startv = MAX_MAP_VERTS;
		numTempElems = 0;
		endv = 0;
	}

	pglDepthRange( gldepthmin, gldepthmax );
	GL_SelectTexture( glConfig.max_texture_units - 1 ); // force to cleanup all the units
	GL_CleanUpTextureUnits( 0 );
	GL_ClipPlane( true );
	GL_Cull( GL_FRONT );

	pglBindVertexArray( GL_FALSE );
	tr.num_draw_surfaces = 0;

	// render all decals on world and opaque bmodels
	R_DrawDecalList( false );
	GL_BindShader( NULL );
}

/*
================
R_RenderShadowBrushList

================
*/
void R_RenderShadowBrushList( void )
{
	int		cached_matrix = -1;
	material_t	*cached_material = NULL;
	qboolean		flush_buffer = false;
	int		startv, endv;

	if( !tr.num_draw_surfaces ) return;

	R_LoadIdentity();
	startv = MAX_MAP_VERTS;
	numTempElems = 0;
	endv = 0;

	// sorting list to reduce shader switches
	if( !cv_nosort->value ) R_QSortDrawFaces( tr.draw_surfaces, 0, tr.num_draw_surfaces - 1 );

	pglBindVertexArray( tr.world_vao );
	GL_BindShader( glsl.depthFillGeneric );

	pglUniform1iARB( RI.currentshader->u_ClipPlane, ( RI.params & RP_CLIPPLANE ));
	pglUniform1iARB( RI.currentshader->u_GenericCondition, GL_FALSE );

	for( int i = 0; i < tr.num_draw_surfaces; i++ )
	{
		gl_bmodelface_t *entry = &tr.draw_surfaces[i];
		mextrasurf_t *es = SURF_INFO( entry->surface, entry->parent->model );
		RI.currentmodel = entry->parent->model;
		RI.currententity = entry->parent;
		msurface_t *s = entry->surface;
		float texofs[2];

		material_t *mat = s->texinfo->texture->material;

		if( cached_matrix != entry->parent->hCachedMatrix )
			flush_buffer = true;

		if( cached_material != mat )
			flush_buffer = true;

		if( flush_buffer )
		{
			if( numTempElems )
			{
				pglDrawRangeElementsEXT( GL_TRIANGLES, startv, endv - 1, numTempElems, GL_UNSIGNED_INT, tempElems );
				r_stats.num_flushes++;
				startv = MAX_MAP_VERTS;
				numTempElems = 0;
				endv = 0;
			}
			flush_buffer = false;
		}

		// begin draw the sorted list
		if( cached_matrix != entry->parent->hCachedMatrix )
		{
			gl_cachedmatrix_t *view = &tr.cached_matrices[entry->parent->hCachedMatrix];
			pglUniformMatrix4fvARB( RI.currentshader->u_ModelViewProjectionMatrix, 1, GL_FALSE, &view->modelviewProjectionMatrix[0] );
			pglUniformMatrix4fvARB( RI.currentshader->u_ModelViewMatrix, 1, GL_FALSE, &view->modelviewMatrix[0] );
			cached_matrix = entry->parent->hCachedMatrix;
			texofs[0] = view->texofs[0];
			texofs[1] = view->texofs[1];
		}

		if( cached_material != mat )
		{
			if( FBitSet( s->flags, SURF_CONVEYOR ))
				pglUniform2fARB( RI.currentshader->u_TexOffset, texofs[0], texofs[1] );
			else pglUniform2fARB( RI.currentshader->u_TexOffset, 0.0f, 0.0f );

			if( FBitSet( mat->flags, BRUSH_TRANSPARENT ))
			{
				pglUniform1iARB( RI.currentshader->u_GenericCondition2, GL_TRUE );
				GL_Bind( GL_TEXTURE0, mat->gl_diffuse_id );
			}
			else
			{ 
				pglUniform1iARB( RI.currentshader->u_GenericCondition2, GL_FALSE );
				pglDisable( GL_TEXTURE_2D );
			}
			cached_material = mat;
		}

		if( es->mesh->firstVert < startv )
			startv = es->mesh->firstVert;

		if(( es->mesh->firstVert + es->mesh->numVerts ) > endv )
			endv = es->mesh->firstVert + es->mesh->numVerts;

		// accumulate the indices
		for( int j = 0; j < es->mesh->numElems; j += 3 )
		{
			ASSERT( numTempElems < ( MAX_MAP_VERTS - 3 ));

			tempElems[numTempElems++] = es->mesh->firstVert + es->mesh->elems[j+0];
			tempElems[numTempElems++] = es->mesh->firstVert + es->mesh->elems[j+1];
			tempElems[numTempElems++] = es->mesh->firstVert + es->mesh->elems[j+2];
		}
	}

	if( numTempElems )
	{
		pglDrawRangeElementsEXT( GL_TRIANGLES, startv, endv - 1, numTempElems, GL_UNSIGNED_INT, tempElems );
		r_stats.num_flushes++;
		startv = MAX_MAP_VERTS;
		numTempElems = 0;
		endv = 0;
	}

	GL_SelectTexture( glConfig.max_texture_units - 1 ); // force to cleanup all the units
	GL_CleanUpTextureUnits( 0 );

	pglBindVertexArray( GL_FALSE );
	tr.num_draw_surfaces = 0;

	GL_BindShader( NULL );
}

void R_AddSolidToDrawList( void )
{
	int	i;

	RI.currententity = GET_ENTITY( 0 );
	RI.currentmodel = RI.currententity->model;

	tr.modelorg = RI.vieworg;
	tr.num_draw_surfaces = 0;
	tr.num_draw_meshes = 0;

	if( HasDynamicLights( ))
		RI.params |= RP_HASDYNLIGHTS;

	R_LoadIdentity();

	// register worldviewProjectionMatrix at zero entry (~80% hits)
	RI.currententity->hCachedMatrix = GL_RegisterCachedMatrix( RI.gl_modelviewProjectionMatrix, RI.gl_modelviewMatrix, tr.modelorg );

	// build the list of visible polys
	R_RecursiveWorldNode( worldmodel->nodes, RI.frustum, RI.clipFlags );

	// add all solid bmodels
	for( i = 0; i < tr.num_solid_bmodel_ents; i++ )
		R_AddBmodelToDrawList( tr.solid_bmodel_ents[i] );

	// add all solid studio
	for( i = 0; i < tr.num_solid_studio_ents; i++ )
		R_AddStudioToDrawList( tr.solid_studio_ents[i] );

	DrawSky();
}

void R_AddTransToDrawList( void )
{
	int	i;

	// add all trans bmodels
	for( i = 0; i < tr.num_trans_bmodel_ents; i++ )
		R_AddBmodelToDrawList( tr.trans_bmodel_ents[i] );

	// add all trans studio
	for( i = 0; i < tr.num_trans_studio_ents; i++ )
		R_AddStudioToDrawList( tr.trans_studio_ents[i] );
}