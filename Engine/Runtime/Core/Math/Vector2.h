
#pragma once

#include "Math.h"

namespace tyr
{

	/// A two dimensional vector.
	class TYR_CORE_EXPORT Vector2
	{
	public:
		Vector2() = default;

		constexpr Vector2(ArgZero)
			:x(0), y(0)
		{ }

		constexpr Vector2(float x, float y)
			:x(x), y(y)
		{ }

		friend Vector2 operator* (float lhs, const Vector2& rhs)
		{
			return Vector2(lhs * rhs.x, lhs * rhs.y);
		}

		friend Vector2 operator/ (float lhs, const Vector2& rhs)
		{
			return Vector2(lhs / rhs.x, lhs / rhs.y);
		}

		friend Vector2 operator+ (Vector2& lhs, float rhs)
		{
			return Vector2(lhs.x + rhs, lhs.y + rhs);
		}

		friend Vector2 operator+ (float lhs, const Vector2& rhs)
		{
			return Vector2(lhs + rhs.x, lhs + rhs.y);
		}

		friend Vector2 operator- (const Vector2& lhs, float rhs)
		{
			return Vector2(lhs.x - rhs, lhs.y - rhs);
		}

		friend Vector2 operator- (const float lhs, const Vector2& rhs)
		{
			return Vector2(lhs - rhs.x, lhs - rhs.y);
		}

		float operator[] (uint i) const
		{
			TYR_ASSERT(i < 2);

			return *(&x + i);
		}

		float& operator[] (uint i)
		{
			TYR_ASSERT(i < 2);

			return *(&x + i);
		}

		float* Ptr()
		{
			return &x;
		}

		const float* CPtr() const
		{
			return &x;
		}

		Vector2& operator= (float rhs)
		{
			x = rhs;
			y = rhs;

			return *this;
		}

		bool operator== (const Vector2& rhs) const
		{
			return (x == rhs.x && y == rhs.y);
		}

		bool operator!= (const Vector2& rhs) const
		{
			return (x != rhs.x || y != rhs.y);
		}

		Vector2 operator+ (const Vector2& rhs) const
		{
			return Vector2(x + rhs.x, y + rhs.y);
		}

		Vector2 operator- (const Vector2& rhs) const
		{
			return Vector2(x - rhs.x, y - rhs.y);
		}

		Vector2 operator* (float rhs) const
		{
			return Vector2(x * rhs, y * rhs);
		}

		Vector2 operator* (const Vector2& rhs) const
		{
			return Vector2(x * rhs.x, y * rhs.y);
		}

		Vector2 operator/ (float rhs) const
		{
			TYR_ASSERT(rhs != 0.0f);

			float inv = 1.0f / rhs;

			return Vector2(x * inv, y * inv);
		}

		Vector2 operator/ (const Vector2& rhs) const
		{
			return Vector2(x / rhs.x, y / rhs.y);
		}

		const Vector2& operator+ () const
		{
			return *this;
		}

		Vector2 operator- () const
		{
			return Vector2(-x, -y);
		}

		Vector2& operator+= (const Vector2& rhs)
		{
			x += rhs.x;
			y += rhs.y;

			return *this;
		}

		Vector2& operator+= (float rhs)
		{
			x += rhs;
			y += rhs;

			return *this;
		}

		Vector2& operator-= (const Vector2& rhs)
		{
			x -= rhs.x;
			y -= rhs.y;

			return *this;
		}

		Vector2& operator-= (float rhs)
		{
			x -= rhs;
			y -= rhs;

			return *this;
		}

		Vector2& operator*= (float rhs)
		{
			x *= rhs;
			y *= rhs;

			return *this;
		}

		Vector2& operator*= (const Vector2& rhs)
		{
			x *= rhs.x;
			y *= rhs.y;

			return *this;
		}

		Vector2& operator/= (float rhs)
		{
			TYR_ASSERT(rhs != 0.0f);

			float inv = 1.0f / rhs;

			x *= inv;
			y *= inv;

			return *this;
		}

		Vector2& operator/= (const Vector2& rhs)
		{
			x /= rhs.x;
			y /= rhs.y;

			return *this;
		}

		/** Returns the magnitude of the vector. */
		float Length() const
		{
			return Math::Sqrt(x * x + y * y);
		}

		/// Returns the magnitude of the vector squared. 
		float SqrdLength() const
		{
			return x * x + y * y;
		}

		/// Returns the distance to another vector. 
		float Dist(const Vector2& v) const
		{
			return (*this - v).Length();
		}

		/// Returns the square of the distance to another vector. 
		float SqrDist(const Vector2& v) const
		{
			return (*this - v).SqrdLength();
		}

		/// Returns the dot product of this vector with another.. 
		float Dot(const Vector2& v) const
		{
			return x * v.x + y * v.y;
		}

		// Returns counter-clockwise perpendicular vector
		Vector2 PerpCCW() const
		{
			return Vector2(-y, x);
		}

		// Returns clockwise perpendicular vector
		Vector2 PerpCW() const
		{
			return Vector2(y, -x);
		}

		/// Returns cross product of both vectors 
		float Cross(const Vector2& v) const
		{
			return (x * v.y) - (y * v.x);
		}

		void Floor(const Vector2& v)
		{
			if (v.x < x) x = v.x;
			if (v.y < y) y = v.y;
		}

		void Ceil(const Vector2& v)
		{
			if (v.x > x) x = v.x;
			if (v.y > y) y = v.y;
		}

		/// Checks if the length is zero.
		bool IsZeroLength(float margin = 1e-04f) const
		{
			float sqrdLen = x * x + y * y;
			return sqrdLen < margin;
		}

		Vector2 Reflect(const Vector2& normal) const
		{
			return Vector2(*this - (2 * this->Dot(normal) * normal));
		}

		float Normalize()
		{
			float length = Length();
			*this *= 1.0f / length;

			return length;
		}

		float SafeNormalize(float tolerance = 1e-04f)
		{
			float length = Length();
			if (length > (tolerance * tolerance))
			{
				*this *= 1.0f / length;
			}

			return length;
		}

		/// Creates a Vector2 from a radius and an angle (radians).
		static Vector2 FromRadiusAndAngle(float radius, float angle)
		{
			return Vector2(radius * Math::Cos(angle), radius * Math::Sin(angle));
		}

		static Vector2 Normalize(const Vector2& v, float tolerance = 1e-04f)
		{
			float sqrLen = v.x * v.x + v.y * v.y;
			return v * Math::InvSqrt(sqrLen);
		}

		static Vector2 SafeNormalize(const Vector2& v, float tolerance = 1e-04f)
		{
			float sqrLen = v.x * v.x + v.y * v.y;
			if (sqrLen > tolerance)
			{
				return v * Math::InvSqrt(sqrLen);
			}

			return v;
		}

		/// Calculates orthonormalized vector using Gram-Schmidt algorithm. 
		static void Orthonormalize(Vector2& u, Vector2& v)
		{
			u.SafeNormalize();

			float dot = u.Dot(v);
			v -= u * dot;
			v.SafeNormalize();
		}

		bool IsNaN() const
		{
			return Math::IsNaN(x) || Math::IsNaN(y);
		}

		float x, y;

		static const Vector2 c_Zero;
		static const Vector2 c_One;
		static const Vector2 c_Right;
		static const Vector2 c_Up;
	};

}
