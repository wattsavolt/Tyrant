

#include "Timer.h"

using namespace std::chrono;

namespace tyr
{
	Timer::Timer()
	{
		Reset();
	}

	void Timer::Reset()
	{
		m_StartTime = m_HighResClock.now();
	}

	uint64 Timer::GetMilliseconds() const
	{
		auto newTime = m_HighResClock.now();
		duration<double> d = newTime - m_StartTime;
		return duration_cast<milliseconds>(d).count();
	}

	double Timer::GetMillisecondsPrecise() const
	{
		auto newTime = m_HighResClock.now();
		duration<double> d = newTime - m_StartTime;
		return d.count() * 1000;
	}

	uint64 Timer::GetMicroseconds() const
	{
		auto newTime = m_HighResClock.now();
		duration<double> d = newTime - m_StartTime;
		return duration_cast<microseconds>(d).count();
	}

	uint64 Timer::GetStartMs() const
	{
		nanoseconds startTimeNs = m_StartTime.time_since_epoch();
		return duration_cast<milliseconds>(startTimeNs).count();
	}
}
