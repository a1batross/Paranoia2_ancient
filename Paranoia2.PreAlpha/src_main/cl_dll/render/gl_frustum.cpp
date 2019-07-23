/*
gl_frustum.cpp - frustum test implementation class
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
#include "ref_params.h"
#include "gl_local.h"
#include <mathlib.h>
#include <stringlib.h>

void CFrustum :: ClearFrustum( void )
{
	memset( planes, 0, sizeof( planes ));
	clipFlags = 0;
}

void CFrustum :: EnablePlane( int side )
{
	ASSERT( side >= 0 && side < FRUSTUM_PLANES );

	// make sure what plane is ready
	if( planes[side].normal != g_vecZero )
		SetBits( clipFlags, BIT( side ));
}

void CFrustum :: DisablePlane( int side )
{
	ASSERT( side >= 0 && side < FRUSTUM_PLANES );
	ClearBits( clipFlags, BIT( side ));
}

void CFrustum :: InitProjection( const matrix3x4 &view, float flZNear, float flZFar, float flFovX, float flFovY )
{
	float	xs, xc;
	Vector	normal;	

	ClearFrustum(); // first, clear the previous frustum

	// horizontal fov used for left and view.GetRight() planes
	SinCos( DEG2RAD( flFovX ) * 0.5f, &xs, &xc );

	// setup left plane
	normal = view.GetForward() * xs + view.GetRight() * -xc;
	SetPlane( FRUSTUM_LEFT, normal, DotProduct( view.GetOrigin(), normal ));

	// setup right plane
	normal = view.GetForward() * xs + view.GetRight() * xc;
	SetPlane( FRUSTUM_RIGHT, normal, DotProduct( view.GetOrigin(), normal ));

	// vertical fov used for top and bottom planes
	SinCos( DEG2RAD( flFovY ) * 0.5f, &xs, &xc );

	// setup bottom plane
	normal = view.GetForward() * xs + view.GetUp() * -xc;
	SetPlane( FRUSTUM_BOTTOM, normal, DotProduct( view.GetOrigin(), normal ));

	// setup top plane
	normal = view.GetForward() * xs + view.GetUp() * xc;
	SetPlane( FRUSTUM_TOP, normal, DotProduct( view.GetOrigin(), normal ));

	// setup far plane
	SetPlane( FRUSTUM_FAR, -view.GetForward(), DotProduct( -view.GetForward(), ( view.GetOrigin() + view.GetForward() * flZFar )));

	// no need to setup backplane for general view. It's only used for portals and mirrors
	if( flZNear == 0.0f ) return;

	// setup near plane
	SetPlane( FRUSTUM_NEAR, view.GetForward(), DotProduct( view.GetForward(), ( view.GetOrigin() + view.GetForward() * flZNear )));
}

void CFrustum :: InitOrthogonal( const matrix3x4 &view, float xLeft, float xRight, float yBottom, float yTop, float flZNear, float flZFar )
{
	ClearFrustum(); // first, clear the previous frustum

	// setup the near and far planes
	float orgOffset = DotProduct( view.GetOrigin(), view.GetForward() );

	SetPlane( FRUSTUM_FAR, -view.GetForward(), -flZFar - orgOffset );
	SetPlane( FRUSTUM_NEAR, view.GetForward(), flZNear + orgOffset );

	// setup left and right planes
	orgOffset = DotProduct( view.GetOrigin(), view.GetRight() );

	SetPlane( FRUSTUM_LEFT, -view.GetRight(), -xLeft - orgOffset );
	SetPlane( FRUSTUM_RIGHT, view.GetRight(), xRight + orgOffset );

	// setup top and buttom planes
	orgOffset = DotProduct( view.GetOrigin(), view.GetUp() );

	SetPlane( FRUSTUM_TOP, view.GetUp(), yTop + orgOffset );
	SetPlane( FRUSTUM_BOTTOM, -view.GetUp(), -yBottom - orgOffset );
}

void CFrustum :: InitBoxFrustum( const Vector &org, float radius )
{
	ClearFrustum(); // first, clear the previous frustum

	for( int i = 0; i < 6; i++ )
	{
		// setup normal for each direction
		Vector normal = g_vecZero;
		normal[((i >> 1) + 1) % 3] = (i & 1) ? 1.0f : -1.0f;
		SetPlane( i, normal, DotProduct( org, normal ) - radius );
	}
}

void CFrustum :: SetPlane( int side, const Vector &vecNormal, float flDist )
{
	ASSERT( side >= 0 && side < FRUSTUM_PLANES );

	planes[side].type = PlaneTypeForNormal( vecNormal );
	planes[side].signbits = SignbitsForPlane( vecNormal );
	planes[side].normal = vecNormal;
	planes[side].dist = flDist;

	clipFlags |= BIT( side );
}

void CFrustum :: ComputeFrustumCorners( Vector corners[8] )
{
	memset( corners, 0, sizeof( Vector ) * 8 );

	PlanesGetIntersectionPoint( &planes[FRUSTUM_LEFT], &planes[FRUSTUM_TOP], &planes[FRUSTUM_FAR], corners[0] );
	PlanesGetIntersectionPoint( &planes[FRUSTUM_RIGHT], &planes[FRUSTUM_TOP], &planes[FRUSTUM_FAR], corners[1] );
	PlanesGetIntersectionPoint( &planes[FRUSTUM_LEFT], &planes[FRUSTUM_BOTTOM], &planes[FRUSTUM_FAR], corners[2] );
	PlanesGetIntersectionPoint( &planes[FRUSTUM_RIGHT], &planes[FRUSTUM_BOTTOM], &planes[FRUSTUM_FAR], corners[3] );

	if( FBitSet( clipFlags, BIT( FRUSTUM_NEAR )))
	{
		PlanesGetIntersectionPoint( &planes[FRUSTUM_LEFT], &planes[FRUSTUM_TOP], &planes[FRUSTUM_NEAR], corners[4] );
		PlanesGetIntersectionPoint( &planes[FRUSTUM_RIGHT], &planes[FRUSTUM_TOP], &planes[FRUSTUM_NEAR], corners[5] );
		PlanesGetIntersectionPoint( &planes[FRUSTUM_LEFT], &planes[FRUSTUM_BOTTOM], &planes[FRUSTUM_NEAR], corners[6] );
		PlanesGetIntersectionPoint( &planes[FRUSTUM_RIGHT], &planes[FRUSTUM_BOTTOM], &planes[FRUSTUM_NEAR], corners[7] );
	}
	else
	{
		PlanesGetIntersectionPoint( &planes[FRUSTUM_LEFT], &planes[FRUSTUM_RIGHT], &planes[FRUSTUM_TOP], corners[4] );
		corners[7] = corners[6] = corners[5] = corners[4];
	}
}

void CFrustum :: DrawFrustumDebug( void )
{
	Vector	bbox[8];

	ComputeFrustumCorners( bbox );

	// g-cont. frustum must be yellow :-)
	pglColor4f( 1.0f, 1.0f, 0.0f, 1.0f );
	pglDisable( GL_TEXTURE_2D );
	pglBegin( GL_LINES );

	for( int i = 0; i < 2; i += 1 )
	{
		pglVertex3fv( bbox[i+0] );
		pglVertex3fv( bbox[i+2] );
		pglVertex3fv( bbox[i+4] );
		pglVertex3fv( bbox[i+6] );
		pglVertex3fv( bbox[i+0] );
		pglVertex3fv( bbox[i+4] );
		pglVertex3fv( bbox[i+2] );
		pglVertex3fv( bbox[i+6] );
		pglVertex3fv( bbox[i*2+0] );
		pglVertex3fv( bbox[i*2+1] );
		pglVertex3fv( bbox[i*2+4] );
		pglVertex3fv( bbox[i*2+5] );
	}

	pglEnd();
	pglEnable( GL_TEXTURE_2D );
}

bool CFrustum :: CullBox( const Vector &mins, const Vector &maxs, unsigned int clipflags )
{
	return false;
}

bool CFrustum :: CullSphere( const Vector &centre, float radius, unsigned int clipflags )
{
	return false;
}

bool CFrustum :: CullSurface( const msurface_t *surf, unsigned int clipflags )
{
	return false;
}