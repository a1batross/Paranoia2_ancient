//
// written by BUzer for HL: Paranoia modification
//
//		2006

#include "hud.h"
#include "cl_util.h"
#include "const.h"
#include "mathlib.h"
#include "entity_state.h"
#include "cl_entity.h"
#include "triangleapi.h"
#include "com_model.h"
#include "r_efx.h"
#include "event_api.h"
#include "pm_defs.h"
#include "pmtrace.h"
#include "gl_local.h"

#define GLOW_INTERP_SPEED		2

cvar_t	*v_glows;
cvar_t	*v_glowstraces;
cvar_t	*v_glowsdebug;

int GlowFilterEntities( int type, struct cl_entity_s *ent, const char *modelname )
{
	if( ent->curstate.rendermode == kRenderGlow && ent->curstate.renderfx == kRenderFxNoDissipation )
	{
		if( !v_glows->value || tr.num_glow_sprites >= MAX_GLOW_SPRITES )
			return false;

		HSPRITE hsprTexture = SPR_Load( modelname );
		if( !hsprTexture )
		{
			ALERT( at_error, "GlowFilterEntities: sprite %s not loaded!\n", modelname );
			return false;
		}

		tr.glow_sprites[tr.num_glow_sprites].psprite = (model_t *)gEngfuncs.GetSpritePointer( hsprTexture );
		tr.glow_sprites[tr.num_glow_sprites].width = (float)SPR_Width( hsprTexture, 0 ) * ent->curstate.scale * 0.5f;
		tr.glow_sprites[tr.num_glow_sprites].height= (float)SPR_Height( hsprTexture, 0 ) * ent->curstate.scale * 0.5f;
		tr.glow_sprites[tr.num_glow_sprites].ent = ent;
		tr.num_glow_sprites++;
		return true;
	}

	return false;
}

// ent->latched.sequencetime keeps previous alpha for interpolation
void DrawGlows( void )
{
	if( !tr.num_glow_sprites || !RP_NORMALPASS( ))
		return;

	pglDisable( GL_DEPTH_TEST );

	float matrix[3][4];
	AngleMatrix( RI.refdef.viewangles, matrix );	// calc view matrix

	for( int i = 0; i < tr.num_glow_sprites; i++ )
	{
		cl_glow_sprite_t	*pGlow = &tr.glow_sprites[i];

		matrix[0][3] = pGlow->ent->curstate.origin[0]; // write origin to matrix
		matrix[1][3] = pGlow->ent->curstate.origin[1];
		matrix[2][3] = pGlow->ent->curstate.origin[2];

		float dst = DotProduct( RI.vforward, pGlow->ent->curstate.origin - RI.vieworg );
		dst = bound( 0.0f, dst, 64.0f );
		dst = dst / 64.0f;
		
		int numtraces = v_glowstraces->value;
		if( numtraces < 1 ) numtraces = 1;
		
		Vector aleft, aright, left, right, dist, step;
		left[0] = 0; left[1] = -pGlow->width / 5; left[2] = 0;
		right[0] = 0; right[1] = pGlow->width / 5; right[2] = 0;
		VectorTransform( left, matrix, aleft );
		VectorTransform( right, matrix, aright );
		VectorSubtract( aright, aleft, dist );
		VectorScale( dist, 1.0f / (numtraces + 1), step );
		float frac = 1.0f / numtraces;
		float totalfrac = 0;

		gEngfuncs.pEventAPI->EV_SetTraceHull( 2 );
		for( int j = 0; j < numtraces; j++ )
		{
			Vector start;
			VectorMA( aleft, j+1, step, start );

			pmtrace_t pmtrace;
			gEngfuncs.pEventAPI->EV_PlayerTrace( RI.vieworg, start, PM_GLASS_IGNORE|PM_STUDIO_IGNORE, -1, &pmtrace );

			if( pmtrace.fraction == 1.0f )
			{
				if( v_glowsdebug->value )
					DrawVector( start, RI.vieworg + Vector( 0, 0, -1 ), 0, 1, 0, 1 );
				totalfrac += frac;
			}
			else
			{
				if( v_glowsdebug->value )
					DrawVector( start, RI.vieworg + Vector( 0, 0, -1 ), 1, 0, 0, 1 );
			}
		}

		float targetalpha = totalfrac;

		if( pGlow->ent->latched.sequencetime > targetalpha )
		{
			pGlow->ent->latched.sequencetime -= RI.refdef.frametime * GLOW_INTERP_SPEED;
			if( pGlow->ent->latched.sequencetime <= targetalpha )
				pGlow->ent->latched.sequencetime = targetalpha;
		}
		else if( pGlow->ent->latched.sequencetime < targetalpha )
		{
			pGlow->ent->latched.sequencetime += RI.refdef.frametime * GLOW_INTERP_SPEED;
			if( pGlow->ent->latched.sequencetime >= targetalpha )
				pGlow->ent->latched.sequencetime = targetalpha;
		}

	//	gEngfuncs.Con_Printf("target: %f, current: %f\n", targetalpha, pGlow->ent->latched.sequencetime);

		gEngfuncs.pTriAPI->RenderMode( kRenderTransAdd );
		gEngfuncs.pTriAPI->CullFace( TRI_NONE ); // no culling
		gEngfuncs.pTriAPI->SpriteTexture( pGlow->psprite, 0 );		
		gEngfuncs.pTriAPI->Color4f( (float)pGlow->ent->curstate.rendercolor.r / 255.0f,
			(float)pGlow->ent->curstate.rendercolor.g / 255.0f,
			(float)pGlow->ent->curstate.rendercolor.b / 255.0f,
			(float)pGlow->ent->curstate.renderamt / 255.0f );
		gEngfuncs.pTriAPI->Brightness(((float)pGlow->ent->curstate.renderamt / 255.0f ) * pGlow->ent->latched.sequencetime * dst );

		gEngfuncs.pTriAPI->Begin( TRI_QUADS );

			gEngfuncs.pTriAPI->TexCoord2f( 0, 0 );
			SetPoint( 0, pGlow->width, tr.glow_sprites[i].height, matrix );

			gEngfuncs.pTriAPI->TexCoord2f( 0, 1 );
			SetPoint( 0, pGlow->width, -tr.glow_sprites[i].height, matrix );

			gEngfuncs.pTriAPI->TexCoord2f( 1, 1 );
			SetPoint( 0, -pGlow->width, -tr.glow_sprites[i].height, matrix );

			gEngfuncs.pTriAPI->TexCoord2f( 1, 0 );
			SetPoint( 0, -pGlow->width, tr.glow_sprites[i].height, matrix );
			
		gEngfuncs.pTriAPI->End();

	//	gEngfuncs.Con_Printf("draw glow at %f, %f, %f\n", pGlow->pos[0], pGlow->pos[1], pGlow->pos[2]);
	//	gEngfuncs.Con_Printf("with color %f, %f, %f, %f\n", pGlow->r, pGlow->g, pGlow->b, pGlow->a);
	//	gEngfuncs.Con_Printf("with width %f and height %f\n", pGlow->width, pGlow->height);
		
	}

	gEngfuncs.pTriAPI->RenderMode( kRenderNormal );
	gEngfuncs.pTriAPI->CullFace( TRI_FRONT );
	pglEnable( GL_DEPTH_TEST );
}

		
void InitGlows( void )
{
	v_glows = CVAR_REGISTER( "gl_glows", "1", 0 );
	v_glowstraces = CVAR_REGISTER( "gl_glowstraces", "5", 0 );
	v_glowsdebug = CVAR_REGISTER( "gl_glowsdebug", "0", 0 );
}