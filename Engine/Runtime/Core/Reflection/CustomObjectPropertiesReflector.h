#pragma once

#include "Base/Base.h"
#include "ReflectionUtil.h"

namespace tyr
{
	class CustomObjectPropertiesReflector;
	template<typename T, uint C>
	class LocalArrayPropertiesReflector;
	template<uint N>
	class LocalStringPropertiesReflector;

	template <typename T>
	constexpr const CustomObjectPropertiesReflector* GetBuiltInCustomObjectPropertiesReflector()
	{
		if constexpr (IsLocalArray<T>::value)
		{
			using ElementType = typename LocalArrayTraits<T>::elementType;
			const uint capacity = LocalArrayTraits<T>::c_Capacity;
			return &LocalArrayPropertiesReflector<ElementType, capacity>::Instance();
		}
		else if constexpr (IsLocalString<T>::value)
		{
			const uint n = LocalStringTraits<T>::c_MaxSize;
			return &LocalStringPropertiesReflector<n>::Instance();
		}
		return nullptr;
	}

	template <typename Class, typename FieldType>
	constexpr const CustomObjectPropertiesReflector* GetFieldBuiltInCustomObjectPropertiesReflector(FieldType Class::* fieldPtr)
	{
		using T = typename FieldTypeResolver<FieldType Class::*>::type;
		return GetBuiltInCustomObjectPropertiesReflector<T>();
	}

	// Lets tools such as the editor read and change an object whose fields aren't reflected, like
	// a LocalArray or LocalString, through its address alone.
	class CustomObjectPropertiesReflector
	{
	public:
		enum class Type : uint8
		{
			LocalArray,
			LocalString
		};

		virtual Type GetType() const = 0;
		// The most elements, or characters not counting the null.
		virtual uint GetCapacity() const = 0;
		virtual uint GetSize(const void* object) const = 0;
		// Compares only what's in use, not leftover data past the end.
		virtual bool Equals(const void* a, const void* b) const = 0;

	protected:
		virtual ~CustomObjectPropertiesReflector() = default;
	};

	// A resizable list of elements, each of one type.
	class ArrayPropertiesReflector : public CustomObjectPropertiesReflector
	{
	public:
		Type GetType() const override { return Type::LocalArray; }
		virtual Id64 GetElementTypeID() const = 0;
		// Set when the elements aren't reflected themselves, like a LocalString.
		virtual const CustomObjectPropertiesReflector* GetElementReflector() const = 0;
		// New elements are default constructed.
		virtual void Resize(void* object, uint size) const = 0;
		virtual void* GetElement(void* object, uint index) const = 0;
	};

	class StringPropertiesReflector : public CustomObjectPropertiesReflector
	{
	public:
		Type GetType() const override { return Type::LocalString; }
		virtual const char* GetString(const void* object) const = 0;
		// value must fit within the capacity.
		virtual void SetString(void* object, const char* value) const = 0;
	};

	template<typename T, uint C>
	class LocalArrayPropertiesReflector : public ArrayPropertiesReflector
	{
	public:
		uint GetCapacity() const override { return C; }

		uint GetSize(const void* object) const override
		{
			return static_cast<const LocalArray<T, C>*>(object)->Size();
		}

		bool Equals(const void* a, const void* b) const override
		{
			const LocalArray<T, C>& arrA = *static_cast<const LocalArray<T, C>*>(a);
			const LocalArray<T, C>& arrB = *static_cast<const LocalArray<T, C>*>(b);
			if (arrA.Size() != arrB.Size())
			{
				return false;
			}
			const CustomObjectPropertiesReflector* elementReflector = GetElementReflector();
			for (uint i = 0; i < arrA.Size(); ++i)
			{
				const bool equal = elementReflector ? elementReflector->Equals(&arrA[i], &arrB[i]) : memcmp(&arrA[i], &arrB[i], sizeof(T)) == 0;
				if (!equal)
				{
					return false;
				}
			}
			return true;
		}

		Id64 GetElementTypeID() const override { return GetTypeID<T>(); }

		const CustomObjectPropertiesReflector* GetElementReflector() const override
		{
			return GetBuiltInCustomObjectPropertiesReflector<T>();
		}

		void Resize(void* object, uint size) const override
		{
			LocalArray<T, C>& arr = *static_cast<LocalArray<T, C>*>(object);
			const uint oldSize = arr.Size();
			arr.Resize(size);
			for (uint i = oldSize; i < size; ++i)
			{
				arr[i] = T{};
			}
		}

		void* GetElement(void* object, uint index) const override
		{
			return &(*static_cast<LocalArray<T, C>*>(object))[index];
		}

		static const LocalArrayPropertiesReflector<T, C>& Instance()
		{
			static LocalArrayPropertiesReflector<T, C> reflector;
			return reflector;
		}
	};

	template<uint N>
	class LocalStringPropertiesReflector : public StringPropertiesReflector
	{
	public:
		uint GetCapacity() const override { return N; }

		uint GetSize(const void* object) const override
		{
			return static_cast<uint>(static_cast<const LocalString<N>*>(object)->Size());
		}

		const char* GetString(const void* object) const override
		{
			return static_cast<const LocalString<N>*>(object)->CStr();
		}

		bool Equals(const void* a, const void* b) const override
		{
			return strcmp(GetString(a), GetString(b)) == 0;
		}

		void SetString(void* object, const char* value) const override
		{
			*static_cast<LocalString<N>*>(object) = value;
		}

		static const LocalStringPropertiesReflector<N>& Instance()
		{
			static LocalStringPropertiesReflector<N> reflector;
			return reflector;
		}
	};
}
