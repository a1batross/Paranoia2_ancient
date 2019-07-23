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
#include "r_efx.h"
#include "event_api.h"
#include "pm_defs.h"
#include "pmtrace.h"
#include "triangleapi.h"
#include "gl_local.h"
#include "stringlib.h"

extern cvar_t *cv_bumpvecs;

color24 test_add_color, test_base_color;
Vector drawvecsrc;
float gdist;

void DrawVector( const Vector &start, const Vector &end, float r, float g, float b, float a )
{
	HSPRITE hsprTexture = LoadSprite( "sprites/white.spr" );

	if( !hsprTexture )
	{
		ALERT( at_error, "NO SPRITE white.spr!\n" );
		return;
	}

	const model_s *pTexture = gEngfuncs.GetSpritePointer( hsprTexture );
	gEngfuncs.pTriAPI->SpriteTexture( (struct model_s *)pTexture, 0 ); 
	gEngfuncs.pTriAPI->RenderMode( kRenderNormal );
	gEngfuncs.pTriAPI->Color4f( r, g, b, a );

	gEngfuncs.pTriAPI->Begin( TRI_LINES );
		gEngfuncs.pTriAPI->Vertex3f( start[0], start[1], start[2] );
		gEngfuncs.pTriAPI->Vertex3f( end[0], end[1], end[2] );
	gEngfuncs.pTriAPI->End();
}

void DrawVectorDir( const Vector &start, const Vector &dir, float scale, float r, float g, float b, float a )
{
	Vector end;
	VectorMA(start, scale, dir, end);
	DrawVector(start, end, r, g, b, a);
}

int RecursiveDrawTextureVecs( mnode_t *node, const Vector &start, const Vector &end )
{
	int		r;
	float		front, back, frac;
	int		side;
	mplane_t		*plane;
	Vector		mid;
	msurface_t	*surf;
	int		s, t, ds, dt;
	mtexinfo_t	*tex;
	color24		*lightmap; // buz

	if( node->contents < 0 )
		return -1; // didn't hit anything
	
	// calculate mid point
	plane = node->plane;
	front = PlaneDiff( start, plane );
	back = PlaneDiff( end, plane );
	side = front < 0;
	
	if(( back < 0.0f ) == side )
		return RecursiveDrawTextureVecs( node->children[side], start, end );
	
	frac = front / (front - back);
	mid[0] = start[0] + (end[0] - start[0])*frac;
	mid[1] = start[1] + (end[1] - start[1])*frac;
	mid[2] = start[2] + (end[2] - start[2])*frac;
	
	// go down front side	
	r = RecursiveDrawTextureVecs( node->children[side], start, mid );

	if( r >= 0 )
		return r;			// hit something
		
	if(( back < 0.0f ) == side )
		return -1;		// didn't hit anuthing
		
	// check for impact on this node
	surf = worldmodel->surfaces + node->firstsurface;

	for( int i = 0; i < node->numsurfaces; i++, surf++ )
	{
		if( surf->flags & SURF_DRAWTILED )
			continue;	// no lightmaps

		tex = surf->texinfo;
		
		s = DotProduct( mid, tex->vecs[0] ) + tex->vecs[0][3];
		t = DotProduct( mid, tex->vecs[1] ) + tex->vecs[1][3];;

		if( s < surf->texturemins[0] || t < surf->texturemins[1] )
			continue;
		
		ds = s - surf->texturemins[0];
		dt = t - surf->texturemins[1];
		
		if( ds > surf->extents[0] || dt > surf->extents[1] )
			continue;

		if( !surf->samples )
			continue;

		ds /= LM_SAMPLE_SIZE;
		dt /= LM_SAMPLE_SIZE;

		lightmap = surf->samples;
		r = 0;

		if( lightmap )
		{
			lightmap += dt * ((surf->extents[0] / LM_SAMPLE_SIZE) + 1) + ds;
			
			for( int style = 0; style < MAXLIGHTMAPS && surf->styles[style] != 255; style++ )
			{
				if( surf->styles[style] == BUMP_LIGHTVECS_STYLE )
				{
					Vector outcolor;
					outcolor[0] = (float)lightmap->r / 255.0f;
					outcolor[1] = (float)lightmap->g / 255.0f;
					outcolor[2] = (float)lightmap->b / 255.0f;

					Vector vec_x = Vector( tex->vecs[0] ).Normalize();
					Vector vec_y = Vector( tex->vecs[1] ).Normalize();
					Vector vec_z = (surf->flags & SURF_PLANEBACK) ? -surf->plane->normal : surf->plane->normal;

					DrawVectorDir( drawvecsrc, vec_x, 8, 1, 0, 0, 1 );
					DrawVectorDir( drawvecsrc, vec_y, 8, 0, 1, 0, 1 );
					DrawVectorDir( drawvecsrc, vec_z, 8, 0, 0, 1, 1 );

					Vector tmp = g_vecZero;
					VectorMA( tmp, outcolor[0] * 2.0f - 1.0f, vec_x, tmp );
					VectorMA( tmp, outcolor[1] * 2.0f - 1.0f, vec_y, tmp );
					VectorMA( tmp, outcolor[2] * 2.0f - 1.0f, vec_z, tmp );
					DrawVectorDir( drawvecsrc, tmp, 300, 1, 1, 0, 1 );
					gdist = tmp.Length();					
				}
				else if( surf->styles[style] == BUMP_ADDLIGHT_STYLE )
				{
					test_add_color.r = lightmap->r;
					test_add_color.g = lightmap->g;
					test_add_color.b = lightmap->b;
				}
				else if( surf->styles[style] == BUMP_BASELIGHT_STYLE )
				{
					test_base_color.r = lightmap->r;
					test_base_color.g = lightmap->g;
					test_base_color.b = lightmap->b;
				}

				lightmap += ((surf->extents[0] / LM_SAMPLE_SIZE ) + 1) * ((surf->extents[1] / LM_SAMPLE_SIZE ) + 1);
			}
		}		

		return r;
	}

	// go down back side
	return RecursiveDrawTextureVecs( node->children[!side], mid, end );
}

void DrawTextureVecs( void )
{
	gdist = 0;

	if( !cv_bumpvecs->value )
		return;

	if( !worldmodel || !worldmodel->lightdata )
		return;

	Vector end;
	VectorMA( RI.vieworg, 1024, RI.vforward, end );

	pmtrace_t pmtrace;
	gEngfuncs.pEventAPI->EV_SetTraceHull( 2 );
	gEngfuncs.pEventAPI->EV_PlayerTrace( RI.vieworg, end, PM_STUDIO_IGNORE|PM_WORLD_ONLY, -1, &pmtrace );					

	if( pmtrace.fraction != 1 )
	{
		VectorMA( pmtrace.endpos, 2, pmtrace.plane.normal, drawvecsrc );
		RecursiveDrawTextureVecs( worldmodel->nodes, RI.vieworg, end );
	}
}

void PrintOtherDebugInfo( void )
{
	if( gdist )
	{
		char msg[256];
		Q_snprintf( msg, sizeof( msg ), "light vector length: %f\n", gdist);
		DrawConsoleString( XRES(10), YRES(50), msg );

		Q_snprintf( msg, sizeof( msg ), "add light: {%d, %d, %d}\n", test_add_color.r, test_add_color.g, test_add_color.b );
		DrawConsoleString( XRES(10), YRES(65), msg );

		Q_snprintf( msg, sizeof( msg ), "base light: {%d, %d, %d}\n", test_base_color.r, test_base_color.g, test_base_color.b );
		DrawConsoleString( XRES(10), YRES(80), msg );
	}
}