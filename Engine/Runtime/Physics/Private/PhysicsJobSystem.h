#pragma once

#include <Jolt/Jolt.h>
#include <Jolt/Core/JobSystemWithBarrier.h>
#include <Jolt/Core/FixedSizeFreeList.h>

namespace tyr
{
	// Runs Jolt's jobs as TaskScheduler tasks, so physics shares the engine's worker threads.
	class PhysicsJobSystem final : public JPH::JobSystemWithBarrier
	{
	public:
		PhysicsJobSystem(JPH::uint maxJobs, JPH::uint maxBarriers);

		int GetMaxConcurrency() const override;
		JPH::JobHandle CreateJob(const char* name, JPH::ColorArg colour, const JobFunction& function, JPH::uint32 dependencyCount = 0) override;

	protected:
		void QueueJob(Job* job) override;
		void QueueJobs(Job** jobs, JPH::uint jobCount) override;
		void FreeJob(Job* job) override;

	private:
		JPH::FixedSizeFreeList<Job> m_Jobs;
	};
}
