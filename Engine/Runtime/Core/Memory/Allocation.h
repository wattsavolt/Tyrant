#pragma once

#undef min
#undef max

#include <new>

#if TYR_PLATFORM == TYR_PLATFORM_LINUX
#include <malloc.h>
#elif TYR_PLATFORM == TYR_PLATFORM_IOSX
#include <stdlib.h>
#endif

#include "CoreMacros.h"
#include "Base/Primitives.h"
#include <limits>

namespace tyr
{
#if TYR_PLATFORM == TYR_PLATFORM_WINDOWS
	inline void* PlatformAllocAligned(size_t size, size_t alignment)
	{
		return _aligned_malloc(size, alignment);
	}

	inline void* PlatformAllocAligned16(size_t size)
	{
		return _aligned_malloc(size, 16);
	}

	inline void PlatformFreeAligned(void* ptr)
	{
		_aligned_free(ptr);
	}

	inline void PlatformFreeAligned16(void* ptr)
	{
		_aligned_free(ptr);
	}
#elif TYR_PLATFORM == TYR_PLATFORM_LINUX || TYR_PLATFORM == TYR_PLATFORM_ANDROID 
	inline void* PlatformAllocAligned(size_t size, size_t alignment)
	{
		return ::memalign(alignment, size);
	}

	inline void* PlatformAllocAligned16(size_t size)
	{
		return ::memalign(16, size);
	}

	inline void PlatformFreeAligned(void* ptr)
	{
		::free(ptr);
	}

	inline void PlatformFreeAligned16(void* ptr)
	{
		::free(ptr);
	}
#elif TYR_PLATFORM == TYR_PLATFORM_IOSX // IOS defaults to 16 byte aligned
	inline void* PlatformAllocAligned(size_t size, size_t alignment)
	{
		void* mem = nullptr;
		(void)posix_memalign(&mem, alignment, size);
		return mem;
	}

	inline void* PlatformAllocAligned16(size_t size)
	{
		return ::malloc(size);
	}

	inline void PlatformFreeAligned(void* ptr)
	{
		::free(((void**)ptr)[-1]);
	}

	inline void PlatformFreeAligned16(void* ptr)
	{
		::free(ptr);
	}
#endif

	/// Thread safe class used for storing total number of memory allocations and deallocations, primarily for statistic
	/// purposes. 
	class MemoryCounter
	{
	public:
		static TYR_CORE_EXPORT uint64 GetAllocCount()
		{
			return s_Allocs;
		}

		static TYR_CORE_EXPORT uint64 GetFreeCount()
		{
			return s_Frees;
		}

	private:
		friend class MemoryAllocatorBase;

		// Threadlocal data can't be exported, so some magic to make it accessible from MemoryAllocator
		static TYR_CORE_EXPORT void IncAllocCount() { ++s_Allocs; }
		static TYR_CORE_EXPORT void IncFreeCount() { ++s_Frees; }

		static TYR_THREADLOCAL uint64 s_Allocs;
		static TYR_THREADLOCAL uint64 s_Frees;
	};

	/// Base class all memory Allocators need to inherit. Provides allocation and free stats. 
	class MemoryAllocatorBase
	{
	protected:
		static void IncAllocCount() { MemoryCounter::IncAllocCount(); }
		static void IncFreeCount() { MemoryCounter::IncFreeCount(); }
	};

	/// General memory allocator 
	template<class T>
	class MemoryAllocator : public MemoryAllocatorBase
	{
	public:
		/// Allocates bytes with default alignment. 
		static void* Allocate(size_t bytes)
		{
#if TYR_DEBUG 
			IncAllocCount();
#endif

			return malloc(bytes);
		}

	    /// Allocates bytes and aligns to the specified boundary (in bytes). If the aligment is less or equal to
		/// 16 it is more efficient to use the AllocateAligned16() alternative of this method. Alignment must be power of two.
		static void* AllocateAligned(size_t bytes, size_t alignment)
		{
#if TYR_DEBUG 
			IncAllocCount();
#endif

			return PlatformAllocAligned(bytes, alignment);
		}

		/// Allocates @p bytes and aligns them to a 16 byte boundary. 
		static void* AllocateAligned16(size_t bytes)
		{
#if TYR_DEBUG 
			IncAllocCount();
#endif

			return PlatformAllocAligned16(bytes);
		}

		/// Frees the memory at the specified location. 
		static void Free(void* ptr)
		{
#if TYR_DEBUG 
			IncFreeCount();
#endif

			::free(ptr);
		}

		/// Frees memory Allocated with AllocateAligned() 
		static void FreeAligned(void* ptr)
		{
#if TYR_DEBUG 
			IncFreeCount();
#endif

			PlatformFreeAligned(ptr);
		}

		/// Frees memory Allocated with AllocateAligned16() 
		static void FreeAligned16(void* ptr)
		{
#if TYR_DEBUG 
			IncFreeCount();
#endif

			PlatformFreeAligned16(ptr);
		}
	};

	
	/// Allocates memory on the heap. Useful for long-term or infrequent allocations.	 
	class HeapAllocator
	{ };


	 /// Allocates the specified number of bytes. 
	template<class A>
	void* Alloc(size_t count)
	{
		return MemoryAllocator<A>::Allocate(count);
	}

	/// Allocates enough bytes to hold the specified type, but doesn't construct it. 
	template<class T, class A>
	T* Alloc()
	{
		return (T*)MemoryAllocator<A>::Allocate(sizeof(T));
	}

	/// Frees all the bytes Allocated at the specified location. 
	template<class A>
	void Free(void* ptr)
	{
		MemoryAllocator<A>::Free(ptr);
	}

	///**************************************************************************
	/// Default allocation/deallocation functions that use HeapAlloc 
	///**************************************************************************

	/// Allocates the specified number of bytes. 
	inline void* Alloc(size_t count)
	{
		return MemoryAllocator<HeapAllocator>::Allocate(count);
	}

	/// Allocates enough bytes to hold the specified type, but doesn't construct it. 
	template<class T>
	T* Alloc()
	{
		return (T*)MemoryAllocator<HeapAllocator>::Allocate(sizeof(T));
	}

	/// Allocates the specified number of bytes aligned to the provided boundary. Boundary is in bytes and must be a power
	/// of two.
	inline void* AllocAligned(size_t count, size_t align)
	{
		return MemoryAllocator<HeapAllocator>::AllocateAligned(count, align);
	}


	/// Allocates the specified number of bytes aligned to a 16 bytes boundary. 
	inline void* AllocAligned16(size_t count)
	{
		return MemoryAllocator<HeapAllocator>::AllocateAligned16(count);
	}

	template<class T, class A = HeapAllocator>
	T* AllocN(size_t count)
	{
		return (T*)MemoryAllocator<A>::Allocate(count * sizeof(T));
	}

	template<class T, class... Args>
	void Construct(void* ptr, Args&&... args) 
	{ 
		new(ptr) T(std::forward<Args>(args)...);
	}

	/// Frees all the bytes Allocated at the specified location. 
	inline void Free(void* ptr)
	{
		MemoryAllocator<HeapAllocator>::Free(ptr);
	}

	/// Frees memory previously Allocated with AllocAligned(). 
	inline void FreeAligned(void* ptr)
	{
		MemoryAllocator<HeapAllocator>::FreeAligned(ptr);
	}

	/// Frees memory previously Allocated with AllocAligned16(). 
	inline void FreeAligned16(void* ptr)
	{
		MemoryAllocator<HeapAllocator>::FreeAligned16(ptr);
	}

	// Allocate and instantiate multiple of the same object
	template<class T, class A = HeapAllocator>
	T* NewN(size_t count)
	{
		T* ptr = AllocN<T, A>(count);

		for (size_t i = 0; i < count; ++i)
		{
			new (&ptr[i]) T;
		}

		return ptr;
	}

	// Allocate and instantiate an object with custom allocator
	template<class T, class A, class... Args>
	T* New(Args &&...args)
	{
		return new (Alloc<T, A>()) T(std::forward<Args>(args)...);
	}

	// Allocate and instantiate an object
	template<class T, class... Args>
	T* New(Args &&...args)
	{
		return New<T, HeapAllocator>(std::forward<Args>(args)...);
	}

	/// Allocator for the standard library that internally uses a heap allocator. 
	/// Function names must be as they are below.
	template <class T, class A = HeapAllocator>
	class StdAllocator
	{
	public:
		using value_type = T;
		using pointer = value_type*;
		using const_pointer = const value_type*;
		using reference = value_type&;
		using const_reference = const value_type&;
		using size_type = std::size_t;
		using difference_type = std::ptrdiff_t;

		constexpr StdAllocator() = default;
		constexpr StdAllocator(StdAllocator&&) = default;
		constexpr StdAllocator(const StdAllocator&) = default;
		template<class U, class A2> constexpr StdAllocator(const StdAllocator<U, A2>&) { };
		template<class U, class A2> constexpr bool operator==(const StdAllocator<U, A2>&) const noexcept { return true; }
		template<class U, class A2> constexpr bool operator!=(const StdAllocator<U, A2>&) const noexcept { return false; }

		template<class U> class rebind { public: using other = StdAllocator<U, A>; };

		/// Allocates but doesn't initialize number elements of type T 
		static T* allocate(const size_t num)
		{
			if (num == 0)
			{
				return nullptr;
			}

			if (num > max_size())
			{
				return nullptr; // Error
			}

			void* const pv = Alloc<A>(num * sizeof(T));
			if (!pv)
			{ 
				return nullptr; // Error
			}

			return static_cast<T*>(pv);
		}

		/// Deallocates pointer to deleted elements. 
		static void deallocate(pointer p, size_type)
		{
			Free<A>(p);
		}

		static constexpr size_t max_size() { return std::numeric_limits<size_type>::max() / sizeof(T); }
		static constexpr void destroy(pointer p) { p->~T(); }

		template<class... Args>
		static void construct(pointer p, Args&&... args) { new(p) T(std::forward<Args>(args)...); }
	};
}

#define TYR_NEW_OVERRIDE_ENABLED 1

#if TYR_NEW_OVERRIDE_ENABLED
void* TYR_CDECL operator new(size_t cb);

void* TYR_CDECL operator new[](size_t cb);

void* TYR_CDECL operator new(size_t cb, const std::nothrow_t&) noexcept;

void TYR_CDECL operator delete(void* pv);

void TYR_CDECL operator delete[](void* pv);
#endif

#ifndef TYR_SAFE_DELETE
#define TYR_SAFE_DELETE(p)  { if(p) { delete p; p = nullptr; } }
#endif

#ifndef TYR_SAFE_DELETE_ARRAY
#define TYR_SAFE_DELETE_ARRAY(p, n)  { if(p) { delete[] p; p = nullptr; } }
#endif



