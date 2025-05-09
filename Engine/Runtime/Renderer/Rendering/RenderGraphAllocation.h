
#pragma once

#include "Memory/Allocation.h"
#include "Utility/Utility.h"
#include "Containers/Containers.h"

namespace tyr
{
	/// A render graph allocator which uses a scratch allocator per thread  
	/// Should only be used on the render thread
	class RenderGraphAllocator
	{
	public:
		static void Create(uint blockSize = 1024 * 1024);

		static uint8* Alloc(uint amount);

		static uint8* AllocAligned(uint amount, uint alignment);

		static void Reset();

		static void Destroy();

	private:
		static class ScratchAllocator* s_Allocator;
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

	template <typename T, typename A = StdAllocator<T, RenderGraphAllocator>>
	using RGVector = std::vector<T, A>;

	template <typename T, typename A = StdAllocator<T, RenderGraphAllocator>>
	using RGStack = std::stack<T, std::deque<T, A>>;

	template <typename T, typename A = StdAllocator<T, RenderGraphAllocator>>
	using RGQueue = std::queue<T, std::deque<T, A>>;

	template <typename T, typename P = std::less<T>, typename A = StdAllocator<T, RenderGraphAllocator>>
	using RGSet = std::set <T, P, A>;

	template <typename K, typename V, typename P = std::less<K>, typename A = StdAllocator<std::pair<const K, V>, RenderGraphAllocator>>
	using RGMap = std::map <K, V, P, A>;

	template <typename T, typename H = std::hash<T>, typename C = std::equal_to<T>, typename A = StdAllocator<T, RenderGraphAllocator>>
	using RGUnorderedSet = std::unordered_set <T, H, C, A>;

	template <typename K, typename V, typename H = std::hash<K>, typename C = std::equal_to<K>, typename A = StdAllocator<std::pair<const K, V>, RenderGraphAllocator>>
	using RGUnorderedMap = std::unordered_map <K, V, H, C, A>;
}



