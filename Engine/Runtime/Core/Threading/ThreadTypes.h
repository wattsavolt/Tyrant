#pragma once

#include <thread>
#include <chrono>
#include <mutex>

namespace tyr
{
	/// Returns the number of logical CPU cores. 
	#define TYR_MAX_CONCURRENT_THREADS std::thread::hardware_concurrency()

	/// Returns the ThreadId of the current thread. 
	#define TYR_THREAD_ID std::this_thread::get_id()

	/// Causes the current thread to sleep for the provided amount of milliseconds. 
	#define TYR_THREAD_SLEEP_MS(ms) std::this_thread::sleep_for(std::chrono::milliseconds(ms));

	/// Causes the current thread to sleep for the provided amount of nanoseconds. 
	#define TYR_THREAD_SLEEP_NS(ns) std::this_thread::sleep_for(std::chrono::nanoseconds(ns));

	using Mutex = std::mutex;

	using RecursiveMutex = std::recursive_mutex;

	using ConditionVariable = std::condition_variable;
 
	using Thread = std::thread;

	using ThreadId = std::thread::id;

	using Lock = std::unique_lock<Mutex>;

	using RecursiveLock = std::unique_lock<RecursiveMutex>;

	using LockGuard = std::lock_guard<Mutex>;

	template <typename T>
	using Atomic = std::atomic<T>;

	/// Class that allows for mutex locking or no locking via a template parameter.
	template<bool UseLock>
	class ConditionalLock
	{ };

	/// Class that can be specialized to be a scoped lock for multi-threaded use or a dummy for single-threaded purposes. 
	template<bool UseLock>
	class ScopedLock
	{ };

	/// Specialization of ConditionalLock that performs no locking. 
	template<>
	class ConditionalLock<false> final
	{
	public:
		ConditionalLock() = default;

		void Lock() { };
		void Unlock() { }
	};

	/// A ConditionalLock that uses a mutex for locking. 
	template<>
	class ConditionalLock<true> final
	{
	public:
		ConditionalLock()
			: m_Lock(m_Mutex, std::defer_lock)
		{ }

		void Lock()
		{
			m_Lock.lock();
		};

		void Unlock()
		{
			m_Lock.unlock();
		}

	private:
		friend class ScopedLock<true>;

		Mutex m_Mutex;
		tyr::Lock m_Lock;
	};

	/// Scoped lock that acts as a dummy. Can only be used with a ConditionalLock. 
	template<>
	class ScopedLock<false>
	{
	public:
		ScopedLock(ConditionalLock<false>& policy)
		{ }
	};

	/// Scoped lock that locks a mutex until it goes out of scope. 
	template<>
	class ScopedLock<true>
	{
	public:
		ScopedLock(ConditionalLock<true>& policy)
			: m_Guard(policy.m_Mutex)
		{ }

		ScopedLock(Mutex& mutex)
			: m_Guard(mutex)
		{ }

	private:
		LockGuard m_Guard;
	};
}

