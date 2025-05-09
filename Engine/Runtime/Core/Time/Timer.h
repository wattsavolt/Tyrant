
#pragma once

#include "CoreMacros.h"
#include "Base/Base.h"
#include <chrono>

namespace tyr
{
	/// Timer class used for querying high precision timers. 
	class TYR_CORE_EXPORT Timer
	{
	public:
		/// Create and begin timing. 
		Timer();

		/// Reset the timer to zero. 
		void Reset();

		/// Returns time in milliseconds since timer was initialized or last reset. 
		uint64 GetMilliseconds() const;

		/// Returns time in milliseconds since timer was initialized or last reset as a double precision floating point . 
		double GetMillisecondsPrecise() const;

		/// Returns time in microseconds since timer was initialized or last reset. 
		uint64 GetMicroseconds() const;

		/// Returns the initialization time in milliseconds.
		uint64 GetStartMs() const;

	private:
		std::chrono::high_resolution_clock m_HighResClock;
		std::chrono::time_point<std::chrono::high_resolution_clock> m_StartTime;
	};

}
