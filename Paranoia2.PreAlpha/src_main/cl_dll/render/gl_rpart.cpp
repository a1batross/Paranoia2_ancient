/*
gl_rpart.cpp - quake-like particle system
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
#include <mathlib.h>
#include "gl_local.h"
#include "com_model.h"
#include "r_studioint.h"
#include "gl_rpart.h"
#include "pm_defs.h"
#include "event_api.h"
#include "triangleapi.h"
#include "pm_movevars.h"

CQuakePartSystem	g_pParticles;

bool CQuakePart :: Evaluate( float gravity )
{
	float	curAlpha, curRadius, curLength;
	Vector	curColor, curOrigin, lastOrigin, curVelocity;
	float	time, time2;

	time = RI.refdef.time - flTime;
	time2 = time * time;

	curAlpha = alpha + alphaVelocity * time;
	curRadius = radius + radiusVelocity * time;
	curLength = length + lengthVelocity * time;

	if( curAlpha <= 0.0f || curRadius <= 0.0f || curLength <= 0.0f )
	{
		// faded out
		return false;
	}

	curColor = color + colorVelocity * time;

	curOrigin.x = origin.x + velocity.x * time + accel.x * time2;
	curOrigin.y = origin.y + velocity.y * time + accel.y * time2;
	curOrigin.z = origin.z + velocity.z * time + accel.z * time2 * gravity;

	if( FBitSet( flags, FPART_UNDERWATER ))
	{
		// underwater particle
		lastOrigin.Init( curOrigin.x, curOrigin.y, curOrigin.z + curRadius );

		int contents = POINT_CONTENTS( lastOrigin );

		if( contents != CONTENTS_WATER && contents != CONTENTS_SLIME && contents != CONTENTS_LAVA )
		{
			// not underwater
			return false;
		}
	}

	if( FBitSet( flags, FPART_FRICTION ))
	{
		// water friction affected particle
		int contents = POINT_CONTENTS( curOrigin );

		if( contents <= CONTENTS_WATER && contents >= CONTENTS_LAVA )
		{
			// add friction		
			switch( contents )
			{
			case CONTENTS_WATER:
				velocity *= 0.25f;
				accel *= 0.25f;
				break;
			case CONTENTS_SLIME:
				velocity *= 0.20f;
				accel *= 0.20f;
				break;
			case CONTENTS_LAVA:
				velocity *= 0.10f;
				accel *= 0.10f;
				break;
			}
			
			// don't add friction again
			flags &= ~FPART_FRICTION;
			curLength = 1.0f;
				
			// reset
			flTime = RI.refdef.time;
			origin = curOrigin;
			color = curColor;
			alpha = curAlpha;
			radius = curRadius;

			// don't stretch
			flags &= ~FPART_STRETCH;
			length = curLength;
			lengthVelocity = 0.0f;
		}
	}

	if( FBitSet( flags, FPART_BOUNCE ))
	{
		Vector	mins( -radius, -radius, -radius );
		Vector	maxs(  radius,  radius,  radius );

		// bouncy particle
		pmtrace_t pmtrace;
		gEngfuncs.pEventAPI->EV_SetTraceHull( 2 );
		gEngfuncs.pEventAPI->EV_PlayerTrace( oldorigin, origin, PM_STUDIO_IGNORE, -1, &pmtrace );

		if( pmtrace.fraction > 0.0f && pmtrace.fraction < 1.0f )
		{
			// reflect velocity
			time = RI.refdef.time - (RI.refdef.frametime + RI.refdef.frametime * pmtrace.fraction);
			time = (time - flTime);

			curVelocity.x = velocity.x;
			curVelocity.y = velocity.y;
			curVelocity.z = velocity.z + accel.z * gravity * time;

			float d = DotProduct( curVelocity, pmtrace.plane.normal ) * -1.0f;
			velocity = curVelocity + pmtrace.plane.normal * d;
			velocity *= bounceFactor;

			// check for stop or slide along the plane
			if( pmtrace.plane.normal.z > 0 && velocity.z < 1.0f )
			{
				if( pmtrace.plane.normal.z == 1.0f )
				{
					velocity = g_vecZero;
					accel = g_vecZero;
					flags &= ~FPART_BOUNCE;
				}
				else
				{
					// FIXME: check for new plane or free fall
					float dot = DotProduct( velocity, pmtrace.plane.normal );
					velocity = velocity + (pmtrace.plane.normal * -dot);

					dot = DotProduct( accel, pmtrace.plane.normal );
					accel = accel + (pmtrace.plane.normal * -dot);
				}
			}

			curOrigin = pmtrace.endpos;
			curLength = 1;

			// reset
			flTime = RI.refdef.time;
			origin = curOrigin;
			color = curColor;
			alpha = curAlpha;
			radius = curRadius;

			// don't stretch
			flags &= ~FPART_STRETCH;
			length = curLength;
			lengthVelocity = 0.0f;
		}
	}
	
	// save current origin if needed
	if( FBitSet( flags, ( FPART_BOUNCE|FPART_STRETCH )))
	{
		lastOrigin = oldorigin;
		oldorigin = curOrigin;
	}

	if( FBitSet( flags, FPART_VERTEXLIGHT ))
	{
		lightinfo_t lightinfo;

		// vertex lit particle
		R_LightForPoint( curOrigin, &lightinfo, false );
		curColor *= lightinfo.ambient;
	}

	if( FBitSet( flags, FPART_INSTANT ))
	{
		// instant particle
		alpha = 0.0f;
		alphaVelocity = 0.0f;
	}

	if( curRadius == 1.0f )
	{
		float	scale = 0.0f;

		// hack a scale up to keep quake particles from disapearing
		scale += (curOrigin[0] - RI.vieworg[0]) * RI.vforward[0];
		scale += (curOrigin[1] - RI.vieworg[1]) * RI.vforward[1];
		scale += (curOrigin[2] - RI.vieworg[2]) * RI.vforward[2];
		if( scale >= 20 ) curRadius = 1.0f + scale * 0.004f;
	}

	Vector	axis[3], verts[4];

	// prepare to draw
	if( curLength != 1.0f )
	{
		// find orientation vectors
		axis[0] = RI.vieworg - origin;
		axis[1] = oldorigin - origin;
		axis[2] = CrossProduct( axis[0], axis[1] );

		axis[1] = axis[1].Normalize();
		axis[2] = axis[2].Normalize();
//		VectorNormalizeFast( axis[1] );
//		VectorNormalizeFast( axis[2] );

		// find normal
		axis[0] = CrossProduct( axis[1], axis[2] );
//		VectorNormalizeFast( axis[0] );
		axis[0] = axis[0].Normalize();

		oldorigin = origin + ( axis[1] * -curLength );
		axis[2] *= radius;

		// setup vertexes
		verts[0] = lastOrigin + axis[2];
		verts[1] = curOrigin + axis[2];
		verts[2] = curOrigin - axis[2];
		verts[3] = lastOrigin - axis[2];
	}
	else
	{
		if( rotation )
		{
			// Rotate it around its normal
			RotatePointAroundVector( axis[1], RI.vforward, RI.vright, rotation );
			axis[2] = CrossProduct( RI.vforward, axis[1] );

			// the normal should point at the viewer
			axis[0] = -RI.vforward;

			// Scale the axes by radius
			axis[1] *= curRadius;
			axis[2] *= curRadius;
		}
		else
		{
			// the normal should point at the viewer
			axis[0] = -RI.vforward;

			// scale the axes by radius
			axis[1] = RI.vright * curRadius;
			axis[2] = RI.vup * curRadius;
		}

		verts[0] = curOrigin + axis[1] + axis[2];
		verts[1] = curOrigin - axis[1] + axis[2];
		verts[2] = curOrigin - axis[1] - axis[2];
		verts[3] = curOrigin + axis[1] - axis[2];
	}

	// draw the particle
	GL_Bind( GL_TEXTURE0, m_hTexture );

	pglEnable( GL_BLEND );
//	pglEnable( GL_ALPHA_TEST );
//	pglAlphaFunc( GL_GREATER, 0.0f );
	if( FBitSet( flags, FPART_ADDITIVE ))
		pglBlendFunc( GL_SRC_ALPHA, GL_ONE );
	else pglBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
	pglTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
/*
	pglTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_ARB );
	pglTexEnvi( GL_TEXTURE_ENV, GL_COMBINE_RGB_ARB, GL_MODULATE );
	pglTexEnvi( GL_TEXTURE_ENV, GL_SOURCE0_RGB_ARB, GL_PREVIOUS_ARB );
	pglTexEnvi( GL_TEXTURE_ENV, GL_SOURCE1_RGB_ARB, GL_TEXTURE );
	pglTexEnvi( GL_TEXTURE_ENV, GL_RGB_SCALE_ARB, 2 );
*/
	if( FBitSet( flags, FPART_ADDITIVE )) pglColor4f( 1.0f, 1.0f, 1.0f, curAlpha );
	else pglColor4f( curColor.x, curColor.y, curColor.z, curAlpha );

	pglBegin( GL_QUADS );
		pglTexCoord2f( 0.0f, 0.0f );
		pglVertex3fv( verts[0] );

		pglTexCoord2f( 1.0f, 0.0f );
		pglVertex3fv( verts[1] );

		pglTexCoord2f( 1.0f, 1.0f );
		pglVertex3fv( verts[2] );

		pglTexCoord2f( 0.0f, 1.0f );
		pglVertex3fv( verts[3] );
	pglEnd();

	return true;
}

CQuakePartSystem :: CQuakePartSystem( void )
{
	memset( m_pParticles, 0, sizeof( CQuakePart ) * MAX_PARTICLES );

	m_pFreeParticles = m_pParticles;
	m_pActiveParticles = NULL;
}

CQuakePartSystem :: ~CQuakePartSystem( void )
{
}

void CQuakePartSystem :: Clear( void )
{
	m_pFreeParticles = m_pParticles;
	m_pActiveParticles = NULL;

	for( int i = 0; i < MAX_PARTICLES; i++ )
		m_pParticles[i].next = &m_pParticles[i+1];

	m_pParticles[MAX_PARTICLES-1].next = NULL;

	m_pAllowParticles = CVAR_REGISTER( "cl_particles", "1", FCVAR_ARCHIVE );
	m_pParticleLod = CVAR_REGISTER( "cl_particle_lod", "0", FCVAR_ARCHIVE );

	// loading TE shaders
	m_hDefaultParticle = FIND_TEXTURE( "*particle" );	// quake particle
	m_hSparks = LOAD_TEXTURE( "gfx/particles/spark.tga", NULL, 0, TF_NOPICMIP|TF_CLAMP );
	m_hSmoke = LOAD_TEXTURE( "gfx/particles/smoke.tga", NULL, 0, TF_NOPICMIP|TF_CLAMP );
	m_hWaterSplash = LOAD_TEXTURE( "gfx/particles/splash1.tga", NULL, 0, TF_NOPICMIP|TF_CLAMP );
}

void CQuakePartSystem :: FreeParticle( CQuakePart *pCur )
{
	pCur->next = m_pFreeParticles;
	m_pFreeParticles = pCur;
}

CQuakePart *CQuakePartSystem :: AllocParticle( void )
{
	CQuakePart	*p;

	if( !m_pFreeParticles )
	{
		ALERT( at_console, "Overflow %d particles\n", MAX_PARTICLES );
		return NULL;
	}

	if( m_pParticleLod->value > 1.0f )
	{
		if( !( RANDOM_LONG( 0, 1 ) % (int)m_pParticleLod->value ))
			return NULL;
	}

	p = m_pFreeParticles;
	m_pFreeParticles = p->next;
	p->next = m_pActiveParticles;
	m_pActiveParticles = p;

	return p;
}
	
void CQuakePartSystem :: Update( void )
{
	CQuakePart	*p, *next;
	CQuakePart	*active = NULL, *tail = NULL;

	if( !m_pAllowParticles->value )
		return;

	float gravity = RI.refdef.frametime * RI.refdef.movevars->gravity;

	for( p = m_pActiveParticles; p; p = next )
	{
		// grab next now, so if the particle is freed we still have it
		next = p->next;

		if( !p->Evaluate( gravity ))
		{
			FreeParticle( p );
			continue;
		}

		p->next = NULL;

		if( !tail )
		{
			active = tail = p;
		}
		else
		{
			tail->next = p;
			tail = p;
		}
	}

	m_pActiveParticles = active;
}

bool CQuakePartSystem :: AddParticle( CQuakePart *src, int texture, int flags )
{
	if( !src ) return false;

	CQuakePart *dst = AllocParticle();

	if( !dst ) return false;

	if( texture ) dst->m_hTexture = texture;
	else dst->m_hTexture = m_hDefaultParticle;
	dst->flTime = RI.refdef.time;
	dst->flags = flags;

	dst->origin = src->origin;
	dst->velocity = src->velocity;
	dst->accel = src->accel; 
	dst->color = src->color;
	dst->colorVelocity = src->colorVelocity;
	dst->alpha = src->alpha;
	dst->scale = 1.0f;

	dst->radius = src->radius;
	dst->length = src->length;
	dst->rotation = src->rotation;
	dst->alphaVelocity = src->alphaVelocity;
	dst->radiusVelocity = src->radiusVelocity;
	dst->lengthVelocity = src->lengthVelocity;
	dst->bounceFactor = src->bounceFactor;

	// needs to save old origin
	if( FBitSet( flags, ( FPART_BOUNCE|FPART_FRICTION )))
		dst->oldorigin = dst->origin;

	return true;
}

/*
=================
CL_ExplosionParticles
=================
*/
void CQuakePartSystem :: ExplosionParticles( const Vector &pos )
{
	CQuakePart src;
	int flags;

	if( !m_pAllowParticles->value )
		return;

	flags = (FPART_STRETCH|FPART_BOUNCE|FPART_FRICTION);

	for( int i = 0; i < 384; i++ )
	{
		src.origin.x = pos.x + RANDOM_LONG( -16, 16 );
		src.origin.y = pos.y + RANDOM_LONG( -16, 16 );
		src.origin.z = pos.z + RANDOM_LONG( -16, 16 );
		src.velocity.x = RANDOM_LONG( -256, 256 );
		src.velocity.y = RANDOM_LONG( -256, 256 );
		src.velocity.z = RANDOM_LONG( -256, 256 );
		src.accel.x = src.accel.y = 0;
		src.accel.z = -60 + RANDOM_FLOAT( -30, 30 );
		src.color = Vector( 1, 1, 1 );
		src.colorVelocity = Vector( 0, 0, 0 );
		src.alpha = 1.0;
		src.alphaVelocity = -3.0;
		src.radius = 0.5 + RANDOM_FLOAT( -0.2, 0.2 );
		src.radiusVelocity = 0;
		src.length = 8 + RANDOM_FLOAT( -4, 4 );
		src.lengthVelocity = 8 + RANDOM_FLOAT( -4, 4 );
		src.rotation = 0;
		src.bounceFactor = 0.2;

		if( !AddParticle( &src, m_hSparks, flags ))
			return;
	}

	// smoke
	flags = FPART_VERTEXLIGHT;

	for( i = 0; i < 5; i++ )
	{
		src.origin.x = pos.x + RANDOM_FLOAT( -10, 10 );
		src.origin.y = pos.y + RANDOM_FLOAT( -10, 10 );
		src.origin.z = pos.z + RANDOM_FLOAT( -10, 10 );
		src.velocity.x = RANDOM_FLOAT( -10, 10 );
		src.velocity.y = RANDOM_FLOAT( -10, 10 );
		src.velocity.z = RANDOM_FLOAT( -10, 10 ) + RANDOM_FLOAT( -5, 5 ) + 25;
		src.accel = Vector( 0, 0, 0 );
		src.color = Vector( 0, 0, 0 );
		src.colorVelocity = Vector( 0.75, 0.75, 0.75 );
		src.alpha = 0.5;
		src.alphaVelocity = RANDOM_FLOAT( -0.1, -0.2 );
		src.radius = 30 + RANDOM_FLOAT( -15, 15 );
		src.radiusVelocity = 15 + RANDOM_FLOAT( -7.5, 7.5 );
		src.length = 1;
		src.lengthVelocity = 0;
		src.rotation = RANDOM_LONG( 0, 360 );

		if( !AddParticle( &src, m_hSmoke, flags ))
			return;
	}
}

/*
=================
CL_BulletParticles
=================
*/
void CQuakePartSystem :: SparkParticles( const Vector &org, const Vector &dir )
{
	CQuakePart src;

	if( !m_pAllowParticles->value )
		return;

	// sparks
	int flags = (FPART_STRETCH|FPART_BOUNCE|FPART_FRICTION);

	for( int i = 0; i < 16; i++ )
	{
		src.origin.x = org[0] + dir[0] * 2 + RANDOM_FLOAT( -1, 1 );
		src.origin.y = org[1] + dir[1] * 2 + RANDOM_FLOAT( -1, 1 );
		src.origin.z = org[2] + dir[2] * 2 + RANDOM_FLOAT( -1, 1 );
		src.velocity.x = dir[0] * 180 + RANDOM_FLOAT( -60, 60 );
		src.velocity.y = dir[1] * 180 + RANDOM_FLOAT( -60, 60 );
		src.velocity.z = dir[2] * 180 + RANDOM_FLOAT( -60, 60 );
		src.accel.x = src.accel.y = 0;
		src.accel.z = -120 + RANDOM_FLOAT( -60, 60 );
		src.color = Vector( 1.0, 1.0f, 1.0f );
		src.colorVelocity = Vector( 0, 0, 0 );
		src.alpha = 1.0;
		src.alphaVelocity = -8.0;
		src.radius = 0.4 + RANDOM_FLOAT( -0.2, 0.2 );
		src.radiusVelocity = 0;
		src.length = 8 + RANDOM_FLOAT( -4, 4 );
		src.lengthVelocity = 8 + RANDOM_FLOAT( -4, 4 );
		src.rotation = 0;
		src.bounceFactor = 0.2;

		if( !AddParticle( &src, m_hSparks, flags ))
			return;
	}
}

/*
=================
CL_RicochetSparks
=================
*/
void CQuakePartSystem :: RicochetSparks( const Vector &org, float scale )
{
	CQuakePart src;

	if( !m_pAllowParticles->value )
		return;

	// sparks
	int flags = (FPART_STRETCH|FPART_BOUNCE|FPART_FRICTION);

	for( int i = 0; i < 16; i++ )
	{
		src.origin.x = org[0] + RANDOM_FLOAT( -1, 1 );
		src.origin.y = org[1] + RANDOM_FLOAT( -1, 1 );
		src.origin.z = org[2] + RANDOM_FLOAT( -1, 1 );
		src.velocity.x = RANDOM_FLOAT( -60, 60 );
		src.velocity.y = RANDOM_FLOAT( -60, 60 );
		src.velocity.z = RANDOM_FLOAT( -60, 60 );
		src.accel.x = src.accel.y = 0;
		src.accel.z = -120 + RANDOM_FLOAT( -60, 60 );
		src.color = Vector( 1.0, 1.0f, 1.0f );
		src.colorVelocity = Vector( 0, 0, 0 );
		src.alpha = 1.0;
		src.alphaVelocity = -8.0;
		src.radius = scale + RANDOM_FLOAT( -0.2, 0.2 );
		src.radiusVelocity = 0;
		src.length = scale + RANDOM_FLOAT( -0.2, 0.2 );
		src.lengthVelocity = scale + RANDOM_FLOAT( -0.2, 0.2 );
		src.rotation = 0;
		src.bounceFactor = 0.2;

		if( !AddParticle( &src, m_hSparks, flags ))
			return;
	}
}

void CQuakePartSystem :: SmokeParticles( const Vector &pos, int count )
{
	CQuakePart src;

	if( !m_pAllowParticles->value )
		return;

	// smoke
	int flags = FPART_VERTEXLIGHT;

	for( int i = 0; i < count; i++ )
	{
		src.origin.x = pos.x + RANDOM_FLOAT( -10, 10 );
		src.origin.y = pos.y + RANDOM_FLOAT( -10, 10 );
		src.origin.z = pos.z + RANDOM_FLOAT( -10, 10 );
		src.velocity.x = RANDOM_FLOAT( -10, 10 );
		src.velocity.y = RANDOM_FLOAT( -10, 10 );
		src.velocity.z = RANDOM_FLOAT( -10, 10 ) + RANDOM_FLOAT( -5, 5 ) + 25;
		src.accel = Vector( 0, 0, 0 );
		src.color = Vector( 0, 0, 0 );
		src.colorVelocity = Vector( 0.75, 0.75, 0.75 );
		src.alpha = 0.5;
		src.alphaVelocity = RANDOM_FLOAT( -0.1, -0.2 );
		src.radius = 30 + RANDOM_FLOAT( -15, 15 );
		src.radiusVelocity = 15 + RANDOM_FLOAT( -7.5, 7.5 );
		src.length = 1;
		src.lengthVelocity = 0;
		src.rotation = RANDOM_LONG( 0, 360 );

		if( !AddParticle( &src, m_hSmoke, flags ))
			return;
	}
}

/*
=================
CL_BulletParticles
=================
*/
void CQuakePartSystem :: BulletParticles( const Vector &org, const Vector &dir )
{
	CQuakePart src;
	int cnt, count;

	if( !m_pAllowParticles->value )
		return;

	count = RANDOM_LONG( 3, 8 );
	cnt = POINT_CONTENTS( (float *)&org );

	if( cnt == CONTENTS_WATER )
		return;

	// sparks
	int flags = (FPART_STRETCH|FPART_BOUNCE|FPART_FRICTION|FPART_ADDITIVE);

	for( int i = 0; i < count; i++ )
	{
		src.origin.x = org[0] + dir[0] * 2 + RANDOM_FLOAT( -1, 1 );
		src.origin.y = org[1] + dir[1] * 2 + RANDOM_FLOAT( -1, 1 );
		src.origin.z = org[2] + dir[2] * 2 + RANDOM_FLOAT( -1, 1 );
		src.velocity.x = dir[0] * 180 + RANDOM_FLOAT( -60, 60 );
		src.velocity.y = dir[1] * 180 + RANDOM_FLOAT( -60, 60 );
		src.velocity.z = dir[2] * 180 + RANDOM_FLOAT( -60, 60 );
		src.accel.x = src.accel.y = 0;
		src.accel.z = -120 + RANDOM_FLOAT( -60, 60 );
		src.color = Vector( 1.0, 1.0f, 1.0f );
		src.colorVelocity = Vector( 0, 0, 0 );
		src.alpha = 1.0;
		src.alphaVelocity = -8.0;
		src.radius = 0.4 + RANDOM_FLOAT( -0.2, 0.2 );
		src.radiusVelocity = 0;
		src.length = 8 + RANDOM_FLOAT( -4, 4 );
		src.lengthVelocity = 8 + RANDOM_FLOAT( -4, 4 );
		src.rotation = 0;
		src.bounceFactor = 0.2;

		if( !AddParticle( &src, m_hSparks, flags ))
			return;
	}

	// smoke
	flags = FPART_VERTEXLIGHT;

	for( i = 0; i < 3; i++ )
	{
		src.origin.x = org[0] + dir[0] * 5 + RANDOM_FLOAT( -1, 1 );
		src.origin.y = org[1] + dir[1] * 5 + RANDOM_FLOAT( -1, 1 );
		src.origin.z = org[2] + dir[2] * 5 + RANDOM_FLOAT( -1, 1 );
		src.velocity.x = RANDOM_FLOAT( -2.5, 2.5 );
		src.velocity.y = RANDOM_FLOAT( -2.5, 2.5 );
		src.velocity.z = RANDOM_FLOAT( -2.5, 2.5 ) + (25 + RANDOM_FLOAT( -5, 5 ));
		src.accel = Vector( 0, 0, 0 );
		src.color = Vector( 0.4, 0.4, 0.4 );
		src.colorVelocity = Vector( 0.2, 0.2, 0.2 );
		src.alpha = 0.5;
		src.alphaVelocity = -(0.4 + RANDOM_FLOAT( 0, 0.2 ));
		src.radius = 3 + RANDOM_FLOAT( -1.5, 1.5 );
		src.radiusVelocity = 5 + RANDOM_FLOAT( -2.5, 2.5 );
		src.length = 1;
		src.lengthVelocity = 0;
		src.rotation = RANDOM_LONG( 0, 360 );

		if( !AddParticle( &src, m_hSmoke, flags ))
			return;
	}
}