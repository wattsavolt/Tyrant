#include "PhysicsContext.h"
#include "Private/PhysicsContextData.h"
#include "Private/PhysicsJobSystem.h"
#include <cstdarg>
#include <cstdio>
#include <cstring>

#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/TempAllocator.h>

namespace tyr
{
	namespace
	{
		constexpr JPH::uint c_MaxJobs = 2048;
		constexpr JPH::uint c_MaxBarriers = 8;
		constexpr size_t c_TempAllocatorSize = 32 * 1024 * 1024;

		// Jolt allocates through the engine's heap.
		void* Allocate(size_t size)
		{
			return MemAlloc(size);
		}

		void* Reallocate(void* block, size_t oldSize, size_t newSize)
		{
			void* newBlock = MemAlloc(newSize);
			if (block)
			{
				memcpy(newBlock, block, oldSize < newSize ? oldSize : newSize);
				Free(block);
			}
			return newBlock;
		}

		void FreeBlock(void* block)
		{
			Free(block);
		}

		void* AlignedAllocate(size_t size, size_t alignment)
		{
			return MemAllocAligned(size, alignment);
		}

		void AlignedFreeBlock(void* block)
		{
			FreeAligned(block);
		}

		void Trace(const char* format, ...)
		{
			char message[1024];
			va_list args;
			va_start(args, format);
			vsnprintf(message, sizeof(message), format, args);
			va_end(args);
			TYR_LOG_WARNING("Jolt: %s", message);
		}

#ifdef JPH_ENABLE_ASSERTS
		bool AssertFailed(const char* expression, const char* message, const char* file, JPH::uint line)
		{
			TYR_LOG_ERROR("Jolt assert %s:%u: (%s) %s", file, line, expression, message ? message : "");
			// Breaks into the debugger.
			return true;
		}
#endif
	}

	PhysicsContext::PhysicsContext()
	{
		JPH::Allocate = &Allocate;
		JPH::Reallocate = &Reallocate;
		JPH::Free = &FreeBlock;
		JPH::AlignedAllocate = &AlignedAllocate;
		JPH::AlignedFree = &AlignedFreeBlock;
		JPH::Trace = &Trace;
		JPH_IF_ENABLE_ASSERTS(JPH::AssertFailed = &AssertFailed;)

		JPH::Factory::sInstance = new JPH::Factory();
		JPH::RegisterTypes();

		m_Data = new PhysicsContextData();
		m_Data->tempAllocator = new JPH::TempAllocatorImpl(c_TempAllocatorSize);
		m_Data->jobSystem = new PhysicsJobSystem(c_MaxJobs, c_MaxBarriers);
	}

	PhysicsContext::~PhysicsContext()
	{
		delete m_Data->jobSystem;
		delete m_Data->tempAllocator;
		delete m_Data;
		m_Data = nullptr;

		JPH::UnregisterTypes();
		delete JPH::Factory::sInstance;
		JPH::Factory::sInstance = nullptr;
	}
}
