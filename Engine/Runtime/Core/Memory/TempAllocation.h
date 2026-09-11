#pragma once

#include "Allocation.h"
#include "Utility/Utility.h"

namespace tyr
{
	/// Allocator for short-lived allocations that will be freed in a few frames to no more than couple of seconds
	/// @note Single-threaded only
	class BlockAllocator;
	class TYR_CORE_API TempAllocator final
	{
	public:
		static TempAllocator& Instance();

		TempAllocator();

		~TempAllocator();

		uint8* Alloc(size_t size);

		uint8* AllocAligned(size_t size, size_t alignment);

		void Free(uint8* ptr);

		void FreeAligned(uint8* ptr);

	private:
		BlockAllocator* m_Allocator;
	};

	inline void* TempAlloc(size_t size)
	{
		return (void*)TempAllocator::Instance().Alloc(size);
	}

	inline void* TempAllocAligned(size_t size, size_t alignment)
	{
		return (void*)TempAllocator::Instance().AllocAligned(size, alignment);
	}

	/// Allocates enough memory to hold the specified type, on the stack, but does not initialize the object.
	template<class T>
	T* TempAlloc()
	{
		return (T*)TempAllocator::Instance().Alloc(sizeof(T));
	}

	 /// Allocates enough memory to hold N objects of the specified type, on the stack, but does not initialize the objects.
	template<class T>
	T* TempAllocN(uint count)
	{
		return (T*)TempAllocator::Instance().Alloc(sizeof(T) * count);
	}

	/// Allocates enough memory to hold the specified type, on the stack, and constructs the object.
	template<class T, class... Args>
	T* TempNew(Args&&... args)
	{
		T* data = TempAlloc<T>();
		new (static_cast<void*>(data)) T(std::forward<Args>(args)...);
		return data;
	}

	template<class T>
	T* TempNewN(uint count = 1)
	{
		T* data = TempAllocN<T>(count);
		for (uint i = 0; i < count; i++)
			new (static_cast<void*>(&data[i])) T();
		return data;
	}

	template<class T, class... Args>
	T* TempNewN(uint count, Args&&... args)
	{
		T* data = TempAllocN<T>(count);
		for (uint i = 0; i < count; i++)
			new (static_cast<void*>(&data[i])) T(std::forward<Args>(args)...);
		return data;
	}

	template<class T>
	void TempDelete(T* data)
	{
		data->~T();
		TempAllocator::Instance().Free(reinterpret_cast<uint8*>(data));
	}

	template<class T>
	void TempDelete(T* data, uint count)
	{
		for (unsigned int i = 0; i < count; i++)
		{
			data[i].~T();
		}

		TempAllocator::Instance().Free(reinterpret_cast<uint8*>(data));
	}

	TYR_FORCEINLINE void TempFree(void* data)
	{
		TempAllocator::Instance().Free(static_cast<uint8*>(data));
	}

	TYR_FORCEINLINE void TempFreeAligned(void* data)
	{
		TempAllocator::Instance().FreeAligned(static_cast<uint8*>(data));
	}

	/// Memory allocator implementation that uses a stack allocator 
	template<>
	class MemoryAllocator<TempAllocator> : public MemoryAllocatorBase
	{
	public:
		static void* Allocate(size_t bytes)
		{
#if TYR_DEBUG 
			IncAllocCount();
#endif

			return TempAlloc((uint)bytes);
		}

		static void* AllocateAligned(size_t bytes, size_t alignment)
		{
#if TYR_DEBUG 
			IncAllocCount();
#endif

			return TempAllocAligned((uint)bytes, alignment);
		}

		static void* AllocateAligned16(size_t bytes)
		{
#if TYR_DEBUG 
			IncAllocCount();
#endif

			return TempAllocAligned((uint)bytes, 16);
		}

		static void Free(void* data)
		{
#if TYR_DEBUG 
			IncFreeCount();
#endif

			TempFree(data);
		}

		static void FreeAligned(void* ptr)
		{
#if TYR_DEBUG 
			IncFreeCount();
#endif

			TempFreeAligned(ptr);
		}

		static void FreeAligned16(void* ptr)
		{
#if TYR_DEBUG 
			IncFreeCount();
#endif

			TempFreeAligned(ptr);
		}
	};
}



