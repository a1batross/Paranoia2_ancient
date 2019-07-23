/*
gl_local.h - renderer local definitions
this code written for Paranoia 2: Savior modification
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

#ifndef GL_RPART_H
#define GL_RPART_H

#define MAX_PARTICLES		8192

// built-in particle-system flags
#define FPART_BOUNCE		(1<<0)	// makes a bouncy particle
#define FPART_FRICTION		(1<<1)
#define FPART_VERTEXLIGHT		(1<<2)	// give some ambient light for it
#define FPART_STRETCH		(1<<3)
#define FPART_UNDERWATER		(1<<4)
#define FPART_INSTANT		(1<<5)
#define FPART_ADDITIVE		(1<<6)

class CQuakePart
{
public:
	Vector		origin;		// position for current frame
	Vector		oldorigin;	// position from previous frame

	Vector		velocity;		// linear velocity
	Vector		accel;
	Vector		color;
	Vector		colorVelocity;
	float		alpha;
	float		alphaVelocity;
	float		radius;
	float		radiusVelocity;
	float		length;
	float		lengthVelocity;
	float		rotation;		// texture ROLL angle
	float		bounceFactor;
	float		scale;

	CQuakePart	*next;		// linked list
	int		m_hTexture;

	float		flTime;
	int		flags;

	bool		Evaluate( float gravity );
};

class CQuakePartSystem
{
	CQuakePart	*m_pActiveParticles;
	CQuakePart	*m_pFreeParticles;
	CQuakePart	m_pParticles[MAX_PARTICLES];

	// private partsystem shaders
	int		m_hDefaultParticle;
	int		m_hSparks;
	int		m_hSmoke;
	int		m_hWaterSplash;

	cvar_t		*m_pAllowParticles;
	cvar_t		*m_pParticleLod;
public:
			CQuakePartSystem( void );
	virtual		~CQuakePartSystem( void );

	void		Clear( void );
	void		Update( void );
	void		FreeParticle( CQuakePart *pCur );
	CQuakePart	*AllocParticle( void );
	bool		AddParticle( CQuakePart *src, int texture = 0, int flags = 0 );

	// example presets
	void		ExplosionParticles( const Vector &pos );
	void		BulletParticles( const Vector &org, const Vector &dir );
	void		BubbleParticles( const Vector &org, int count, float magnitude );
	void		SparkParticles( const Vector &org, const Vector &dir );
	void		RicochetSparks( const Vector &org, float scale );
	void		SmokeParticles( const Vector &pos, int count );
};

extern CQuakePartSystem	g_pParticles;

#endif//GL_RPART_H