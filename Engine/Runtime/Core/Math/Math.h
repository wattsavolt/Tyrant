
#pragma once

#include "Base/Base.h"
#include <cmath>
#include <algorithm>
#include <limits>

namespace tyr
{
	/// Different orders for 3D axes 
	enum class AxesOrder
	{
		XYZ,
		XZY,
		YXZ,
		YZX,
		ZXY,
		ZYX
	};

	/// Used for constructing different math types to be all zeros 
	enum class ArgZero
	{
		Zero
	};

	/// Used for constructing different math types to the identity value 
	enum class ArgIdentity
	{
		Identity
	};

	struct Extents2
	{
		uint width;
		uint height;
	};

	struct Extents3
	{
		uint width;
		uint height;
		uint depth;
	};

	/// Class providing math utility functions and values 
	class TYR_CORE_EXPORT Math
	{
	public:
		/// Checks if the number is valid 
		static inline bool IsNaN(float f) { return f != f; }

		/// Inverse cosine. 
		static float Acos(float f);

		/// Inverse sine. 
		static float Asin(float f);

		/// Inverse tangent. 
		static inline float Atan(float f) { return std::atan(f); }

		/// Inverse tangent with two arguments, returns angle between the X axis and the point. 
		static inline float Atan2(float y, float x) { return std::atan2(y, x); }

		/// Cosine 
		static inline float Cos(float f) { return std::cos(f); }

		/// Sine
		static inline float Sin(float f) { return std::sin(f); }

		/// Tangent
		static inline float Tan(float f) { return std::tan(f); }

		/// Square root
		static inline float Sqrt(float f) { return std::sqrt(f); }

		/// inverse of square root 
		static inline float InvSqrt(float f) { return 1.0f / std::sqrt(f); }

		/// Returns square of the provided value.
		static inline float Sqr(float f) { return f * f; }

		/// Returns base raised to the provided power. 
		static inline float Pow(float f, float power) { return std::pow(f, power); }

		/// Returns euler number (e) raised to the provided power. 
		static inline float Exp(float f) { return std::exp(f); }

		/// Returns natural (base e) logarithm of the provided value. 
		static inline float Log(float f) { return std::log(f); }

		/// Returns base 2 logarithm of the provided value. 
		static inline float Log2(float f) { return std::log(f) / c_LogBase2; }

		/// Returns base N logarithm of the provided value.
		static inline float LogN(float base, float val) { return std::log(val) / std::log(base); }

		/// Returns the absolute value. 
		static inline float Abs(float f) { return std::fabs(f); }

		/// Returns to the nearest integer. 
		static inline float Round(float f) { return std::round(f); }

		/// Returns the nearest integer equal or higher to the provided value. 
		static inline float Ceil(float val) { return std::ceil(val); }

		/// Returns the sign of the provided value as 1 or -1. 
		static float Sign(float f);

		/// Clamp a number within provided range. 
		template <typename T>
		static T Clamp(T num, T min, T max)
		{
			TYR_ASSERT(min <= max);
			return std::max(std::min(num, max), min);
		}

		/// Clamp a number within the range [0..1]. 
		template <typename T>
		static T clamp01(T num)
		{
			return std::max(std::min(num, (T)1), (T)0);
		}

		/// Checks if two floats are equal accounting for tolerable inaccuracies. 
		static inline bool ApproxEquals(float a, float b,
			float tolerance = std::numeric_limits<float>::epsilon())
		{
			return fabs(b - a) <= tolerance;
		}

		/// Checks if two doubles are equal accounting for tolerable inaccuracies. 
		static inline bool ApproxEquals(double a, double b,
			double tolerance = std::numeric_limits<double>::epsilon())
		{
			return fabs(b - a) <= tolerance;
		}

		static constexpr float c_ApproxOne = 0.99999994f;
		static constexpr float c_Infinity = std::numeric_limits<float>::infinity();
		static constexpr float c_NegInfinity = -c_Infinity;
		static constexpr float c_Sqrt2 = 1.4142135623730951f;
		static constexpr float c_InvSqrt2 = 1.0f / c_Sqrt2;
		static constexpr float c_Pi = 3.14159265358979323846f;
		static constexpr float c_TwoPi = 2.0f * c_Pi;
		static constexpr float c_HalfPi = 0.5f * c_Pi;
		static constexpr float c_QuarterPi = 0.25f * c_Pi;
		static constexpr float c_InvPi = 1.0f / c_Pi;
		static constexpr float c_InvTwoPi = 0.5f * c_InvPi;
		static constexpr float c_InvHalfPi = 2.0f * c_InvPi;
		static constexpr float c_InvQuarterPi = 0.25f * c_InvPi;
		static constexpr float c_DegToRad = c_Pi / 180.0f;
		static constexpr float c_RadToDeg = 180.0f / c_Pi;
		static const float c_LogBase2;
	};
}
