
#pragma once

#include "Math/Math.h"

namespace tyr
{
	class Vector4;

	/// A three dimensional vector with integer values. 
	class TYR_CORE_EXPORT Vector3I
	{
	public:
		Vector3I() = default;

		constexpr Vector3I(ArgZero)
			:x(0), y(0), z(0)
		{ }

		constexpr Vector3I(int x, int y, int z)
			:x(x), y(y), z(z)
		{ }

		explicit Vector3I(const Vector4& vec);

		int operator[] (uint i) const
		{
			TYR_ASSERT(i < 3);

			return *(&x + i);
		}

		int& operator[] (uint i)
		{
			TYR_ASSERT(i < 3);

			return *(&x + i);
		}

		/// Pointer accessor for direct copying. 
		int* Ptr()
		{
			return &x;
		}

		/// Pointer accessor for direct copying. 
		const int* CPtr() const
		{
			return &x;
		}

		Vector3I& operator= (int rhs)
		{
			x = rhs;
			y = rhs;
			z = rhs;

			return *this;
		}

		bool operator== (const Vector3I& rhs) const
		{
			return (x == rhs.x && y == rhs.y && z == rhs.z);
		}

		bool operator!= (const Vector3I& rhs) const
		{
			return (x != rhs.x || y != rhs.y || z != rhs.z);
		}

		Vector3I operator+ (const Vector3I& rhs) const
		{
			return Vector3I(x + rhs.x, y + rhs.y, z + rhs.z);
		}

		Vector3I operator- (const Vector3I& rhs) const
		{
			return Vector3I(x - rhs.x, y - rhs.y, z - rhs.z);
		}

		Vector3I operator* (int rhs) const
		{
			return Vector3I(x * rhs, y * rhs, z * rhs);
		}

		Vector3I operator* (const Vector3I& rhs) const
		{
			return Vector3I(x * rhs.x, y * rhs.y, z * rhs.z);
		}

		Vector3I operator/ (int val) const
		{
			TYR_ASSERT(val != 0.0);

			float fInv = 1.0f / val;
			return Vector3I(static_cast<int>(x * fInv), static_cast<int>(y * fInv), static_cast<int>(z * fInv));
		}

		Vector3I operator/ (const Vector3I& rhs) const
		{
			return Vector3I(x / rhs.x, y / rhs.y, z / rhs.z);
		}

		const Vector3I& operator+ () const
		{
			return *this;
		}

		Vector3I operator- () const
		{
			return Vector3I(-x, -y, -z);
		}

		Vector3I& operator+= (const Vector3I& rhs)
		{
			x += rhs.x;
			y += rhs.y;
			z += rhs.z;

			return *this;
		}

		Vector3I& operator+= (int rhs)
		{
			x += rhs;
			y += rhs;
			z += rhs;

			return *this;
		}

		Vector3I& operator-= (const Vector3I& rhs)
		{
			x -= rhs.x;
			y -= rhs.y;
			z -= rhs.z;

			return *this;
		}

		Vector3I& operator-= (int rhs)
		{
			x -= rhs;
			y -= rhs;
			z -= rhs;

			return *this;
		}

		Vector3I& operator*= (int rhs)
		{
			x *= rhs;
			y *= rhs;
			z *= rhs;

			return *this;
		}

		Vector3I& operator*= (const Vector3I& rhs)
		{
			x *= rhs.x;
			y *= rhs.y;
			z *= rhs.z;

			return *this;
		}

		Vector3I& operator/= (int rhs)
		{
			TYR_ASSERT(rhs != 0);

			float inv = 1.0f / rhs;

			x = static_cast<int>(x * inv);
			y = static_cast<int>(y * inv);
			z = static_cast<int>(z * inv);

			return *this;
		}

		Vector3I& operator/= (const Vector3I& rhs)
		{
			x /= rhs.x;
			y /= rhs.y;
			z /= rhs.z;

			return *this;
		}

		friend Vector3I operator* (int lhs, const Vector3I& rhs)
		{
			return Vector3I(lhs * rhs.x, lhs * rhs.y, lhs * rhs.z);
		}

		friend Vector3I operator/ (int lhs, const Vector3I& rhs)
		{
			return Vector3I(lhs / rhs.x, lhs / rhs.y, lhs / rhs.z);
		}

		friend Vector3I operator+ (const Vector3I& lhs, int rhs)
		{
			return Vector3I(lhs.x + rhs, lhs.y + rhs, lhs.z + rhs);
		}

		friend Vector3I operator+ (int lhs, const Vector3I& rhs)
		{
			return Vector3I(lhs + rhs.x, lhs + rhs.y, lhs + rhs.z);
		}

		friend Vector3I operator- (const Vector3I& lhs, int rhs)
		{
			return Vector3I(lhs.x - rhs, lhs.y - rhs, lhs.z - rhs);
		}

		friend Vector3I operator- (int lhs, const Vector3I& rhs)
		{
			return Vector3I(lhs - rhs.x, lhs - rhs.y, lhs - rhs.z);
		}

		/// Returns the length (magnitude) of the vector. 
		float Length() const
		{
			return std::sqrt(x * x + y * y + z * z);
		}

		/// Returns the square of the length(magnitude) of the vector. 
		int SqrLength() const
		{
			return x * x + y * y + z * z;
		}

		int x, y, z;

		static const Vector3I c_Zero;
		static const Vector3I c_One;
		static const Vector3I c_Infinity;
		static const Vector3I c_Right;
		static const Vector3I c_Up;
		static const Vector3I c_Forward;
	};
}

