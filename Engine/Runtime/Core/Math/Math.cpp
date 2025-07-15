
#include "Math/Math.h"

namespace tyr
{

	const float Math::c_LogBase2 = std::log(2.0f);

	float Math::Sign(float f)
	{
		if (f < 0.0f)
		{
			return -1;
		}
		if (f > 0.0f)
		{
			return 1;
		}
		return 0;
	}

	float Math::Acos(float f)
	{
		if (-1.0f < f)
		{
			if (f < 1.0f)
			{
				return std::acos(f);
			}
			else
			{
				return 0;
			}
		}
		else
		{
			return c_Pi;
		}
	}

	float Math::Asin(float f)
	{
		if (-1.0f < f)
		{
			if (f < 1.0f)
			{
				return std::asin(f);
			}
			else
			{
				return c_HalfPi;
			}
		}
		else
		{
			return -c_HalfPi;
		}
	}

	uint Math::NextPowerOfTwo(uint v)
	{
		v--;
		v |= v >> 1;
		v |= v >> 2;
		v |= v >> 4;
		v |= v >> 8;
		v |= v >> 16;
		v++;

		// mask = 0 if v != 0, else all bits set if v == 0
		const uint mask = (uint)(-(v == 0));

		// if v == 0, return 1; else return v
		return (v & ~mask) | (1 & mask);
	}
}