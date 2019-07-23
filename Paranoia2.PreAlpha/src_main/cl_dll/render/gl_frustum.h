/*
gl_frustum.h - frustum test implementation class
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

#ifndef GL_FRUSTUM_H
#define GL_FRUSTUM_H

// don't change this order
#define FRUSTUM_LEFT	0
#define FRUSTUM_RIGHT	1
#define FRUSTUM_BOTTOM	2
#define FRUSTUM_TOP		3
#define FRUSTUM_FAR		4
#define FRUSTUM_NEAR	5
#define FRUSTUM_PLANES	6

class CFrustum
{
public:
	void InitProjection( const matrix3x4 &view, float flZNear, float flZFar, float flFovX, float flFovY );
	void InitOrthogonal( const matrix3x4 &view, float xLeft, float xRight, float yBottom, float yTop, float flZNear, float flZFar );
	void InitBoxFrustum( const Vector &org, float radius ); // used for pointlights
	void SetPlane( int side, const Vector &vecNormal, float flDist );
	const mplane_t &GetPlane( int side ) { return planes[side]; }
	unsigned int GetClipFlags( void ) { return clipFlags; }
	void ComputeFrustumCorners( Vector bbox[8] );
	void DrawFrustumDebug( void );
	void ClearFrustum( void );

	// cull methods
	bool CullBox( const Vector &mins, const Vector &maxs, unsigned int clipflags = 0 );
	bool CullSphere( const Vector &centre, float radius, unsigned int clipflags = 0 );
	bool CullSurface( const msurface_t *surf, unsigned int clipflags = 0 );

	// plane manipulating
	void EnablePlane( int side );
	void DisablePlane( int side );
private:
	mplane_t		planes[6];
	unsigned int 	clipFlags;
};

#endif//GL_FRUSTUM_H