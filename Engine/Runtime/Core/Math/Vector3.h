
#pragma once

#include "Math/Math.h"

namespace tyr
{
	class Vector4;

	/// A three dimensional vector. 
	class TYR_CORE_EXPORT Vector3
	{
	public:
		Vector3() = default;

		constexpr Vector3(ArgZero)
			:x(0), y(0), z(0)
		{ }

		constexpr Vector3(float x, float y, float z)
			:x(x), y(y), z(z)
		{ }

		explicit Vector3(const Vector4& vec);

		float operator[] (uint i) const
		{
			TYR_ASSERT(i < 3);

			return *(&x + i);
		}

		float& operator[] (uint i)
		{
			TYR_ASSERT(i < 3);

			return *(&x + i);
		}

		/// Pointer accessor for direct copying. 
		float* Ptr()
		{
			return &x;
		}

		/// Pointer accessor for direct copying. 
		const float* CPtr() const
		{
			return &x;
		}

		Vector3& operator= (float rhs)
		{
			x = rhs;
			y = rhs;
			z = rhs;

			return *this;
		}

		bool operator== (const Vector3& rhs) const
		{
			return (x == rhs.x && y == rhs.y && z == rhs.z);
		}

		bool operator!= (const Vector3& rhs) const
		{
			return (x != rhs.x || y != rhs.y || z != rhs.z);
		}

		Vector3 operator+ (const Vector3& rhs) const
		{
			return Vector3(x + rhs.x, y + rhs.y, z + rhs.z);
		}

		Vector3 operator- (const Vector3& rhs) const
		{
			return Vector3(x - rhs.x, y - rhs.y, z - rhs.z);
		}

		Vector3 operator* (float rhs) const
		{
			return Vector3(x * rhs, y * rhs, z * rhs);
		}

		Vector3 operator* (const Vector3& rhs) const
		{
			return Vector3(x * rhs.x, y * rhs.y, z * rhs.z);
		}

		Vector3 operator/ (float val) const
		{
			TYR_ASSERT(val != 0.0);

			float fInv = 1.0f / val;
			return Vector3(x * fInv, y * fInv, z * fInv);
		}

		Vector3 operator/ (const Vector3& rhs) const
		{
			return Vector3(x / rhs.x, y / rhs.y, z / rhs.z);
		}

		const Vector3& operator+ () const
		{
			return *this;
		}

		Vector3 operator- () const
		{
			return Vector3(-x, -y, -z);
		}

		Vector3& operator+= (const Vector3& rhs)
		{
			x += rhs.x;
			y += rhs.y;
			z += rhs.z;

			return *this;
		}

		Vector3& operator+= (float rhs)
		{
			x += rhs;
			y += rhs;
			z += rhs;

			return *this;
		}

		Vector3& operator-= (const Vector3& rhs)
		{
			x -= rhs.x;
			y -= rhs.y;
			z -= rhs.z;

			return *this;
		}

		Vector3& operator-= (float rhs)
		{
			x -= rhs;
			y -= rhs;
			z -= rhs;

			return *this;
		}

		Vector3& operator*= (float rhs)
		{
			x *= rhs;
			y *= rhs;
			z *= rhs;

			return *this;
		}

		Vector3& operator*= (const Vector3& rhs)
		{
			x *= rhs.x;
			y *= rhs.y;
			z *= rhs.z;

			return *this;
		}

		Vector3& operator/= (float rhs)
		{
			TYR_ASSERT(rhs != 0.0f);

			float inv = 1.0f / rhs;

			x *= inv;
			y *= inv;
			z *= inv;

			return *this;
		}

		Vector3& operator/= (const Vector3& rhs)
		{
			x /= rhs.x;
			y /= rhs.y;
			z /= rhs.z;

			return *this;
		}

		friend Vector3 operator* (float lhs, const Vector3& rhs)
		{
			return Vector3(lhs * rhs.x, lhs * rhs.y, lhs * rhs.z);
		}

		friend Vector3 operator/ (float lhs, const Vector3& rhs)
		{
			return Vector3(lhs / rhs.x, lhs / rhs.y, lhs / rhs.z);
		}

		friend Vector3 operator+ (const Vector3& lhs, float rhs)
		{
			return Vector3(lhs.x + rhs, lhs.y + rhs, lhs.z + rhs);
		}

		friend Vector3 operator+ (float lhs, const Vector3& rhs)
		{
			return Vector3(lhs + rhs.x, lhs + rhs.y, lhs + rhs.z);
		}

		friend Vector3 operator- (const Vector3& lhs, float rhs)
		{
			return Vector3(lhs.x - rhs, lhs.y - rhs, lhs.z - rhs);
		}

		friend Vector3 operator- (float lhs, const Vector3& rhs)
		{
			return Vector3(lhs - rhs.x, lhs - rhs.y, lhs - rhs.z);
		}

		/// Returns the length (magnitude) of the vector. 
		float Length() const
		{
			return std::sqrt(x * x + y * y + z * z);
		}

		/// Returns the square of the length(magnitude) of the vector. 
		float SqrLength() const
		{
			return x * x + y * y + z * z;
		}

		///	Returns the distance to another vector. 
		float Dist(const Vector3& rhs) const
		{
			return (*this - rhs).Length();
		}

		/// Returns the square of the distance to another vector. 
		float SqrDist(const Vector3& rhs) const
		{
			return (*this - rhs).SqrLength();
		}

		/// Returns the dot product of this vector with another. 
		float Dot(const Vector3& vec) const
		{
			return x * vec.x + y * vec.y + z * vec.z;
		}

		/// Calculates the cross-product of 2 vectors, that is, the vector that lies perpendicular to them both. 
		Vector3 Cross(const Vector3& vec) const
		{
			return Vector3(
				y * vec.z - z * vec.y,
				z * vec.x - x * vec.z,
				x * vec.y - y * vec.x);
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
				*this *= 1.0f / length;

			return length;
		}

		static Vector3 Normalize(const Vector3& v, float tolerance = 1e-04f)
		{
			float sqrLen = v.x * v.x + v.y * v.y + v.z * v.z;
			return v * Math::InvSqrt(sqrLen);
		}

		static Vector3 SafeNormalize(const Vector3& v, float tolerance = 1e-04f)
		{
			float sqrLen = v.x * v.x + v.y * v.y + v.z * v.z;
			if (sqrLen > tolerance)
			{
				return v * Math::InvSqrt(sqrLen);
			}

			return v;
		}

		/// Returns vector perpendicular to this vector. 
		Vector3 Perpendicular() const
		{
			static const float squareZero = (float)(1e-06 * 1e-06);

			Vector3 perpVec = this->Cross(Vector3::c_Right);

			if (perpVec.SqrLength() < squareZero)
			{
				perpVec = this->Cross(Vector3::c_Up);
			}

			perpVec.Normalize();
			return perpVec;
		}

		/// Gets the angle between 2 vectors in radians. 
		inline float AngleBetween(const Vector3& dest) const;

		/// Returns true if this vector is zero length. 
		bool isZeroLength(float tolerance = 1e-04f) const
		{
			float sqrdLen = x * x + y * y + z * z;
			return sqrdLen < tolerance;
		}

		/// Calculates a reflection vector to the plane with the given normal. 
		Vector3 reflect(const Vector3& normal) const
		{
			return Vector3(*this - (2 * this->Dot(normal) * normal));
		}

		/// Gets orthonormalized vector using Gram-Schmidt algorithm. 
		static void Orthonormalize(Vector3& v1, Vector3& v2, Vector3& v3)
		{
			v1.Normalize();

			float dot1 = v1.Dot(v2);
			v2 -= dot1 * v1;
			v2.Normalize();

			float dot2 = v2.Dot(v3);
			dot1 = v1.Dot(v3);
			v3 -= dot1 * v1 + dot2 * v2;
			v3.Normalize();
		}

		/// Calculates two vectors orthonormal to the current vector, and normalizes the current vector if not already. 
		void orthogonalComplement(Vector3& a, Vector3& b)
		{
			if (std::abs(x) > std::abs(y))
			{
				a = Vector3(-z, 0, x);
			}
			else
			{
				a = Vector3(0, z, -y);
			}

			b = Cross(a);

			Orthonormalize(*this, a, b);
		}

		/// Calculates the dot (scalar) product of two vectors. 
		static float Dot(const Vector3& a, const Vector3& b)
		{
			return a.x * b.x + a.y * b.y + a.z * b.z;
		}

		/// Calculates the cross-product of 2 vectors, that is, the vector that lies perpendicular to them both. 
		static Vector3 Cross(const Vector3& a, const Vector3& b)
		{
			return Vector3(
				a.y * b.z - a.z * b.y,
				a.z * b.x - a.x * b.z,
				a.x * b.y - a.y * b.x);
		}

		/// Linearly interpolates between the two vectors using @p t. t should be in [0, 1] range, where t = 0 corresponds
		/// to the left vector, while t = 1 corresponds to the right vector. 
		static Vector3 Lerp(float t, const Vector3& a, const Vector3& b)
		{
			return (1.0f - t) * a + t * b;
		}

		/// Checks are any of the vector components not a number. 
		inline bool IsNaN() const
		{
			return Math::IsNaN(x) || Math::IsNaN(y) || Math::IsNaN(z);
		}

		float x, y, z;

		static const Vector3 c_Zero;
		static const Vector3 c_One;
		static const Vector3 c_Infinity;
		static const Vector3 c_Right;
		static const Vector3 c_Up;
		static const Vector3 c_Forward;
	};
}

