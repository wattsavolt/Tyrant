
#pragma once

#include "Math.h"

namespace tyr
{
	/// A two dimensional vector with integer values.
	class TYR_CORE_EXPORT Vector2I
	{
	public:
		Vector2I() = default;

		constexpr Vector2I(ArgZero)
			:x(0), y(0)
		{ }

		constexpr Vector2I(int x, int y)
			:x(x), y(y)
		{ }

		friend Vector2I operator* (int lhs, const Vector2I& rhs)
		{
			return Vector2I(lhs * rhs.x, lhs * rhs.y);
		}

		friend Vector2I operator/ (int lhs, const Vector2I& rhs)
		{
			return Vector2I(lhs / rhs.x, lhs / rhs.y);
		}

		friend Vector2I operator+ (Vector2I& lhs, int rhs)
		{
			return Vector2I(lhs.x + rhs, lhs.y + rhs);
		}

		friend Vector2I operator+ (int lhs, const Vector2I& rhs)
		{
			return Vector2I(lhs + rhs.x, lhs + rhs.y);
		}

		friend Vector2I operator- (const Vector2I& lhs, int rhs)
		{
			return Vector2I(lhs.x - rhs, lhs.y - rhs);
		}

		friend Vector2I operator- (const int lhs, const Vector2I& rhs)
		{
			return Vector2I(lhs - rhs.x, lhs - rhs.y);
		}

		int operator[] (uint i) const
		{
			TYR_ASSERT(i < 2);

			return *(&x + i);
		}

		int& operator[] (uint i)
		{
			TYR_ASSERT(i < 2);

			return *(&x + i);
		}

		int* Ptr()
		{
			return &x;
		}

		const int* CPtr() const
		{
			return &x;
		}

		Vector2I& operator= (int rhs)
		{
			x = rhs;
			y = rhs;

			return *this;
		}

		bool operator== (const Vector2I& rhs) const
		{
			return (x == rhs.x && y == rhs.y);
		}

		bool operator!= (const Vector2I& rhs) const
		{
			return (x != rhs.x || y != rhs.y);
		}

		Vector2I operator+ (const Vector2I& rhs) const
		{
			return Vector2I(x + rhs.x, y + rhs.y);
		}

		Vector2I operator- (const Vector2I& rhs) const
		{
			return Vector2I(x - rhs.x, y - rhs.y);
		}

		Vector2I operator* (float rhs) const
		{
			return Vector2I(x * rhs, y * rhs);
		}

		Vector2I operator* (const Vector2I& rhs) const
		{
			return Vector2I(x * rhs.x, y * rhs.y);
		}

		Vector2I operator/ (int rhs) const
		{
			TYR_ASSERT(rhs != 0);

			float inv = 1.0f / rhs;

			return Vector2I(x * inv, y * inv);
		}

		Vector2I operator/ (const Vector2I& rhs) const
		{
			return Vector2I(x / rhs.x, y / rhs.y);
		}

		const Vector2I& operator+ () const
		{
			return *this;
		}

		Vector2I operator- () const
		{
			return Vector2I(-x, -y);
		}

		Vector2I& operator+= (const Vector2I& rhs)
		{
			x += rhs.x;
			y += rhs.y;

			return *this;
		}

		Vector2I& operator+= (int rhs)
		{
			x += rhs;
			y += rhs;

			return *this;
		}

		Vector2I& operator-= (const Vector2I& rhs)
		{
			x -= rhs.x;
			y -= rhs.y;

			return *this;
		}

		Vector2I& operator-= (int rhs)
		{
			x -= rhs;
			y -= rhs;

			return *this;
		}

		Vector2I& operator*= (int rhs)
		{
			x *= rhs;
			y *= rhs;

			return *this;
		}

		Vector2I& operator*= (const Vector2I& rhs)
		{
			x *= rhs.x;
			y *= rhs.y;

			return *this;
		}

		Vector2I& operator/= (int rhs)
		{
			TYR_ASSERT(rhs != 0.0f);

			float inv = 1.0f / rhs;

			x *= inv;
			y *= inv;

			return *this;
		}

		Vector2I& operator/= (const Vector2I& rhs)
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

		int x, y;

		static const Vector2I c_Zero;
		static const Vector2I c_One;
		static const Vector2I c_Right;
		static const Vector2I c_Up;
	};

}
