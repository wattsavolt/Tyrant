#pragma once

#include "Allocation.h"
#include "Utility/Utility.h"
#include "Containers/Array.h"

namespace tyr
{
	/// A frame allocator which uses a scratch allocator per thread  
	class FrameAllocator
	{
	public:
		static TYR_CORE_API void Create(size_t blockSize = 1024 * 1024);

		static TYR_CORE_API uint8* Alloc(size_t size);

		static TYR_CORE_API uint8* AllocAligned(size_t size, size_t alignment);

		static TYR_CORE_API void Reset();

		static TYR_CORE_API void Destroy();

	private:
		static TYR_THREADLOCAL class ScratchAllocator* s_Allocator;
	};

	inline void* FrameAlloc(size_t size)
	{
		return (void*)FrameAllocator::Alloc(size);
	}

	inline void* FrameAllocAligned(size_t size, size_t alignment)
	{
		return (void*)FrameAllocator::AllocAligned(size, alignment);
	}

	/// Allocates enough memory to store the specified type, but does not instantiate the object.
	template<class T>
	T* FrameAlloc()
	{
		return (T*)FrameAllocator::Alloc(sizeof(T));
	}

	 /// Allocates enough memory to store N objects of the specified type, but does not instantiate the objects.
	template<class T>
	T* FrameAllocN(size_t size)
	{
		return (T*)FrameAllocator::Alloc(sizeof(T) * size);
	}

	/// Allocates enough memory to hold a single instance of T from the frame allocator,
    /// and default- or args-constructs it.
	template<class T, class... Args>
	T* FrameNew(Args&&... args)
	{
		T* data = FrameAlloc<T>();
		new (static_cast<void*>(data)) T(std::forward<Args>(args)...);
		return data;
	}

	/// Allocates enough memory to hold `count` instances of T from the frame allocator,
	/// and default-constructs each one.
	template<class T>
	T* FrameNewN(uint count)
	{
		T* data = FrameAllocN<T>(count);

		for (uint i = 0; i < count; i++)
		{
			new (static_cast<void*>(&data[i])) T();
		}

		return data;
	}

	/// Allocates enough memory to hold `count` instances of T from the frame allocator,
	/// and args-constructs each one identically.
	template<class T, class... Args>
	T* FrameNewN(uint count, Args&&... args)
	{
		T* data = FrameAllocN<T>(count);

		for (uint i = 0; i < count; i++)
		{
			new (static_cast<void*>(&data[i])) T(std::forward<Args>(args)...);
		}

		return data;
	}

	/// Destructs object
	template<class T>
	void FrameDelete(T* data)
	{
		data->~T();
	}

	/// Destructs an array of objects
	template<class T>
	void FrameDelete(T* data, uint count)
	{
		for (unsigned int i = 0; i < count; i++)
		{
			data[i].~T();
		}
	}

	// No need to do anything in the following three functions but maybe do something for debugging later.
	inline void FrameDelete(void* data, uint count)
	{
	
	}

	inline void FrameFree(void* data)
	{
		
	}

	inline void FrameFreeAligned(void* data)
	{
		
	}

	/// Memory allocator implementations that uses a frame allocator in normal new/delete/free/dealloc
	/// operators.
	/// @see MemoryAllocator for function descriptions
	template<>
	class MemoryAllocator<FrameAllocator> : public MemoryAllocatorBase
	{
	public:
		static void* Allocate(size_t bytes)
		{
#if TYR_DEBUG 
			IncAllocCount();
#endif

			return FrameAlloc((uint)bytes);
		}

		static void* AllocateAligned(size_t bytes, size_t alignment)
		{
#if TYR_DEBUG 
			IncAllocCount();
#endif

			return FrameAllocAligned((uint)bytes, alignment);
		}

		static void* AllocateAligned16(size_t bytes)
		{
#if TYR_DEBUG 
			IncAllocCount();
#endif

			return FrameAllocAligned((uint)bytes, 16);
		}

		static void Free(void* data)
		{
#if TYR_DEBUG 
			IncFreeCount();
#endif

			FrameFree(data);
		}

		static void FreeAligned(void* ptr)
		{
#if TYR_DEBUG 
			IncFreeCount();
#endif

			FrameFreeAligned(ptr);
		}

		static void FreeAligned16(void* ptr)
		{
#if TYR_DEBUG 
			IncFreeCount();
#endif

			FrameFreeAligned(ptr);
		}
	};

	template <typename T, typename A>
	using FrameArray = Array<T, FrameAllocator>;
}



