
// ==============================
// grass.cpp
// written by BUzer for HL: Paranoia modification
// ==============================

#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"
#include "const.h"
#include "entity_types.h"
#include "cdll_int.h"
#include "pm_defs.h"
#include "event_api.h"
#include "com_model.h"
#include "r_studioint.h"
#include "triangleapi.h"
#include "gl_local.h"
#include "gl_studio.h"
#include "gl_sprite.h"

#define MAX_GRASS_SPRITES	1000
#define MAX_GRASS_MODELS	100
#define MAX_GRASS_TYPES	32
#define MAX_TYPE_TEXTURES	8
#define FADE_RANGE		0.7f

enum
{
	MODE_SCREEN_PARALLEL = 0,
	MODE_FACING_PLAYER,
	MODE_FIXED,
	MODE_STUDIO,
};


typedef struct grass_sprite_s
{
	Vector		pos;
	Vector		lightcolor;
	int		visible;
	float		hscale;
	float		vscale;
	Vector		vecside; // for fixed sprites
} grass_sprite_t;

typedef struct grass_type_s
{
	float		radius;
	float		baseheight;
	model_t		*sprite; // model or sprite pointer
	int		modelindex; // for models
	float		sprheight;  // height for sprites, scale for models
	float		sprhalfwidth; // for sprites
	int		mode;
	int		startindex;
	int		numents;
	int		state; // see enum below
	int		numtextures;
	char		worldtextures[MAX_TYPE_TEXTURES][16];
} grass_type_t;


enum
{
	TYPE_STATE_NORMAL = 0,
	TYPE_STATE_UNINITIALIZED,
};

grass_sprite_t	grass_sprites[MAX_GRASS_SPRITES];
grass_type_t	grass_types[MAX_GRASS_TYPES];
cl_entity_t	grass_models[MAX_GRASS_MODELS]; // omg, what a giant waste of memory..

static int	sprites_count = 0;
static int	models_count = 0;
static int	types_count = 0;
static int 	hasdynlights = 0;

// grass info msg
int MsgGrassInfo(const char *pszName, int iSize, void *pbuf)
{
	// got new grass type:
	//		coord radius
	//		coord height
	//		short numents
	//		short (sprite scale)*256
	//		byte  mode
	//		byte  numtextures
	//		strings[numtextures] texture names
	//		string sprite name

	if( types_count == MAX_GRASS_TYPES )
	{
		ALERT( at_warning, "new grass type ignored (MAX_GRASS_TYPES exceeded)\n");
		return 0;
	}

	grass_type_t *t = &grass_types[types_count];
	BEGIN_READ( pbuf, iSize );

	t->state = TYPE_STATE_UNINITIALIZED;
	t->radius = READ_COORD();
	t->baseheight = READ_COORD();
	t->numents = READ_SHORT();
	float sprscale = (float)READ_SHORT() / 256.0f;
	t->mode = READ_BYTE();
	t->numtextures = READ_BYTE();

	for( int i = 0; i < t->numtextures; i++ )
		strcpy( t->worldtextures[i], READ_STRING() );
	char *sprname = READ_STRING();

	if ( IEngineStudio.IsHardware() != 1 )
		return 1; // only hardware..

	if( t->mode == MODE_STUDIO )
	{
		t->startindex = models_count;
		t->sprheight = sprscale;

		if(( t->startindex + t->numents ) > MAX_GRASS_MODELS )
		{
			ALERT( at_warning, "new grass type ignored (MAX_GRASS_MODELS exceeded)\n" );
			return 1;
		}

		// load model
		t->sprite = gEngfuncs.CL_LoadModel( sprname, &t->modelindex );

		if( !t->sprite || !t->modelindex )
		{
			ALERT( at_warning, "new grass type ignored (cannot load model %s)\n", sprname );
			return 1;
		}
		models_count += t->numents;
	}
	else
	{
		t->startindex = sprites_count;

		if(( t->startindex + t->numents ) > MAX_GRASS_SPRITES )
		{
			ALERT( at_warning, "new grass type ignored (MAX_GRASS_SPRITES exceeded)\n" );
			return 1;
		}

		// load sprite
		HSPRITE hSpr = SPR_Load( sprname );

		if( !hSpr )
		{
			ALERT( at_warning, "new grass type ignored (cannot load sprite %s)\n", sprname );
			return 1;
		}

		t->sprhalfwidth = (float)SPR_Width( hSpr, 0 ) * sprscale * 0.5f;
		t->sprheight = (float)SPR_Height( hSpr, 0 ) * sprscale;
		t->sprite = (struct model_s *)gEngfuncs.GetSpritePointer( hSpr );

		if( !t->sprite )
		{
			ALERT( at_warning, "new grass type ignored (cannot get sprptr for %s)\n", sprname );
			return 1;
		}

		sprites_count += t->numents;
	}

	types_count++;	
	return 1;
}

// create cvars here, hook messages, etc..
void GrassInit( void )
{
	gEngfuncs.pfnHookUserMsg( "GrassInfo", MsgGrassInfo );
}

// reset
void GrassVidInit( void )
{
	sprites_count = 0;
	types_count = 0;
	models_count = 0;
	memset( grass_models, 0, sizeof( grass_models ));
}

float AlphaDist( const Vector &sprpos, const Vector &playerpos, float maxdist )
{
	float dist_x = playerpos[0] - sprpos[0];
	float dist_y = playerpos[1] - sprpos[1];

	if( dist_x < 0 ) dist_x = -dist_x;
	if( dist_y < 0 ) dist_y = -dist_y;

	float fract = dist_x > dist_y ? dist_x : dist_y;
	fract /= maxdist;

	if( fract >= 1.0f ) return 0;
	if( fract <= FADE_RANGE ) return 1.0;

	return (1.0f - ((fract - FADE_RANGE) * (1.0f / (1.0f - FADE_RANGE))));
}


// =========================================================
//		sprites
// =========================================================
void GetSpriteRandomVec( Vector &vec )
{
	SinCos( RANDOM_FLOAT( 0.0f, M_PI * 2 ), &vec.x, &vec.y );
	vec.z = 0;
}

// corrects pos[2], checks world texture, sets lighting and other settings
void PlaceGrassSprite( grass_type_t *t, grass_sprite_t *s )
{
	Vector temp;

	s->visible = 0;
	s->pos[2] = t->baseheight;
	temp = s->pos;
	temp.z -= 8192;

	pmtrace_t ptr;
	gEngfuncs.pEventAPI->EV_SetTraceHull( 2 );
	gEngfuncs.pEventAPI->EV_PlayerTrace( s->pos, temp, PM_STUDIO_IGNORE, -1, &ptr );

	if( ptr.startsolid || ptr.allsolid || ptr.fraction == 1.0f )
		return;

	const char *texname = gEngfuncs.pEventAPI->EV_TraceTexture( ptr.ent, s->pos, temp );
	if( !texname ) return;

	for( int cht = 0; cht < t->numtextures; cht++ )
	{
		if( !strcmp( t->worldtextures[cht], texname ))
		{
			s->visible = 1;
			s->vscale = RANDOM_FLOAT( 0.7f, 1.0f );
			s->hscale = RANDOM_LONG( 0, 1 ) ? s->vscale : -s->vscale;

			lightinfo_t light;
			R_LightForPoint( s->pos, &light, false ); // get static lighting
			s->lightcolor = light.ambient;
			s->pos = ptr.endpos;
			if( t->mode == MODE_FIXED )
				GetSpriteRandomVec( s->vecside );
			break;
		}
	}
}

void UpdateGrassSpritePosition( grass_type_t *t, grass_sprite_t *s, const Vector &mins, const Vector &maxs )
{
	int shift_x = 0, shift_y = 0;
	float radx2 = maxs[0] - mins[0];

	if( s->pos[0] > maxs[0] )
	{
		do { s->pos[0] -= radx2; } while( s->pos[0] > maxs[0] );
		shift_x = 1;
	}
	else if( s->pos[0] < mins[0] )
	{
		do { s->pos[0] += radx2; } while( s->pos[0] < maxs[0] );
		shift_x = 1;
	}

	if( s->pos[1] > maxs[1] )
	{
		do { s->pos[1] -= radx2; } while( s->pos[1] > maxs[1] );
		shift_y = 1;
	}
	else if( s->pos[1] < mins[1] )
	{
		do { s->pos[1] += radx2; } while( s->pos[1] < maxs[1] );
		shift_y = 1;
	}

	if( shift_x )
	{
		if( !shift_y )
			s->pos[1] = RANDOM_FLOAT( mins[1], maxs[1] );
		PlaceGrassSprite( t, s );
	}
	else if( shift_y )
	{
		s->pos[0] = RANDOM_FLOAT( mins[0], maxs[0] );
		PlaceGrassSprite( t, s );
	}
}

_forceinline static void DrawSpriteQuad( const Vector &pos, const Vector &side, const float sprheight )
{
	pglBegin( GL_QUADS );
		pglTexCoord2f( 0.0f, 0.0f );
		pglVertex3f( pos[0] - side[1], pos[1] + side[0], pos[2] + sprheight );
		pglTexCoord2f( 0.0f, 1.0f );
		pglVertex3f( pos[0] - side[1], pos[1] + side[0], pos[2] );
		pglTexCoord2f( 1.0f, 1.0f );
		pglVertex3f( pos[0] + side[1], pos[1] - side[0], pos[2] );
		pglTexCoord2f( 1.0f, 0.0f );
		pglVertex3f( pos[0] + side[1], pos[1] - side[0], pos[2] + sprheight );
	pglEnd();
}

void GrassGammaCorrection( bool enable )
{
	if( r_fullbright->value ) return; // don't need

	if( enable )
	{
		pglTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_ARB );
		pglTexEnvi( GL_TEXTURE_ENV, GL_COMBINE_RGB_ARB, GL_MODULATE );
		pglTexEnvi( GL_TEXTURE_ENV, GL_SOURCE0_RGB_ARB, GL_PREVIOUS_ARB );
		pglTexEnvi( GL_TEXTURE_ENV, GL_SOURCE1_RGB_ARB, GL_TEXTURE );
		pglTexEnvi( GL_TEXTURE_ENV, GL_RGB_SCALE_ARB, 2 );
	}
	else
	{
		pglTexEnvi( GL_TEXTURE_ENV, GL_RGB_SCALE_ARB, 1 );
		pglTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
	}
}

void DrawGrassSprite( grass_sprite_t *s, grass_type_t *t, const Vector &pos, float alpha )
{
	Vector vecSide;
	int l;

	if( alpha <= 0.1f ) return;	// faded

	int spriteTexture = R_GetSpriteTexture( t->sprite, 0 );
	if( !spriteTexture ) return;	// bad sprite?

	if( t->mode == MODE_FACING_PLAYER ) 
	{
		vecSide[0] = pos[0] - s->pos[0];
		vecSide[1] = pos[1] - s->pos[1];
		vecSide[2] = 0;
		vecSide = vecSide.Normalize();
		vecSide[0] *= t->sprhalfwidth * s->hscale;
		vecSide[1] *= t->sprhalfwidth * s->hscale;
	}
	else
	{
		Vector vecSide = pos * (t->sprhalfwidth * s->hscale);
	}

	float sprheight = t->sprheight * s->vscale;
	int numlights = 0;

	// go through dynamic lights list
	float time = GET_CLIENT_TIME();
	DynamicLight *pl;

	for( l = 0, pl = cl_dlights; l < MAX_DLIGHTS && hasdynlights; l++, pl++ )
	{
		if( pl->die < time || !pl->radius )
			continue;

		if( R_CullBoxExt( pl->frustum, s->pos + t->sprite->mins, s->pos + t->sprite->maxs, pl->clipflags ))
			continue;

		numlights++;
	}

	if( hasdynlights && numlights )
	{
		GL_Bind( GL_TEXTURE0, spriteTexture );
		pglDisable( GL_BLEND );
		pglDepthMask( GL_TRUE );
		pglEnable( GL_ALPHA_TEST );
		pglAlphaFunc( GL_GEQUAL, 0.5f );
		pglTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_ARB );
		pglTexEnvi( GL_TEXTURE_ENV, GL_SOURCE0_RGB_ARB, GL_PREVIOUS_ARB );
		pglTexEnvi( GL_TEXTURE_ENV, GL_COMBINE_RGB_ARB, GL_REPLACE );
		pglColor4f( s->lightcolor[0], s->lightcolor[1], s->lightcolor[2], alpha );
		DrawSpriteQuad( s->pos, vecSide, sprheight );
		pglDisable( GL_ALPHA_TEST );

		for( l = 0, pl = cl_dlights; l < MAX_DLIGHTS; l++, pl++ )
		{
			if( pl->die < time || !pl->radius )
				continue;

			if( R_CullBoxExt( pl->frustum, s->pos + t->sprite->mins, s->pos + t->sprite->maxs, pl->clipflags ))
				continue;

			// lit faces
			R_BeginDrawProjection( pl );
			DrawSpriteQuad( s->pos, vecSide, sprheight );
			R_EndDrawProjection();
		}

		GL_Bind( GL_TEXTURE0, spriteTexture );
		pglDisable( GL_ALPHA_TEST );
		pglDepthFunc( GL_EQUAL );
		pglEnable( GL_BLEND );
		pglBlendFunc( GL_DST_COLOR, GL_SRC_COLOR );
		pglTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE );
		DrawSpriteQuad( s->pos, vecSide, sprheight );
		pglDepthFunc( GL_LEQUAL );
	}
	else
	{
		GL_Bind( GL_TEXTURE0, spriteTexture );
		pglDisable( GL_BLEND );
		pglDepthMask( GL_TRUE );
		pglEnable( GL_ALPHA_TEST );
		pglAlphaFunc( GL_GEQUAL, 0.5f );
		pglTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );

		if( !( RI.params & RP_SHADOWVIEW ))
		{
			pglColor4f( s->lightcolor[0], s->lightcolor[1], s->lightcolor[2], alpha );
			GrassGammaCorrection( true );
		}
		DrawSpriteQuad( s->pos, vecSide, sprheight );

		if( !( RI.params & RP_SHADOWVIEW ))
			GrassGammaCorrection( false );
	}
}

void GrassDraw( void )
{
	cl_entity_t *player = gEngfuncs.GetLocalPlayer();
	grass_type_t *t = grass_types;
	hasdynlights = FALSE;

	if( RI.params & RP_SHADOWVIEW )
	{
		pglEnable( GL_TEXTURE_2D );
	}
	else
	{
		gEngfuncs.pTriAPI->RenderMode( kRenderTransAlpha );
		hasdynlights = HasDynamicLights();
	}

	gEngfuncs.pTriAPI->CullFace( TRI_NONE );

	for( int type = 0; type < types_count; type++, t++ )
	{
		if( t->mode == MODE_STUDIO )
			continue;

		Vector mins = player->origin;
		Vector maxs = player->origin;
		mins[0] -= t->radius; mins[1] -= t->radius;
		maxs[0] += t->radius; maxs[1] += t->radius;

		if( t->state == TYPE_STATE_UNINITIALIZED )
		{
			grass_sprite_t *s = &grass_sprites[t->startindex];
			for( int sprite = 0; sprite < t->numents; sprite++, s++ )
			{
				s->pos[0] = RANDOM_FLOAT( mins[0], maxs[0] );
				s->pos[1] = RANDOM_FLOAT( mins[1], maxs[1] );
				PlaceGrassSprite( t, s );
			}
			t->state = TYPE_STATE_NORMAL;
		}

		if( t->state != TYPE_STATE_NORMAL )
			continue;	// bad sprite?

		int sprite;
		Vector vec;

		gEngfuncs.pTriAPI->SpriteTexture( t->sprite, 0 );
		grass_sprite_t *s = &grass_sprites[t->startindex];
		Vector vecside;

		for( sprite = 0; sprite < t->numents; sprite++, s++ )
		{
			switch( t->mode )
			{
			case MODE_SCREEN_PARALLEL: vecside = RI.refdef.forward; break;
			case MODE_FACING_PLAYER: vecside = player->origin; break;
			case MODE_FIXED: vecside = s->vecside; break;
			}

			UpdateGrassSpritePosition( t, s, mins, maxs );

			if( !s->visible || R_CullBoxExt( RI.frustum, s->pos + t->sprite->mins, s->pos + t->sprite->maxs, RI.clipFlags ))
				continue;

			DrawGrassSprite( s, t, vecside, AlphaDist( s->pos, player->origin, t->radius ));
		}
	} // grass types

	gEngfuncs.pTriAPI->CullFace( TRI_FRONT );

	if( RI.params & RP_SHADOWVIEW )
	{
		pglDisable( GL_ALPHA_TEST );
		pglDisable( GL_TEXTURE_2D );
	}
}


// =========================================================
//		models
// =========================================================
// origin, angles
// curstate.iuser1 - is visible
// curstate.scale - scale

void PlaceGrassModel( grass_type_t *t, cl_entity_t *s )
{
	Vector temp;
	s->curstate.iuser1 = 0;
	s->origin[2] = t->baseheight;
	VectorCopy( s->origin, temp );
	temp[2] -= 8192;

	pmtrace_t ptr;
	gEngfuncs.pEventAPI->EV_SetTraceHull( 2 );
	gEngfuncs.pEventAPI->EV_PlayerTrace( s->origin, temp, PM_STUDIO_IGNORE, -1, &ptr );

	if( ptr.startsolid || ptr.allsolid || ptr.fraction == 1.0f )
		return;

	const char *texname = gEngfuncs.pEventAPI->EV_TraceTexture( ptr.ent, s->origin, temp );
	if( !texname ) return;

	for( int cht = 0; cht < t->numtextures; cht++ )
	{
		if( !strcmp( t->worldtextures[cht], texname ))
		{
			s->curstate.iuser1 = 1;
			s->curstate.scale = RANDOM_FLOAT( 0.7f, 1.0f ) * t->sprheight;
			s->origin = ptr.endpos;
			break;
		}
	}
}

void UpdateGrassModelPosition( grass_type_t *t, cl_entity_t *s, const Vector &mins, const Vector &maxs )
{
	int shift_x = 0, shift_y = 0;
	float radx2 = maxs[0] - mins[0];

	if( s->origin[0] > maxs[0] )
	{
		do { s->origin[0] -= radx2; } while( s->origin[0] > maxs[0] );
		shift_x = 1;
	}
	else if( s->origin[0] < mins[0] )
	{
		do { s->origin[0] += radx2; } while( s->origin[0] < maxs[0] );
		shift_x = 1;
	}

	if( s->origin[1] > maxs[1] )
	{
		do { s->origin[1] -= radx2; } while( s->origin[1] > maxs[1] );
		shift_y = 1;
	}
	else if( s->origin[1] < mins[1] )
	{
		do { s->origin[1] += radx2; } while( s->origin[1] < maxs[1] );
		shift_y = 1;
	}

	if( shift_x )
	{
		if( !shift_y )
			s->origin[1] = RANDOM_FLOAT( mins[1], maxs[1] );
		PlaceGrassModel( t, s );
	}
	else if( shift_y )
	{
		s->origin[0] = RANDOM_FLOAT( mins[0], maxs[0] );
		PlaceGrassModel( t, s );
	}
}

void GrassCreateEntities( void )
{
	cl_entity_t *player = gEngfuncs.GetLocalPlayer();
	grass_type_t *t = grass_types;

	for( int type = 0; type < types_count; type++, t++ )
	{
		if( t->mode != MODE_STUDIO )
			continue;

		Vector mins, maxs;
		VectorCopy( player->origin, mins );
		VectorCopy( player->origin, maxs );

		mins[0] -= t->radius; mins[1] -= t->radius;
		maxs[0] += t->radius; maxs[1] += t->radius;

		if( t->state == TYPE_STATE_UNINITIALIZED )
		{
			cl_entity_t *s = &grass_models[t->startindex];

			for( int model = 0; model < t->numents; model++, s++ )
			{
				s->origin[0] = gEngfuncs.pfnRandomFloat( mins[0], maxs[0] );
				s->origin[1] = gEngfuncs.pfnRandomFloat( mins[1], maxs[1] );
				s->model = t->sprite;
				s->curstate.modelindex = t->modelindex;
				PlaceGrassModel( t, s );
			}
			t->state = TYPE_STATE_NORMAL;
		}
		else
		{
			cl_entity_t *s = &grass_models[t->startindex];

			for( int model = 0; model < t->numents; model++, s++ )
			{
				UpdateGrassModelPosition( t, s, mins, maxs );
				if( s->curstate.iuser1 )
				{
					float a = AlphaDist( s->origin, player->origin, t->radius );

					if( a < 1.0 )
					{
						s->curstate.rendermode = kRenderTransAlpha;
						s->curstate.renderamt = a * 255;
					}
					else
					{
						s->curstate.rendermode = kRenderNormal;
						s->curstate.renderamt = 255;
					}

					gEngfuncs.CL_CreateVisibleEntity( ET_NORMAL, s );
				}
			} // models
		}
	} // grass types
}