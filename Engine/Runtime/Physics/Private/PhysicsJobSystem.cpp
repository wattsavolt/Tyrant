#include "PhysicsJobSystem.h"
#include "Threading/TaskScheduler.h"

namespace tyr
{
	PhysicsJobSystem::PhysicsJobSystem(JPH::uint maxJobs, JPH::uint maxBarriers)
	{
		JobSystemWithBarrier::Init(maxBarriers);
		m_Jobs.Init(maxJobs, maxJobs);
	}

	int PhysicsJobSystem::GetMaxConcurrency() const
	{
		// The workers plus the thread waiting on the step, which runs jobs while it waits.
		return static_cast<int>(TaskScheduler::c_MaxWorkers) + 1;
	}

	JPH::JobHandle PhysicsJobSystem::CreateJob(const char* name, JPH::ColorArg colour, const JobFunction& function, JPH::uint32 dependencyCount)
	{
		const JPH::uint32 index = m_Jobs.ConstructObject(name, colour, this, function, dependencyCount);
		TYR_ASSERT(index != JPH::FixedSizeFreeList<Job>::cInvalidObjectIndex);
		Job* job = &m_Jobs.Get(index);

		// The handle keeps the job alive, since a queued job can finish straight away.
		const JPH::JobHandle handle(job);
		if (dependencyCount == 0)
		{
			QueueJob(job);
		}
		return handle;
	}

	void PhysicsJobSystem::QueueJob(Job* job)
	{
		// A barrier's waiting thread may run the job first, in which case Execute does nothing.
		job->AddRef();
		TaskScheduler::Instance().CreateAndEnqueueTask([job]()
		{
			job->Execute();
			job->Release();
		});
	}

	void PhysicsJobSystem::QueueJobs(Job** jobs, JPH::uint jobCount)
	{
		for (JPH::uint i = 0; i < jobCount; ++i)
		{
			QueueJob(jobs[i]);
		}
	}

	void PhysicsJobSystem::FreeJob(Job* job)
	{
		m_Jobs.DestructObject(job);
	}
}
