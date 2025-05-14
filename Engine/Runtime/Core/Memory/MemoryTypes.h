
#pragma once

#include <memory>
#include <functional>
#include "Base/Base.h"
#include "Memory/Allocation.h"

namespace tyr
{
	template <typename T>
	using URef = std::unique_ptr<T>;

	template<typename Type>
	URef<Type> CreateURef(Type* data)
	{
		return std::unique_ptr<Type>(data);
	}

	template<typename T, typename... Args>
	constexpr URef<T> MakeURef(Args&&... args)
	{
		return std::make_unique<T>(std::forward<Args>(args)...);
	}

	template <typename T>
	using Ref = std::shared_ptr<T>;

	template<typename Type>
	Ref<Type> CreateRef(Type* data)
	{
		return std::shared_ptr<Type>(data);
	}

	template<typename T, typename... Args>
	constexpr Ref<T> MakeRef(Args&&... args)
	{
		return std::make_shared<T>(std::forward<Args>(args)...);
	}

	template <typename T>
	using WRef = std::weak_ptr<T>;

	class RefCountedObject
	{
	public:
		RefCountedObject() = default;
		virtual ~RefCountedObject() = default;

		virtual void AddRef() = 0;

		virtual uint RemoveRef() = 0;
	};

	class AtomicRefCountedObject : public RefCountedObject
	{
	public:
        AtomicRefCountedObject() : m_RefCount(0) {}

		// Should not be called directly
		void AddRef() override
		{
			m_RefCount.fetch_add(1, std::memory_order_relaxed);
		}

		// Should not be called directly
		uint RemoveRef() override
		{
			// fetch_sub returns the ref count before the subtraction occurs
			const uint refCount = m_RefCount.fetch_sub(1, std::memory_order_acq_rel);
			return refCount - 1;
		}

	protected:
		virtual ~AtomicRefCountedObject()
		{
			TYR_ASSERT(m_RefCount == 0);
		}

	private:
		std::atomic<uint> m_RefCount;
	};

	class SingleThreadedRefCountedObject : public RefCountedObject
	{
	public:
		SingleThreadedRefCountedObject() : m_RefCount(0) {}

		// Should not be called directly
		void AddRef() override
		{
			m_RefCount++;
		}

		// Should not be called directly
		uint RemoveRef() override
		{
			return --m_RefCount;
		}

	protected:
		virtual ~SingleThreadedRefCountedObject()
		{
			TYR_ASSERT(m_RefCount == 0);
		}

	private:
		uint m_RefCount;
	};

	using RefCountedObjectDeleter = std::function<void(RefCountedObject*)>;

    template<typename T>
    class SharedRef final
    {
        // Allow private member access to SRefs of base and derived types
        template<typename> friend class SharedRef;

    public:
        void Reset(T* ptr = nullptr)
        {
            if (m_Ptr)
            {
                if (m_Ptr->RemoveRef() == 0 && m_Deleter)
                {
                    m_Deleter(m_Ptr);
                }
            }
            m_Ptr = ptr;
        }

        SharedRef()
        {
            TYR_STATIC_ASSERT((std::is_base_of_v<RefCountedObject, T>), "T must derive from RefCountedObject");
            m_Deleter = [](RefCountedObject* obj) { delete obj; };
        }

        SharedRef(T* ptr, RefCountedObjectDeleter deleter = nullptr)
            : m_Ptr(ptr)
        {
            TYR_STATIC_ASSERT((std::is_base_of_v<RefCountedObject, T>), "T must derive from RefCountedObject");
            if (m_Ptr)
            {
                m_Ptr->AddRef();
                if (m_Deleter)
                {
                    m_Deleter = std::move(deleter);
                }
                else
                {
                    m_Deleter = [](RefCountedObject* obj) { delete obj; };
                }
            }
        }

        SharedRef(const SharedRef& other)
            : m_Ptr(other.m_Ptr), m_Deleter(other.m_Deleter)
        {
            if (m_Ptr)
            {
                m_Ptr->AddRef();
            }
        }

        SharedRef(SharedRef&& other) noexcept
            : m_Ptr(other.m_Ptr), m_Deleter(std::move(other.m_Deleter))
        {
            other.m_Ptr = nullptr;
        }

        // Conversion constructor
        template<typename U, typename = std::enable_if_t<std::is_base_of_v<T, U>>>
        SharedRef(const SharedRef<U>& other)
            : m_Ptr(other.m_Ptr)
            , m_Deleter(other.m_Deleter)
        {
            if (m_Ptr)
            {
                m_Ptr->AddRef();
            }
        }

        ~SharedRef()
        {
            Reset();
        }

        SharedRef& operator=(const SharedRef& other)
        {
            if (this != &other)
            {
                Reset(other.m_Ptr);
                m_Deleter = other.m_Deleter;
                if (m_Ptr)
                {
                    m_Ptr->AddRef();
                }
            }
            return *this;
        }

        SharedRef& operator=(SharedRef&& other) noexcept
        {
            if (this != &other)
            {
                Reset(other.m_Ptr);
                m_Deleter = std::move(other.m_Deleter);
                other.m_Ptr = nullptr;
            }
            return *this;
        }

        // Conversion assignment operator
        template<typename U, typename = std::enable_if_t<std::is_base_of_v<T, U>>>
        SharedRef<T>& operator=(const SharedRef<U>& other)
        {
            if (m_Ptr != other.m_Ptr)
            {
                Reset(other.m_Ptr);
                m_Deleter = other.m_Deleter;
                if (m_Ptr)
                {
                    m_Ptr->AddRef();
                }
            }
            return *this;
        }

        const T* Get() const
        {
            return m_Ptr;
        }

        T* Get()
        {
            return m_Ptr;
        }

        template <typename U>
        U* GetAs() const
        {
            // Skip the static assertion if T is void because void can't be a base class
            if constexpr (!std::is_same_v<T, void>)
            {
                TYR_STATIC_ASSERT((std::is_base_of<T, U>::value), "U does not derive from T");
            }

            TYR_ASSERT(m_Ptr != nullptr);

            return static_cast<U*>(m_Ptr);
        }

        T& operator*() const
        {
            TYR_ASSERT(m_Ptr);
            return *m_Ptr;
        }

        T* operator->() const
        {
            TYR_ASSERT(m_Ptr);
            return m_Ptr;
        }

        explicit operator bool() const
        {
            return m_Ptr != nullptr;
        }

        bool operator==(const SharedRef& other) const
        {
            return m_Ptr == other.m_Ptr;
        }

        bool operator!=(const SharedRef& other) const
        {
            return !(*this == other);
        }

    private:
        T* m_Ptr = nullptr;
        RefCountedObjectDeleter m_Deleter;
    };

    template<typename T>
    using SRef = SharedRef<T>;

    template<typename T, typename... Args>
    constexpr SRef<T> MakeSRef(RefCountedObjectDeleter deleter, Args&&... args)
    {
        return SRef(new T(std::forward<Args>(args)...));
    }
}

