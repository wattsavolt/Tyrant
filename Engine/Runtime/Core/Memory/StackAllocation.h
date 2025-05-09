
#pragma once

#include "Allocation.h"
#include "Utility/Utility.h"

namespace tyr
{
	/// Describes a memory stack of a certain block capacity. See MemoryStack for more information.
	///
	/// The BlockSize is the minimum size of a block. Larger blocks mean less memory allocations, but also potentially
	/// more wasted memory. If an allocation requests more bytes than BlockSize, first largest multiple is used instead.
	template <int BlockSize = 1024 * 1024>
	class MemoryStackInternal
	{
	private:
		/// A single block of memory of BlockSize size. A pointer to the first free address is stored, and a remaining
		/// size.
		class Block final
		{
		public:
			Block(uint size) 
				: m_Size(size) { }

			~Block() = default;

			/// Returns the first free address and increments the free pointer. Caller needs to ensure the remaining block
			/// size is adequate before calling.
			uint8* Alloc(uint amount)
			{
				uint8* freePtr = &m_Data[m_Pos];
				m_Pos += amount;

				return freePtr;
			}

			
			/// Deallocates the provided pointer. Deallocation must happen in opposite order from allocation otherwise
			/// corruption will occur.
			///
			/// @note	Pointer to data param isn't actually needed, but is provided for debug purposes in order to more
			///			easily track out-of-order deallocations.
			void Dealloc(uint amount)
			{
				m_Pos -= amount;
			}

			uint8* m_Data = nullptr;
			// current position in the memory block
			uint m_Pos = 0;
			uint m_Size = 0;
			Block* m_NextBlock = nullptr;
			Block* m_PrevBlock = nullptr;
		};

	public:
		MemoryStackInternal()
		{
			AllocBlock(BlockSize);
		}

		~MemoryStackInternal()
		{
			TYR_ASSERT_MSG(m_FreeBlock->m_Pos == 0, "Not all blocks were released before shutting down the stack allocator.");

			Block* curBlock = m_FreeBlock;
			while (curBlock != nullptr)
			{
				Block* nextBlock = curBlock->m_NextBlock;
				DeallocBlock(curBlock);

				curBlock = nextBlock;
			}
		}

		/// Allocates the given amount of memory on the stack.
		///
		/// @param[in]	requested	The amount to allocate in bytes.
		/// @note
		/// Allocates the memory in the currently active block if it is large enough, otherwise a new block is allocated.
		/// If the allocation is larger than the default block size, a separate block will be allocated only for that allocation,
		/// making it essentially a slower heap allocator.
		/// @note
		/// Each allocation comes with a 4 byte overhead.
		uint8* Allocate(uint requested)
		{
			uint amount = requested + sizeof(uint);

			const uint freeMem = m_FreeBlock->m_Size - m_FreeBlock->m_Pos;
			if (amount > freeMem)
			{
				// Create new block if neccessary
				AllocBlock(amount);
			}

			uint8* data = m_FreeBlock->Alloc(amount);

			TYR_ASSERT(data != nullptr);

			// Write total size to the last four bytes of the data
			uint* storedSize = reinterpret_cast<uint*>(data + requested);
			*storedSize = amount;

			return data;
		}

		/// Deallocates the given memory in the reverse order it was allocated. 
		void Deallocate()
		{
			uint* storedSize = reinterpret_cast<uint*>(&m_FreeBlock->m_Data[m_FreeBlock->m_Pos - sizeof(uint)]);
			m_FreeBlock->Dealloc(*storedSize);

			if (m_FreeBlock->m_Pos == 0)
			{
				Block* emptyBlock = m_FreeBlock;

				if (emptyBlock->m_PrevBlock != nullptr)
				{
					m_FreeBlock = emptyBlock->m_PrevBlock;
				}

				// Merge with next block
				if (emptyBlock->m_NextBlock != nullptr)
				{
					uint totalSize = emptyBlock->m_Size + emptyBlock->m_NextBlock->m_Size;

					if (emptyBlock->m_PrevBlock != nullptr)
					{
						emptyBlock->m_PrevBlock->m_NextBlock = nullptr;
					}
					else
					{
						m_FreeBlock = nullptr;
					}

					DeallocBlock(emptyBlock->m_NextBlock);
					DeallocBlock(emptyBlock);

					AllocBlock(totalSize);
				}
			}
		}

	private:
		Block* m_FreeBlock = nullptr;

		/// Allocates a new block of memory using a heap allocator. Block will never be smaller than BlockSize no matter
		/// the size requested. 
		void AllocBlock(uint wantedSize)
		{
			uint blockSize = BlockSize;
			if (wantedSize > blockSize)
			{
				blockSize = wantedSize;
			}

			Block* newBlock = nullptr;
			Block* curBlock = m_FreeBlock;

			while (curBlock != nullptr)
			{
				Block* nextBlock = curBlock->m_NextBlock;
				if (nextBlock != nullptr && nextBlock->m_Size >= blockSize)
				{
					newBlock = nextBlock;
					break;
				}

				curBlock = nextBlock;
			}

			if (newBlock == nullptr)
			{
				uint8* data = (uint8*)reinterpret_cast<uint8*>(Alloc(blockSize + sizeof(Block)));
				newBlock = new (data)Block(blockSize);
				data += sizeof(Block);

				newBlock->m_Data = data;
				newBlock->m_PrevBlock = m_FreeBlock;

				if (m_FreeBlock != nullptr)
				{
					if (m_FreeBlock->m_NextBlock != nullptr)
					{
						m_FreeBlock->m_NextBlock->m_PrevBlock = newBlock;
					}

					newBlock->m_NextBlock = m_FreeBlock->m_NextBlock;
					m_FreeBlock->m_NextBlock = newBlock;
				}
			}

			m_FreeBlock = newBlock;
		}

		/// Deallocates a block of memory. 
		void DeallocBlock(Block* block)
		{
			block->~Block();
			Free(block);
		}
	};

	/// Fast but limited type of allocator. All deallocations must happen in opposite order from
	/// allocations.
	///
	/// @note
	/// Useful for allocations that will be freed in the same function..
	/// Each allocation has a 4 bytes of appended metdata representing the total size, so don't use it for small allocations.
	/// Thread safe but each thrwad has its own stack.
	class TYR_CORE_EXPORT MemoryStack final
	{
	public:
		 /// Sets up the stack with the currently active thread. Needs to be called on a thread before doing any
		 /// allocations or deallocations.
		static MemoryStack& Instance();

		uint8* Alloc(uint amount);

		void DeallocLast();

	private:
		MemoryStackInternal<1024 * 1024> m_Stack;
	};

	inline void* StackAlloc(uint count)
	{
		return (void*)MemoryStack::Instance().Alloc(count);
	}

	/// Allocates enough memory to hold the specified type, on the stack, but does not initialize the object.
	template<class T>
	T* StackAlloc()
	{
		return (T*)MemoryStack::Instance().Alloc(sizeof(T));
	}

	 /// Allocates enough memory to hold N objects of the specified type, on the stack, but does not initialize the objects.
	template<class T>
	T* StackAlloc(uint amount)
	{
		return (T*)MemoryStack::Instance().Alloc(sizeof(T) * amount);
	}

	 /// Allocates enough memory to hold the specified type, on the stack, and constructs the object.
	template<class T>
	T* StackNew(uint count = 0)
	{
		T* data = StackAlloc<T>(count);

		for (uint i = 0; i < count; i++)
		{
			new ((void*)&data[i]) T;
		}

		return data;
	}

	/// Allocates enough memory to hold the specified type, on the stack, and constructs the object.
	template<class T, class... Args>
	T* StackNew(Args &&...args, uint count = 0)
	{
		T* data = StackAlloc<T>(count);

		for (unsigned int i = 0; i < count; i++)
		{
			new ((void*)&data[i]) T(std::forward<Args>(args)...);
		}

		return data;
	}

	/// Destructs and deallocates last allocated entry currently located on the stack.
	template<class T>
	void StackDelete(T* data)
	{
		data->~T();
		MemoryStack::Instance().DeallocLast();
	}

	/// Destructs an array of objects and deallocates last allocated entry currently located on stack.
	template<class T>
	void StackDelete(T* data, uint count)
	{
		for (unsigned int i = 0; i < count; i++)
		{
			data[i].~T();
		}

		MemoryStack::Instance().DeallocLast();
	}

	/// Deallocates last allocated entry currently located on stack. Used for arrays of primitive types.
	inline void StackDelete(void* data, uint count)
	{
		MemoryStack::Instance().DeallocLast();
	}

	inline void StackFreeLast()
	{
		MemoryStack::Instance().DeallocLast();
	}

	/// An object used to free a stack allocation when it's no longer in scope. 
	template<typename T>
	struct SmartStack
	{
		/// Provide implicit conversion to the allocated buffer so that users of this code can "pretend" this object is a
		/// pointer to the stack buffer that they wanted.
		constexpr operator T* () const& noexcept
		{
			return m_Ptr;
		}

		/// This ensures that the result of SmartStackAlloc() doesn't get passed to a function call as a temporary,
		/// or immediately assigned as a T*. Instead, the user of this class is forced to deal with this class as itself,
		/// when handling the return value of SmartStackAlloc() preventing an immediate (and erroneous) call to
		/// StackFree().
		constexpr operator T* () const&& noexcept = delete;

		explicit constexpr SmartStack(T* p, size_t count = 1)
			: m_Ptr(p), m_Count(count)
		{ }

		/// Required before C++ 17 
		SmartStack(SmartStack&& stackMem)
			: m_Ptr(std::exchange(stackMem.m_Ptr, nullptr))
			, m_Count(std::exchange(stackMem.m_Count, 0))
		{ }

		SmartStack(SmartStack const&) = delete;
		SmartStack& operator=(SmartStack&&) = delete;
		SmartStack& operator=(SmartStack const&) = delete;

		/// Frees the stack allocation. 
		~SmartStack()
		{
			if (m_Ptr != nullptr)
			{
				/// Count will only be greater than zero if the memory was initialized and not just allocated.
				if (m_Count > 0)
				{
					StackDelete(m_Ptr, (uint)m_Count);
				}
				else
				{
					StackFreeLast();
				}
			}
		}

	private:
		T* m_Ptr = nullptr;
		size_t m_Count = 0;
		bool m_MemInitialized = false;
	};

	/// Same as StackAlloc() except the returned object takes care of automatically cleaning up when it goes out of
	// scope.
	inline SmartStack<void> SmartStackAlloc(uint amount)
	{
		return SmartStack<void>(StackAlloc(amount));
	}

	/// Same as StackAlloc() except the returned object takes care of automatically cleaning up when it goes out of
	/// scope.
	template<class T>
	SmartStack<T> SmartStackAlloc()
	{
		return SmartStack<T>(StackAlloc<T>());
	}

	/// Same as StackAlloc() except the returned object takes care of automatically cleaning up when it goes out of
	/// scope.
	template<class T>
	SmartStack<T> SmartStackAlloc(uint amount)
	{
		return SmartStack<T>(StackAlloc<T>(amount));
	}

	/// Same as StackNew() except the returned object takes care of automatically cleaning up when it goes out of
	/// scope.
	template<class T>
	SmartStack<T> SmartStackNew(uint count = 1)
	{
		return SmartStack<T>(StackNew<T>(count), count);
	}

	/// Same as StackNew() except the returned object takes care of automatically cleaning up when it goes out of
	/// scope.
	template<class T, class... Args>
	SmartStack<T> SmartStackNew(Args && ... args, size_t count = 1)
	{
		return SmartStack<T>(StackNew<T>(std::forward<Args>(args)..., count), count);
	}

	/// Allows use of a stack allocator by using normal new/delete/free/dealloc operators.
	class StackAllocator
	{ };

	/// Memory allocator implementation that uses a stack allocator 
	template<>
	class MemoryAllocator<StackAllocator> : public MemoryAllocatorBase
	{
	public:
		static void* Allocate(size_t bytes)
		{
			return StackAlloc((uint)bytes);
		}

		static void FreeLast()
		{
			StackFreeLast();
		}
	};

	/// Helper class for managing stack allocation and deallocation of C struct types.
	/// Memory will be released when it goes out of scope.
	class TYR_CORE_EXPORT StackAllocManager final
	{
	public:
		StackAllocManager();
		~StackAllocManager();

		template<class T>
		T* Alloc()
		{
			m_Allocs++;
			return (T*)MemoryStack::Instance().Alloc(sizeof(T));
		}

		template<class T>
		T* Alloc(uint amount)
		{
			m_Allocs++;
			return (T*)MemoryStack::Instance().Alloc(sizeof(T) * amount);
		}

	private:
		uint64 m_Allocs;
	};

#ifndef TYR_SAFE_STACK_FREE
#define TYR_SAFE_STACK_FREE(p)  { if(p) { StackFree(p); } }
#endif
}



