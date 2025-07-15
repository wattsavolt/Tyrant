#pragma once

#include "Containers/Array.h"

namespace tyr
{
	template <uint N>
	class ScratchAllocatorPool;

	/// A render graph allocator which uses a scratch allocator pool  
	/// Should only be used by the render graph
	class RenderGraphAllocator
	{
	public:
		static constexpr uint c_AllocatorCount = 2;

		static void Create(uint blockSize = 1024 * 1024);

		static uint8* Alloc(uint amount);

		static uint8* AllocAligned(uint amount, uint alignment);

		// Switch to the next allocator
		static void NextFrame();

		static void Destroy();

	private:
		static ScratchAllocatorPool<c_AllocatorCount>* s_ScratchAllocatorPool;
		static bool s_Initialized;
	};

	inline void* RenderGraphAlloc(uint count)
	{
		return (void*)RenderGraphAllocator::Alloc(count);
	}

	inline void* RenderGraphAllocAligned(uint count, uint alignment)
	{
		return (void*)RenderGraphAllocator::AllocAligned(count, alignment);
	}

	/// Allocates enough memory to store the specified type, but does not instantiate the object.
	template<class T>
	T* RenderGraphAlloc()
	{
		return (T*)RenderGraphAllocator::Alloc(sizeof(T));
	}

	 /// Allocates enough memory to store N objects of the specified type, but does not instantiate the objects.
	template<class T>
	T* RenderGraphAlloc(uint amount)
	{
		return (T*)RenderGraphAllocator::Alloc(sizeof(T) * amount);
	}

	 /// Allocates enough memory to hold the specified type, on the stack, and constructs the object.
	template<class T>
	T* RenderGraphNew(uint count = 0)
	{
		T* data = RenderGraphAlloc<T>(count);

		for (uint i = 0; i < count; i++)
		{
			new ((void*)&data[i]) T;
		}

		return data;
	}

	/// Allocates enough memory to hold the specified type, on the stack, and constructs the object.
	template<class T, class... Args>
	T* RenderGraphNew(Args &&...args, uint count = 0)
	{
		T* data = RenderGraphAlloc<T>(count);

		for (unsigned int i = 0; i < count; i++)
		{
			new ((void*)&data[i]) T(std::forward<Args>(args)...);
		}

		return data;
	}

	/// Destructs and deallocates last allocated entry currently located on the stack.
	template<class T>
	void RenderGraphDelete(T* data)
	{
		data->~T();
	}

	/// Destructs an array of objects and deallocates last allocated entry currently located on stack.
	template<class T>
	void RenderGraphDelete(T* data, uint count)
	{
		for (unsigned int i = 0; i < count; i++)
		{
			data[i].~T();
		}
	}

	// No need to do anything in the following three functions but maybe do something for debugging later.
	inline void RenderGraphDelete(void* data, uint count)
	{
		
	}

	inline void RenderGraphFree(void* data)
	{
		
	}

	inline void RenderGraphFreeAligned(void* data)
	{
		
	}

	/// Memory allocator implementations that uses a render graph allocator in normal new/delete/free/dealloc
	/// operators.
	/// @see MemoryAllocator for function descriptions
	template<>
	class MemoryAllocator<RenderGraphAllocator> : public MemoryAllocatorBase
	{
	public:
		static void* Allocate(size_t bytes)
		{
#if TYR_DEBUG 
			IncAllocCount();
#endif

			return RenderGraphAlloc((uint)bytes);
		}

		static void* AllocateAligned(size_t bytes, size_t alignment)
		{
#if TYR_DEBUG 
			IncAllocCount();
#endif

			return RenderGraphAllocAligned(bytes, alignment);
		}

		static void* AllocateAligned16(size_t bytes)
		{
#if TYR_DEBUG 
			IncAllocCount();
#endif

			return RenderGraphAllocAligned(bytes, 16);
		}

		static void Free(void* data)
		{
#if TYR_DEBUG 
			IncFreeCount();
#endif

			RenderGraphFree(data);
		}

		static void FreeAligned(void* ptr)
		{
#if TYR_DEBUG 
			IncFreeCount();
#endif

			RenderGraphFreeAligned(ptr);
		}

		static void FreeAligned16(void* ptr)
		{
#if TYR_DEBUG 
			IncFreeCount();
#endif

			RenderGraphFreeAligned(ptr);
		}
	};

	template<typename T>
	using RGArray = Array<T, RenderGraphAllocator>;
}



