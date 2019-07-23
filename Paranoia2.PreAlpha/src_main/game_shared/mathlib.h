//=======================================================================
//			Copyright (C) Shambler Team 2005
//		          mathlib.h - shared mathlib header
//=======================================================================
#ifndef MATHLIB_H
#define MATHLIB_H

#ifndef M_PI
#define M_PI		3.14159265358979323846	// matches value in gcc v2 math.h
#endif

#include <math.h>
#include <vector.h>
#include "matrix.h"

// NOTE: PhysX mathlib is conflicted with standard min\max
#define Q_min( a, b )		(((a) < (b)) ? (a) : (b))
#define Q_max( a, b )		(((a) > (b)) ? (a) : (b))

#define bound( min, num, max )	((num) >= (min) ? ((num) < (max) ? (num) : (max)) : (min))
#define IS_NAN(x)			(((*(int *)&x) & (255<<23)) == (255<<23))

// some Physic Engine declarations
#define METERS_PER_INCH		0.0254f
#define METER2INCH( x )		(float)( x * ( 1.0f / METERS_PER_INCH ))
#define INCH2METER( x )		(float)( x * ( METERS_PER_INCH / 1.0f ))

// Keeps clutter down a bit, when using a float as a bit-vector
#define SetBits( iBitVector, bits )	((iBitVector) = (iBitVector) | (bits))
#define ClearBits( iBitVector, bits )	((iBitVector) = (iBitVector) & ~(bits))
#define FBitSet( iBitVector, bit )	((iBitVector) & (bit))

// Used to represent sides of things like planes.
#define SIDE_FRONT			0
#define SIDE_BACK			1
#define SIDE_ON			2

#define PLANE_X			0	// 0 - 2 are axial planes
#define PLANE_Y			1	// 3 needs alternate calc
#define PLANE_Z			2
#define PLANE_NONAXIAL		3

#define PLANE_NORMAL_EPSILON		0.00001f
#define PLANE_DIST_EPSILON		0.01f

#define SMALL_FLOAT			1e-12

inline float anglemod( float a ) { return (360.0f / 65536) * ((int)(a * (65536 / 360.0f)) & 65535); }
void TransformAABB( const matrix4x4& world, const Vector &mins, const Vector &maxs, Vector &absmin, Vector &absmax );
void TransformAABBLocal( const matrix4x4& world, const Vector &mins, const Vector &maxs, Vector &outmins, Vector &outmaxs );
void CalcClosestPointOnAABB( const Vector &mins, const Vector &maxs, const Vector &point, Vector &closestOut );
void RotatePointAroundVector( Vector &dst, const Vector &dir, const Vector &point, float degrees );
bool PlanesGetIntersectionPoint( const struct mplane_s *plane1, const struct mplane_s *plane2, const struct mplane_s *plane3, Vector &out );
void VectorAngles( const Vector &forward, Vector &angles );
void VectorAngles2( const Vector &forward, Vector &angles );
float RemapVal( float val, float A, float B, float C, float D );
int PlaneTypeForNormal( const Vector &normal );
int SignbitsForPlane( const Vector &normal );
int NearestPOW( int value, bool roundDown );

//
// bounds operations
//
void ClearBounds( Vector &mins, Vector &maxs );
void ClearBounds( Vector2D &mins, Vector2D &maxs );
void AddPointToBounds( const Vector &v, Vector &mins, Vector &maxs );
void AddPointToBounds( const Vector2D &v, Vector2D &mins, Vector2D &maxs );
bool BoundsIntersect( const Vector &mins1, const Vector &maxs1, const Vector &mins2, const Vector &maxs2 );
bool BoundsAndSphereIntersect( const Vector &mins, const Vector &maxs, const Vector &origin, float radius );
float RadiusFromBounds( const Vector &mins, const Vector &maxs );
void PerpendicularVector( Vector &dst, const Vector &src );

//
// quaternion operations
//
void AngleQuaternion( const Vector &angles, Vector4D &quat, bool degrees = false );
void QuaternionAngle( const Vector4D &quat, Vector &angles, bool degrees = false );
void QuaternionAlign( const Vector4D &p, const Vector4D &q, Vector4D &qt );
void QuaternionSlerp( const Vector4D &p, const Vector4D &q, float t, Vector4D &qt );
void QuaternionSlerpNoAlign( const Vector4D &p, const Vector4D &q, float t, Vector4D &qt );

//
// lerping stuff
//
void InterpolateOrigin( const Vector& start, const Vector& end, Vector& output, float frac, bool back = false );
void InterpolateAngles( const Vector& start, const Vector& end, Vector& output, float frac, bool back = false );
void NormalizeAngles( Vector &angles );

struct mplane_s;

void VectorMatrix( Vector &forward, Vector &right, Vector &up );
float ColorNormalize( const Vector &in, Vector &out );

bool PlaneFromPoints( const Vector triangle[3], struct mplane_s *plane );
bool ComparePlanes( struct mplane_s *plane, const Vector &normal, float dist );
void CategorizePlane( struct mplane_s *plane );
void SnapPlaneToGrid( struct mplane_s *plane );
void SnapVectorToGrid( Vector &normal );

void CalcTBN( const Vector &p0, const Vector &p1, const Vector &p2, const Vector2D &t0, const Vector2D &t1, const Vector2D& t2, Vector &s, Vector &t );

int BoxOnPlaneSide( const Vector &emins, const Vector &emaxs, const struct mplane_s *plane );
#define BOX_ON_PLANE_SIDE(emins, emaxs, p) (((p)->type < 3)?(((p)->dist <= (emins)[(p)->type])? 1 : (((p)->dist >= (emaxs)[(p)->type])? 2 : 3)):BoxOnPlaneSide( (emins), (emaxs), (p)))

#define PlaneDist(point,plane) ((plane)->type < 3 ? (point)[(plane)->type] : DotProduct((point), (plane)->normal))
#define PlaneDiff(point,plane) (((plane)->type < 3 ? (point)[(plane)->type] : DotProduct((point), (plane)->normal)) - (plane)->dist)
#define VectorNormalizeFast( v ) { float ilength = (float)rsqrt( DotProduct( v,v )); v[0] *= ilength; v[1] *= ilength; v[2] *= ilength; }

// FIXME: get rid of this
#define VectorCopy(a,b)	((b)[0]=(a)[0],(b)[1]=(a)[1],(b)[2]=(a)[2])
#define round( x, y )	(floor(( x + 0.5) * y ) / y )

/*
=================
rsqrt
=================
*/
inline float rsqrt( float number )
{
	if( number == 0.0f )
		return 0.0f;

	int x = number * 0.5f;
	int i = *(int *)&number;	// evil floating point bit level hacking
	i = 0x5f3759df - (i >> 1);	// what the fuck?
	int y = *(float *)&i;
	y = y * (1.5f - (x * y * y));	// first iteration

	return y;
}

#endif//MATHLIB_H