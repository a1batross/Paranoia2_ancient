//=======================================================================
//			Copyright (C) Shambler Team 2005
//		          mathlib.cpp - shared math library
//=======================================================================

#include "mathlib.h"
#include "const.h"
#include "com_model.h"
#include <math.h>

const Vector g_vecZero( 0, 0, 0 );

/*
=================
SignbitsForPlane

fast box on planeside test
=================
*/
int SignbitsForPlane( const Vector &normal )
{
	for( int bits = 0, i = 0; i < 3; i++ )
		if( normal[i] < 0.0f )
			bits |= 1<<i;
	return bits;
}

/*
=================
PlaneTypeForNormal
=================
*/
int PlaneTypeForNormal( const Vector &normal )
{
	if( normal.x == 1.0f )
		return PLANE_X;
	if( normal.y == 1.0 )
		return PLANE_Y;
	if( normal.z == 1.0 )
		return PLANE_Z;
	return PLANE_NONAXIAL;
}

/*
=================
PlanesGetIntersectionPoint

=================
*/
bool PlanesGetIntersectionPoint( const mplane_t *plane1, const mplane_t *plane2, const mplane_t *plane3, Vector &out )
{
	Vector n1 = plane1->normal.Normalize();
	Vector n2 = plane2->normal.Normalize();
	Vector n3 = plane3->normal.Normalize();

	Vector n1n2 = CrossProduct( n1, n2 );
	Vector n2n3 = CrossProduct( n2, n3 );
	Vector n3n1 = CrossProduct( n3, n1 );

	float denom = DotProduct( n1, n2n3 );
	out = g_vecZero;

	// check if the denominator is zero (which would mean that no intersection is to be found
	if( denom == 0.0f )
	{
		// no intersection could be found, return <0,0,0>
		return false;
	}

	// compute intersection point
	out += n2n3 * plane1->dist;
	out += n3n1 * plane2->dist;
	out += n1n2 * plane3->dist;
	out *= (1.0f / denom );

	return true;
}

void PerpendicularVector( Vector &dst, const Vector &src )
{
	// LordHavoc: optimized to death and beyond
	int	pos;
	float	minelem;

	if( src.x )
	{
		dst.x = 0;
		if( src.y )
		{
			dst.y = 0;
			if( src.z )
			{
				dst.z = 0;
				pos = 0;
				minelem = fabs( src.x );
				if( fabs( src.y ) < minelem )
				{
					pos = 1;
					minelem = fabs( src.y );
				}
				if( fabs( src.y ) < minelem )
					pos = 2;

				dst[pos] = 1;
				dst.x -= src[pos] * src.x;
				dst.y -= src[pos] * src.y;
				dst.z -= src[pos] * src.z;

				// normalize the result
				dst = dst.Normalize();
			}
			else dst.z = 1;
		}
		else
		{
			dst.y = 1;
			dst.z = 0;
		}
	}
	else
	{
		dst.x = 1;
		dst.y = 0;
		dst.z = 0;
	}
}

/*
=================
VectorAngles

=================
*/
void VectorAngles( const Vector &forward, Vector &angles )
{
	angles[ROLL] = 0.0f;

	if( forward.x || forward.y )
	{
		float tmp;

		angles[YAW] = RAD2DEG( atan2( forward.y, forward.x ));
		if( angles[YAW] < 0 ) angles[YAW] += 360;

		tmp = sqrt( forward.x * forward.x + forward.y * forward.y );
		angles[PITCH] = RAD2DEG( atan2( forward.z, tmp ));
		if( angles[PITCH] < 0 ) angles[PITCH] += 360;
	}
	else
	{
		// fast case
		angles[YAW] = 0.0f;
		if( forward.z > 0 )
			angles[PITCH] = 90.0f;
		else angles[PITCH] = 270.0f;
	}
}

/*
=================
VectorAngles2

this version without stupid quake bug
=================
*/
void VectorAngles2( const Vector &forward, Vector &angles )
{
	angles[ROLL] = 0.0f;

	if( forward.x || forward.y )
	{
		float tmp;

		angles[YAW] = RAD2DEG( atan2( forward.y, forward.x ));
		if( angles[YAW] < 0.0f ) angles[YAW] += 360.0f;

		tmp = sqrt( forward.x * forward.x + forward.y * forward.y );
		angles[PITCH] = RAD2DEG( atan2( -forward.z, tmp ));
		if( angles[PITCH] < 0.0f ) angles[PITCH] += 360.0f;
	}
	else
	{
		// fast case
		angles[YAW] = 0.0f;
		if( forward.z > 0.0f )
			angles[PITCH] = 270.0f;
		else angles[PITCH] = 90.0f;
	}
}

/*
=================
NearestPOW
=================
*/
int NearestPOW( int value, bool roundDown )
{
	int	n = 1;

	if( value <= 0 ) return 1;
	while( n < value ) n <<= 1;

	if( roundDown )
	{
		if( n > value ) n >>= 1;
	}
	return n;
}

// remap a value in the range [A,B] to [C,D].
float RemapVal( float val, float A, float B, float C, float D )
{
	return C + (D - C) * (val - A) / (B - A);
}

//-----------------------------------------------------------------------------
// Transforms a AABB into another space; which will inherently grow the box.
//-----------------------------------------------------------------------------
void TransformAABB( const matrix4x4& world, const Vector &mins, const Vector &maxs, Vector &absmin, Vector &absmax )
{
	Vector localCenter = (mins + maxs) * 0.5f;
	Vector localExtents = maxs - localCenter;
	Vector worldCenter = world.VectorTransform( localCenter );
	matrix4x4 iworld = world.Transpose();
	Vector worldExtents;

	worldExtents.x = DotProductAbs( localExtents, iworld.GetForward( ));
	worldExtents.y = DotProductAbs( localExtents, iworld.GetRight( ));
	worldExtents.z = DotProductAbs( localExtents, iworld.GetUp( ));

	absmin = worldCenter - worldExtents;
	absmax = worldCenter + worldExtents;
}

/*
==================
TransformAABBLocal
==================
*/
void TransformAABBLocal( const matrix4x4& world, const Vector &mins, const Vector &maxs, Vector &outmins, Vector &outmaxs )
{
	matrix4x4	im = world.Invert();
	ClearBounds( outmins, outmaxs );
	Vector p1, p2;
	int i;

	// compute a full bounding box
	for( i = 0; i < 8; i++ )
	{
		p1.x = ( i & 1 ) ? mins.x : maxs.x;
		p1.y = ( i & 2 ) ? mins.y : maxs.y;
		p1.z = ( i & 4 ) ? mins.z : maxs.z;

		p2.x = DotProduct( p1, im[0] );
		p2.y = DotProduct( p1, im[1] );
		p2.z = DotProduct( p1, im[2] );

		if( p2.x < outmins.x ) outmins.x = p2.x;
		if( p2.x > outmaxs.x ) outmaxs.x = p2.x;
		if( p2.y < outmins.y ) outmins.y = p2.y;
		if( p2.y > outmaxs.y ) outmaxs.y = p2.y;
		if( p2.z < outmins.z ) outmins.z = p2.z;
		if( p2.z > outmaxs.z ) outmaxs.z = p2.z;
	}

	// sanity check
	for( i = 0; i < 3; i++ )
	{
		if( outmins[i] > outmaxs[i] )
		{
			outmins = g_vecZero;
			outmaxs = g_vecZero;
			return;
		}
	}
}

void CalcClosestPointOnAABB( const Vector &mins, const Vector &maxs, const Vector &point, Vector &closestOut )
{
	closestOut.x = bound( mins.x, point.x, maxs.x );
	closestOut.y = bound( mins.y, point.y, maxs.y );
	closestOut.z = bound( mins.z, point.z, maxs.z );
}

//
// bounds operations
//
/*
=================
ClearBounds
=================
*/
void ClearBounds( Vector &mins, Vector &maxs )
{
	// make bogus range
	mins.x = mins.y = mins.z =  999999;
	maxs.x = maxs.y = maxs.z = -999999;
}

/*
=================
ClearBounds
=================
*/
void ClearBounds( Vector2D &mins, Vector2D &maxs )
{
	// make bogus range
	mins.x = mins.y =  999999;
	maxs.x = maxs.y = -999999;
}

/*
=================
AddPointToBounds
=================
*/
void AddPointToBounds( const Vector &v, Vector &mins, Vector &maxs )
{
	for( int i = 0; i < 3; i++ )
	{
		if( v[i] < mins[i] ) mins[i] = v[i];
		if( v[i] > maxs[i] ) maxs[i] = v[i];
	}
}

/*
=================
AddPointToBounds
=================
*/
void AddPointToBounds( const Vector2D &v, Vector2D &mins, Vector2D &maxs )
{
	for( int i = 0; i < 2; i++ )
	{
		if( v[i] < mins[i] ) mins[i] = v[i];
		if( v[i] > maxs[i] ) maxs[i] = v[i];
	}
}

/*
=================
BoundsIntersect
=================
*/
bool BoundsIntersect( const Vector &mins1, const Vector &maxs1, const Vector &mins2, const Vector &maxs2 )
{
	if( mins1.x > maxs2.x || mins1.y > maxs2.y || mins1.z > maxs2.z )
		return false;
	if( maxs1.x < mins2.x || maxs1.y < mins2.y || maxs1.z < mins2.z )
		return false;
	return true;
}

/*
=================
BoundsAndSphereIntersect
=================
*/
bool BoundsAndSphereIntersect( const Vector &mins, const Vector &maxs, const Vector &origin, float radius )
{
	if( mins.x > origin.x + radius || mins.y > origin.y + radius || mins.z > origin.z + radius )
		return false;
	if( maxs.x < origin.x - radius || maxs.y < origin.y - radius || maxs.z < origin.z - radius )
		return false;
	return true;
}

/*
=================
RadiusFromBounds
=================
*/
float RadiusFromBounds( const Vector &mins, const Vector &maxs )
{
	Vector	corner;

	for( int i = 0; i < 3; i++ )
	{
		corner[i] = fabs( mins[i] ) > fabs( maxs[i] ) ? fabs( mins[i] ) : fabs( maxs[i] );
	}
	return corner.Length();
}

//
// quaternion operations
//

/*
====================
AngleQuaternion
====================
*/
void AngleQuaternion( const Vector &angles, Vector4D &quat, bool degrees )
{
	float sr, sp, sy, cr, cp, cy;

	if( degrees )
	{
		SinCos( DEG2RAD( angles.z ) * 0.5f, &sy, &cy );
		SinCos( DEG2RAD( angles.x ) * 0.5f, &sp, &cp );
		SinCos( DEG2RAD( angles.y ) * 0.5f, &sr, &cr );
	}
	else
	{
		SinCos( angles.z * 0.5f, &sy, &cy );
		SinCos( angles.y * 0.5f, &sp, &cp );
		SinCos( angles.x * 0.5f, &sr, &cr );
	}

	float srXcp = sr * cp, crXsp = cr * sp;
	quat.x = srXcp * cy - crXsp * sy; // X
	quat.y = crXsp * cy + srXcp * sy; // Y

	float crXcp = cr * cp, srXsp = sr * sp;
	quat.z = crXcp * sy - srXsp * cy; // Z
	quat.w = crXcp * cy + srXsp * sy; // W (real component)
}

/*
====================
QuaternionAngle

NOTE: return only euler angles!
====================
*/
void QuaternionAngle( const Vector4D &quat, Vector &angles, bool euler )
{
	// g-cont. it's incredible stupid way but...
	matrix3x3	temp( quat );
	temp.GetAngles( angles );
}

/*
====================
QuaternionAlign

make sure quaternions are within 180 degrees of one another,
if not, reverse q
====================
*/
void QuaternionAlign( const Vector4D &p, const Vector4D &q, Vector4D &qt )
{
	// decide if one of the quaternions is backwards
	float a = 0;
	float b = 0;
	int i;

	for( i = 0; i < 4; i++ ) 
	{
		a += (p[i] - q[i]) * (p[i] - q[i]);
		b += (p[i] + q[i]) * (p[i] + q[i]);
	}

	if( a > b ) 
	{
		for( i = 0; i < 4; i++ ) 
		{
			qt[i] = -q[i];
		}
	}
	else if( &qt != &q )
	{
		for( i = 0; i < 4; i++ ) 
		{
			qt[i] = q[i];
		}
	}
}

/*
====================
QuaternionSlerpNoAlign
====================
*/
void QuaternionSlerpNoAlign( const Vector4D &p, const Vector4D &q, float t, Vector4D &qt )
{
	float omega, cosom, sinom, sclp, sclq;

	// 0.0 returns p, 1.0 return q.
	cosom = p[0] * q[0] + p[1] * q[1] + p[2] * q[2] + p[3] * q[3];

	if(( 1.0f + cosom ) > 0.000001f )
	{
		if(( 1.0f - cosom ) > 0.000001f )
		{
			omega = acos( cosom );
			sinom = sin( omega );
			sclp = sin( (1.0f - t) * omega) / sinom;
			sclq = sin( t * omega ) / sinom;
		}
		else
		{
			// TODO: add short circuit for cosom == 1.0f?
			sclp = 1.0f - t;
			sclq = t;
		}

		for( int i = 0; i < 4; i++ )
		{
			qt[i] = sclp * p[i] + sclq * q[i];
		}
	}
	else
	{
		qt[0] = -q[1];
		qt[1] = q[0];
		qt[2] = -q[3];
		qt[3] = q[2];
		sclp = sin(( 1.0f - t ) * ( 0.5f * M_PI ));
		sclq = sin( t * ( 0.5f * M_PI ));

		for( int i = 0; i < 3; i++ )
		{
			qt[i] = sclp * p[i] + sclq * qt[i];
		}
	}
}

/*
====================
QuaternionSlerp

Quaternion sphereical linear interpolation
====================
*/
void QuaternionSlerp( const Vector4D &p, const Vector4D &q, float t, Vector4D &qt )
{
	Vector4D	q2;

	// 0.0 returns p, 1.0 return q.
	// decide if one of the quaternions is backwards
	QuaternionAlign( p, q, q2 );

	QuaternionSlerpNoAlign( p, q2, t, qt );
}

//
// lerping stuff
//

/*
===================
InterpolateOrigin

Interpolate position.
Frac is 0.0 to 1.0
===================
*/
void InterpolateOrigin( const Vector& start, const Vector& end, Vector& output, float frac, bool back )
{
	if( back ) output += frac * ( end - start );
	else output = start + frac * ( end - start );
}

/*
===================
InterpolateAngles

Interpolate Euler angles.
Frac is 0.0 to 1.0 ( i.e., should probably be clamped, but doesn't have to be )
===================
*/
void InterpolateAngles( const Vector& start, const Vector& end, Vector& output, float frac, bool back )
{
#if 0
	Vector4D src, dest;

	// convert to quaternions
	AngleQuaternion( start, src, true );
	AngleQuaternion( end, dest, true );

	Vector4D result;
	Vector out;

	// slerp
	QuaternionSlerp( src, dest, frac, result );

	// convert to euler
	QuaternionAngle( result, out );

	if( back ) output += out;
	else output = out;
#else
	for( int i = 0; i < 3; i++ )
	{
		float	ang1, ang2;

		ang1 = end[i];
		ang2 = start[i];

		float d = ang1 - ang2;

		if( d > 180 ) d -= 360;
		else if( d < -180 ) d += 360;

		output[i] += d * frac;
	}
#endif
}

/*
====================
RotatePointAroundVector
====================
*/
void RotatePointAroundVector( Vector &dst, const Vector &dir, const Vector &point, float degrees )
{
	float	t0, t1;
	float	angle, c, s;
	Vector	vr, vu, vf = dir;

	angle = DEG2RAD( degrees );
	SinCos( angle, &s, &c );
	VectorMatrix( vf, vr, vu );

	t0 = vr.x *  c + vu.x * -s;
	t1 = vr.x *  s + vu.x *  c;
	dst.x  = (t0 * vr.x + t1 * vu.x + vf.x * vf.x) * point.x
	       + (t0 * vr.y + t1 * vu.y + vf.x * vf.y) * point.y
	       + (t0 * vr.z + t1 * vu.z + vf.x * vf.z) * point.z;

	t0 = vr.y *  c + vu.y * -s;
	t1 = vr.y *  s + vu.y *  c;
	dst.y  = (t0 * vr.x + t1 * vu.x + vf.y * vf.x) * point.x
	       + (t0 * vr.y + t1 * vu.y + vf.y * vf.y) * point.y
	       + (t0 * vr.z + t1 * vu.z + vf.y * vf.z) * point.z;

	t0 = vr.z *  c + vu.z * -s;
	t1 = vr.z *  s + vu.z *  c;
	dst.z  = (t0 * vr.x + t1 * vu.x + vf.z * vf.x) * point.x
	       + (t0 * vr.y + t1 * vu.y + vf.z * vf.y) * point.y
	       + (t0 * vr.z + t1 * vu.z + vf.z * vf.z) * point.z;
}

void NormalizeAngles( Vector &angles )
{
	for( int i = 0; i < 3; i++ )
	{
		if( angles[i] > 180.0f )
		{
			angles[i] -= 360.0f;
		}
		else if( angles[i] < -180.0f )
		{
			angles[i] += 360.0f;
		}
	}
}

/*
===================
AngleBetweenVectors

===================
*/
float AngleBetweenVectors( const Vector v1, const Vector v2 )
{
	float l1 = v1.Length();
	float l2 = v2.Length();

	if( !l1 || !l2 )
		return 0.0f;

	float angle = acos( DotProduct( v1, v2 )) / (l1 * l2);

	return RAD2DEG( angle );
}

void VectorMatrix( Vector &forward, Vector &right, Vector &up )
{
	if( forward.x || forward.y )
	{
		right = Vector( forward.y, -forward.x, 0 );
		right = right.Normalize();
		up = CrossProduct( forward, right );
	}
	else
	{
		right = Vector( 1, 0, 0 );
		up = Vector( 0, 1, 0 );
	}
}

/*
==================
BoxOnPlaneSide

Returns 1, 2, or 1 + 2
==================
*/
int BoxOnPlaneSide( const Vector &emins, const Vector &emaxs, const mplane_t *p )
{
	float	dist1, dist2;
	int	sides = 0;

	// general case
	switch( p->signbits )
	{
	case 0:
		dist1 = p->normal.x * emaxs.x + p->normal.y * emaxs.y + p->normal.z * emaxs.z;
		dist2 = p->normal.x * emins.x + p->normal.y * emins.y + p->normal.z * emins.z;
		break;
	case 1:
		dist1 = p->normal.x * emins.x + p->normal.y * emaxs.y + p->normal.z * emaxs.z;
		dist2 = p->normal.x * emaxs.x + p->normal.y * emins.y + p->normal.z * emins.z;
		break;
	case 2:
		dist1 = p->normal.x * emaxs.x + p->normal.y * emins.y + p->normal.z * emaxs.z;
		dist2 = p->normal.x * emins.x + p->normal.y * emaxs.y + p->normal.z * emins.z;
		break;
	case 3:
		dist1 = p->normal.x * emins.x + p->normal.y * emins.y + p->normal.z * emaxs.z;
		dist2 = p->normal.x * emaxs.x + p->normal.y * emaxs.y + p->normal.z * emins.z;
		break;
	case 4:
		dist1 = p->normal.x * emaxs.x + p->normal.y * emaxs.y + p->normal.z * emins.z;
		dist2 = p->normal.x * emins.x + p->normal.y * emins.y + p->normal.z * emaxs.z;
		break;
	case 5:
		dist1 = p->normal.x * emins.x + p->normal.y * emaxs.y + p->normal.z * emins.z;
		dist2 = p->normal.x * emaxs.x + p->normal.y * emins.y + p->normal.z * emaxs.z;
		break;
	case 6:
		dist1 = p->normal.x * emaxs.x + p->normal.y * emins.y + p->normal.z * emins.z;
		dist2 = p->normal.x * emins.x + p->normal.y * emaxs.y + p->normal.z * emaxs.z;
		break;
	case 7:
		dist1 = p->normal.x * emins.x + p->normal.y * emins.y + p->normal.z * emins.z;
		dist2 = p->normal.x * emaxs.x + p->normal.y * emaxs.y + p->normal.z * emaxs.z;
		break;
	default:
		// shut up compiler
		dist1 = dist2 = 0;
		break;
	}

	if( dist1 >= p->dist )
		sides = 1;
	if( dist2 < p->dist )
		sides |= 2;

	return sides;
}

/*
=====================
PlaneFromPoints

Returns false if the triangle is degenrate.
The normal will point out of the clock for clockwise ordered points
=====================
*/
bool PlaneFromPoints( const Vector triangle[3], mplane_t *plane )
{
	Vector v1 = triangle[1] - triangle[0];
	Vector v2 = triangle[2] - triangle[0];
	plane->normal = CrossProduct( v2, v1 );

	if( plane->normal.Length() == 0.0f )
	{
		plane->normal = g_vecZero;
		return false;
	}

	plane->normal = plane->normal.Normalize();

	plane->dist = DotProduct( triangle[0], plane->normal );
	return true;
}

/*
=================
CategorizePlane

A slightly more complex version of SignbitsForPlane and PlaneTypeForNormal,
which also tries to fix possible floating point glitches (like -0.00000 cases)
=================
*/
void CategorizePlane( mplane_t *plane )
{
	plane->signbits = 0;
	plane->type = PLANE_NONAXIAL;

	for( int i = 0; i < 3; i++ )
	{
		if( plane->normal[i] < 0 )
		{
			plane->signbits |= (1<<i);

			if( plane->normal[i] == -1.0f )
			{
				plane->signbits = (1<<i);
				plane->normal = g_vecZero;
				plane->normal[i] = -1.0f;
				break;
			}
		}
		else if( plane->normal[i] == 1.0f )
		{
			plane->type = i;
			plane->signbits = 0;
			plane->normal = g_vecZero;
			plane->normal[i] = 1.0f;
			break;
		}
	}
}

/*
=================
ComparePlanes
=================
*/
bool ComparePlanes( mplane_t *plane, const Vector &normal, float dist )
{
	if( fabs( plane->normal.x - normal.x ) < PLANE_NORMAL_EPSILON
	 && fabs( plane->normal.y - normal.y ) < PLANE_NORMAL_EPSILON
	 && fabs( plane->normal.z - normal.z ) < PLANE_NORMAL_EPSILON
	 && fabs( plane->dist - dist ) < PLANE_DIST_EPSILON )
		return true;
	return false;
}

/*
==================
SnapVectorToGrid

==================
*/
void SnapVectorToGrid( Vector &normal )
{
	for( int i = 0; i < 3; i++ )
	{
		if( fabs( normal[i] - 1.0f ) < PLANE_NORMAL_EPSILON )
		{
			normal = g_vecZero;
			normal[i] = 1.0f;
			break;
		}

		if( fabs( normal[i] - -1.0f ) < PLANE_NORMAL_EPSILON )
		{
			normal = g_vecZero;
			normal[i] = -1.0f;
			break;
		}
	}
}

/*
==============
SnapPlaneToGrid

==============
*/
void SnapPlaneToGrid( mplane_t *plane )
{
	SnapVectorToGrid( plane->normal );

	if( fabs( plane->dist - Q_rint( plane->dist )) < PLANE_DIST_EPSILON )
		plane->dist = Q_rint( plane->dist );
}

float ColorNormalize( const Vector &in, Vector &out )
{
	float	max, scale;

	max = in.x;
	if( in.y > max )
		max = in.y;
	if( in.z > max )
		max = in.z;

	if( max == 0.0f )
		return 0.0f;

	scale = 1.0f / max;
	out = in * scale;

	return max;
}

void CalcTBN( const Vector &p0, const Vector &p1, const Vector &p2, const Vector2D &t0, const Vector2D &t1, const Vector2D& t2, Vector &s, Vector &t )
{
	// Compute the partial derivatives of X, Y, and Z with respect to S and T.
	s.Init( 0.0f, 0.0f, 0.0f );
	t.Init( 0.0f, 0.0f, 0.0f );

	// x, s, t
	Vector edge01( p1.x - p0.x, t1.x - t0.x, t1.y - t0.y );
	Vector edge02( p2.x - p0.x, t2.x - t0.x, t2.y - t0.y );

	Vector cross = CrossProduct( edge01, edge02 );

	if( fabs( cross.x ) > SMALL_FLOAT )
	{
		s.x += -cross.y / cross.x;
		t.x += -cross.z / cross.x;
	}

	// y, s, t
	edge01.Init( p1.y - p0.y, t1.x - t0.x, t1.y - t0.y );
	edge02.Init( p2.y - p0.y, t2.x - t0.x, t2.y - t0.y );

	cross = CrossProduct( edge01, edge02 );

	if( fabs( cross.x ) > SMALL_FLOAT )
	{
		s.y += -cross.y / cross.x;
		t.y += -cross.z / cross.x;
	}

	// z, s, t
	edge01.Init( p1.z - p0.z, t1.x - t0.x, t1.y - t0.y );
	edge02.Init( p2.z - p0.z, t2.x - t0.x, t2.y - t0.y );

	cross = CrossProduct( edge01, edge02 );

	if( fabs( cross.x ) > SMALL_FLOAT )
	{
		s.z += -cross.y / cross.x;
		t.z += -cross.z / cross.x;
	}

	// Normalize s and t
	s = s.Normalize();
	t = t.Normalize();
}