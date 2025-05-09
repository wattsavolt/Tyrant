
#pragma once

#include "Math/Math.h"

namespace tyr
{
	/// A four dimensional vector. 
	class TYR_CORE_EXPORT Vector4
	{
	public:
		Vector4() = default;

		constexpr Vector4(ArgZero)
			:x(0), y(0), z(0), w(0)
		{ }

		constexpr Vector4(float x, float y, float z, float w)
			: x(x), y(y), z(z), w(w)
		{ }


		float operator[] (uint i) const
		{
			TYR_ASSERT(i < 4);

			return *(&x + i);
		}

		float& operator[] (uint i)
		{
			TYR_ASSERT(i < 4);

			return *(&x + i);
		}

		/// Pointer to first member. 
		float* Ptr()
		{
			return &x;
		}

		/// Const pointer to first member. 
		const float* CPtr() const
		{
			return &x;
		}

		Vector4& operator= (float rhs)
		{
			x = rhs;
			y = rhs;
			z = rhs;
			w = rhs;

			return *this;
		}

		bool operator== (const Vector4& rhs) const
		{
			return (x == rhs.x && y == rhs.y && z == rhs.z);
		}

		bool operator!= (const Vector4& rhs) const
		{
			return (x != rhs.x || y != rhs.y || z != rhs.z);
		}

		Vector4 operator+ (const Vector4& rhs) const
		{
			return Vector4(x + rhs.x, y + rhs.y, z + rhs.z, w + rhs.w);
		}

		Vector4 operator- (const Vector4& rhs) const
		{
			return Vector4(x - rhs.x, y - rhs.y, z - rhs.z, w - rhs.w);
		}

		Vector4 operator* (float rhs) const
		{
			return Vector4(x * rhs, y * rhs, z * rhs, w * rhs);
		}

		Vector4 operator* (const Vector4& rhs) const
		{
			return Vector4(x * rhs.x, y * rhs.y, z * rhs.z, w * rhs.w);
		}

		Vector4 operator/ (float f) const
		{
			TYR_ASSERT(f != 0.0);

			float inv = 1.0f / f;
			return Vector4(x * inv, y * inv, z * inv, w * inv);
		}

		Vector4 operator/ (const Vector4& rhs) const
		{
			return Vector4(x / rhs.x, y / rhs.y, z / rhs.z, w / rhs.w);
		}

		const Vector4& operator+ () const
		{
			return *this;
		}

		Vector4 operator- () const
		{
			return Vector4(-x, -y, -z, -w);
		}

		Vector4& operator+= (const Vector4& rhs)
		{
			x += rhs.x;
			y += rhs.y;
			z += rhs.z;
			w += rhs.w;

			return *this;
		}

		Vector4& operator+= (float rhs)
		{
			x += rhs;
			y += rhs;
			z += rhs;
			w += rhs;

			return *this;
		}

		Vector4& operator-= (const Vector4& rhs)
		{
			x -= rhs.x;
			y -= rhs.y;
			z -= rhs.z;
			w -= rhs.w;

			return *this;
		}

		Vector4& operator-= (float rhs)
		{
			x -= rhs;
			y -= rhs;
			z -= rhs;
			w -= rhs;

			return *this;
		}

		Vector4& operator*= (float rhs)
		{
			x *= rhs;
			y *= rhs;
			z *= rhs;
			w *= rhs;

			return *this;
		}

		Vector4& operator*= (const Vector4& rhs)
		{
			x *= rhs.x;
			y *= rhs.y;
			z *= rhs.z;
			w *= rhs.w;

			return *this;
		}

		Vector4& operator/= (float rhs)
		{
			TYR_ASSERT(rhs != 0.0f);

			float inv = 1.0f / rhs;

			x *= inv;
			y *= inv;
			z *= inv;
			w *= inv;

			return *this;
		}

		Vector4& operator/= (const Vector4& rhs)
		{
			x /= rhs.x;
			y /= rhs.y;
			z /= rhs.z;
			w /= rhs.w;

			return *this;
		}

		friend Vector4 operator* (float lhs, const Vector4& rhs)
		{
			return Vector4(lhs * rhs.x, lhs * rhs.y, lhs * rhs.z, lhs * rhs.w);
		}

		friend Vector4 operator/ (float lhs, const Vector4& rhs)
		{
			return Vector4(lhs / rhs.x, lhs / rhs.y, lhs / rhs.z, lhs / rhs.w);
		}

		friend Vector4 operator+ (const Vector4& lhs, float rhs)
		{
			return Vector4(lhs.x + rhs, lhs.y + rhs, lhs.z + rhs, lhs.w + rhs);
		}

		friend Vector4 operator+ (float lhs, const Vector4& rhs)
		{
			return Vector4(lhs + rhs.x, lhs + rhs.y, lhs + rhs.z, lhs + rhs.w);
		}

		friend Vector4 operator- (const Vector4& lhs, float rhs)
		{
			return Vector4(lhs.x - rhs, lhs.y - rhs, lhs.z - rhs, lhs.w - rhs);
		}

		friend Vector4 operator- (float lhs, const Vector4& rhs)
		{
			return Vector4(lhs - rhs.x, lhs - rhs.y, lhs - rhs.z, lhs - rhs.w);
		}

		/// Returns the dot product of this vector with another. 
		float Dot(const Vector4& vec) const
		{
			return x * vec.x + y * vec.y + z * vec.z + w * vec.w;
		}

		float x, y, z, w;

		static const Vector4 c_Zero;
		static const Vector4 c_One;
		static const Vector4 c_Infinity;
	};

}

