
#pragma once

#include "Math.h"
#include "SIMD.h"

namespace tyr
{

	/// A two dimensional vector.
	class TYR_CORE_API Half2
	{
	public:
		inline uint16 FloatToHalf(float f)
		{
			const uint bits = *(uint*)&f;
			const uint sign = (bits >> 31) & 0x1;
			int exp = ((bits >> 23) & 0xFF) - 127;
			const uint mant = bits & 0x7FFFFF;

			if (exp > 15)      // overflow, clamp to max
				exp = 15;
			else if (exp < -14) // underflow, set to zero
				return uint16(sign << 15);

			const uint16 halfExp = uint16(exp + 15);
			const uint16 halfMant = uint16(mant >> 13);

			return (sign << 15) | (halfExp << 10) | halfMant;
		}

		Half2() = default;

		constexpr Half2(ArgZero)
			:x(0), y(0)
		{ }

		Half2(float fx, float fy)
		{
#ifdef TYR_USE_AVX2
			const float vals[8] = { fx, fy, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f };
			const Reg f = SIMD::Load(vals);

			const Reg128I h = SIMD::FloatToHalf(f);
			const uint packed = SIMD::ExtractLowI(h);

			x = static_cast<uint16>(packed);
			y = static_cast<uint16>(packed >> 16);
#else
			x = FloatToHalf(fx);
			y = FloatToHalf(fy);
#endif
		}

		Half2& operator= (float rhs)
		{
			x = rhs;
			y = rhs;

			return *this;
		}

		bool operator== (const Half2& rhs) const
		{
			return (x == rhs.x && y == rhs.y);
		}

		bool operator!= (const Half2& rhs) const
		{
			return (x != rhs.x || y != rhs.y);
		}

		

		uint16 x, y;

		static const Half2 c_Zero;
		static const Half2 c_One;
		static const Half2 c_Right;
		static const Half2 c_Up;
	};

}
