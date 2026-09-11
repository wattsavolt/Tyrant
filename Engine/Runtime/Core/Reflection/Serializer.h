#pragma once

#include "Base/Base.h"
#include "ReflectionUtil.h"
#include "TypeRegistry.h"
#include "IO/BufferedFileStream.h"

namespace tyr
{
    class CustomObjectSerializer;
    template<typename T, uint C>
    class LocalArraySerializer;
    template<typename T>
    class ArraySerializer;
    template<typename K, typename V, bool FC>
    class HashMapSerializer;
    template<uint N>
    class LocalStringSerializer;
    template<typename T, T offsetBasis, T prime>
    class IdentifierSerializer;

    template <typename T>
    constexpr const CustomObjectSerializer* GetBuiltInCustomObjectSerializer()
    {
        if constexpr (IsLocalArray<T>::value)
        {
            using ElementType = typename LocalArrayTraits<T>::elementType;
            const uint capacity = LocalArrayTraits<T>::c_Capacity;
            return &LocalArraySerializer<ElementType, capacity>::Instance();
        }
        else if constexpr (IsArray<T>::value)
        {
            using ElementType = typename ArrayTraits<T>::elementType;
            return &ArraySerializer<ElementType>::Instance();
        }
        else if constexpr (IsHashMap<T>::value)
        {
            using KeyType = typename HashMapTraits<T>::keyType;
            using ValueType = typename HashMapTraits<T>::valueType;
            const bool hasFixedCapacity = HashMapTraits<T>::c_HasFixedCapacity;
            return &HashMapSerializer<KeyType, ValueType, hasFixedCapacity>::Instance();
        }
        else if constexpr (IsLocalString<T>::value)
        {
            const uint n = LocalStringTraits<T>::c_MaxSize;
            return &LocalStringSerializer<n>::Instance();
        }
        else if constexpr (IsIdentifier<T>::value)
        {
            using HashType = typename IdentifierTraits<T>::hashType;
            const HashType offsetBasis  = IdentifierTraits<T>::c_OffsetBasis;
            const HashType prime = IdentifierTraits<T>::c_Prime;
            return &IdentifierSerializer<HashType, offsetBasis, prime>::Instance();
        }
        return nullptr;
    }

    template <typename Class, typename FieldType>
    constexpr const CustomObjectSerializer* GetFieldBuiltInCustomObjectSerializer(FieldType Class::* fieldPtr)
    {
        using T = typename FieldTypeResolver<FieldType Class::*>::type;
        return GetBuiltInCustomObjectSerializer<T>();
    }

    class Serializer;

    template<typename T>
    void Serialize(BufferedFileStream& stream, const T& data)
    {
        if constexpr (IsLocalArray<T>::value)
        {
            using ElementType = typename LocalArrayTraits<T>::elementType;
            const uint capacity = LocalArrayTraits<T>::c_Capacity;
            LocalArraySerializer<ElementType, capacity>::Instance().SerializeObject(stream, &data);
        }
        else if constexpr (IsArray<T>::value)
        {
            using ElementType = typename ArrayTraits<T>::elementType;
            ArraySerializer<ElementType>::Instance().SerializeObject(stream, &data);
        }
        else if constexpr (IsHashMap<T>::value)
        {
            using KeyType = typename HashMapTraits<T>::keyType;
            using ValueType = typename HashMapTraits<T>::valueType;
            const bool hasFixedCapacity = HashMapTraits<T>::c_HasFixedCapacity;
            HashMapSerializer<KeyType, ValueType, hasFixedCapacity>::Instance().SerializeObject(stream, &data);
        }
        else if constexpr (IsLocalString<T>::value)
        {
            const uint n = LocalStringTraits<T>::c_MaxSize;
            LocalStringSerializer<n>::Instance().SerializeObject(stream, &data);
        }
        else if constexpr (IsIdentifier<T>::value)
        {
            using HashType = typename IdentifierTraits<T>::hashType;
            const HashType offsetBasis = IdentifierTraits<T>::c_OffsetBasis;
            const HashType prime = IdentifierTraits<T>::c_Prime;
            IdentifierSerializer<HashType, offsetBasis, prime>::Instance().SerializeObject(stream, &data);
        }
        // One-dimensional arrays only supported currently
        else if constexpr (std::is_array<T>())
        {
            using ElementType = typename CArrayTraits<T>::elementType;
            const Id64 typeID = GetTypeID<ElementType>();
            const uint count = static_cast<uint>(CArrayTraits<T>::c_Capacity);
            const TypeInfo& typeInfo = TypeRegistry::Instance().GetType(typeID);
            Serializer::Instance().SerializeCArray(stream, reinterpret_cast<const uint8*>(&data), count, typeInfo);
        }
        else if constexpr (std::is_class<T>::value)
        {
            const Id64 typeID = GetTypeID<T>();
            const TypeInfo& typeInfo = TypeRegistry::Instance().GetType(typeID);
            Serializer::Instance().SerializeObject(stream, reinterpret_cast<const uint8*>(&data), typeInfo);
        }
        else
        {
            stream.Write(&data, sizeof(T));
        }
    }

    template<typename T>
    void Deserialize(BufferedFileStream& stream, T& data)
    {
        if constexpr (IsLocalArray<T>::value)
        {
            using ElementType = typename LocalArrayTraits<T>::elementType;
            const uint capacity = LocalArrayTraits<T>::c_Capacity;
            LocalArraySerializer<ElementType, capacity>::Instance().DeserializeObject(stream, &data);
        }
        else if constexpr (IsArray<T>::value)
        {
            using ElementType = typename ArrayTraits<T>::elementType;
            ArraySerializer<ElementType>::Instance().DeserializeObject(stream, &data);
        }
        else if constexpr (IsHashMap<T>::value)
        {
            using KeyType = typename HashMapTraits<T>::keyType;
            using ValueType = typename HashMapTraits<T>::valueType;
            const bool hasFixedCapacity = HashMapTraits<T>::c_HasFixedCapacity;
            HashMapSerializer<KeyType, ValueType, hasFixedCapacity>::Instance().DeserializeObject(stream, &data);
        }
        else if constexpr (IsLocalString<T>::value)
        {
            const uint n = LocalStringTraits<T>::c_MaxSize;
            LocalStringSerializer<n>::Instance().DeserializeObject(stream, &data);
        }
        else if constexpr (IsIdentifier<T>::value)
        {
            using HashType = typename IdentifierTraits<T>::hashType;
            const HashType offsetBasis = IdentifierTraits<T>::c_OffsetBasis;
            const HashType prime = IdentifierTraits<T>::c_Prime;
            IdentifierSerializer<HashType, offsetBasis, prime>::Instance().DeserializeObject(stream, &data);
        }
        // One-dimensional arrays only supported currently
        else if constexpr (std::is_array<T>())
        {
            using ElementType = typename CArrayTraits<T>::elementType;
            const Id64 typeID = GetTypeID<ElementType>();
            const uint count = static_cast<uint>(CArrayTraits<T>::c_Capacity);
            const TypeInfo& typeInfo = TypeRegistry::Instance().GetType(typeID);
            Serializer::Instance().DeserializeCArray(stream, reinterpret_cast<uint8*>(&data), count, typeInfo);
        }
        else if constexpr (std::is_class<T>::value)
        {
            const Id64 typeID = GetTypeID<T>();
            const TypeInfo& typeInfo = TypeRegistry::Instance().GetType(typeID);
            Serializer::Instance().DeserializeObject(stream, reinterpret_cast<uint8*>(&data), typeInfo);
        }
        else
        {
            stream.Read(&data, sizeof(T));
        }
    }

    struct Field;
    struct TypeInfo;
    class TYR_CORE_API Serializer final
    {
    public:
        ~Serializer() = default;

        static Serializer& Instance();

        template <typename T>
        void SerializeToFile(const char* filePath, const T& data, bool overwrite = true)
        {
            BufferedFileStream stream(m_Buffer.Data(), m_Buffer.Size(), filePath, BinaryStream::Operation::Write, overwrite);
            Serialize<T>(stream, data);
        }

        template <typename T>
        void DeserializeFromFile(const char* filePath, T& data)
        {
            BufferedFileStream stream(m_Buffer.Data(), m_Buffer.Size(), filePath, BinaryStream::Operation::Read);
            Deserialize<T>(stream, data);
        }

        void SerializeCArray(BufferedFileStream& stream, const uint8* data, uint count, const TypeInfo& typeInfo);

        void SerializeObject(BufferedFileStream& stream, const uint8* data, const TypeInfo& typeInfo);

        void SerializeValue(BufferedFileStream& stream, const uint8* data, const TypeInfo& typeInfo);

        void DeserializeField(BufferedFileStream& stream, uint8* data, const Field& field);

        void DeserializeCArray(BufferedFileStream& stream, uint8* data, const TypeInfo& typeInfo);

        void DeserializeObject(BufferedFileStream& stream, uint8* data, const TypeInfo& typeInfo);

        void SetSerializeNonFinal(bool serializeNonFinal);

        bool IsSerializingNonFinal() const { return m_SerializeNonFinal; }

    private:
        Serializer(bool serializeNonFinal);

        // Unused currently and might be removed
        void SerializeVersion(BufferedFileStream& stream, int version);
        void SerializeField(BufferedFileStream& stream, const uint8* data, const Field& field);
       
        // Unused currently and might be removed
        int DeserializeVersion(BufferedFileStream& stream);
        void DeserializeValue(BufferedFileStream& stream, uint8* data, const TypeInfo& typeInfo);

        // The size of an object for writing or reading can be greater than the size of the buffer.
        // Buffer is for temporary storage so files can be read from and written to in large chunks greatly reducing the number of file reads and writes. 
        Array<uint8> m_Buffer;
        // Should editor-only / debug fields be included
        // Note: The editor will only load in types that include non-final fields
        // but can serialize including or excluding (packaging) non-final fields
        bool m_SerializeNonFinal;
    };

    class CustomObjectSerializer
    {
    public:
        virtual void SerializeObject(BufferedFileStream& stream, const void* Object) const = 0;
        virtual void DeserializeObject(BufferedFileStream& stream, void* Object) const = 0;

    protected:
        virtual ~CustomObjectSerializer() = default;
    };

    template<typename T, uint C>
    class LocalArraySerializer : public CustomObjectSerializer
    {
    public:
        void SerializeObject(BufferedFileStream& stream, const void* object) const override
        {
            const LocalArray<T, C>& arr = *(static_cast<const LocalArray<T, C>*>(object));
            const uint size = arr.Size();
            Serialize<uint>(stream, size);
            for (const T& elem : arr)
            {
                Serialize<T>(stream, elem);
            }
        }

        void DeserializeObject(BufferedFileStream& stream, void* object) const override
        {
            LocalArray<T, C>& arr = *(static_cast<LocalArray<T, C>*>(object));
            arr.Clear();
            uint size;
            Deserialize<uint>(stream, size);
            for (uint i = 0; i < size; ++i)
            {
                T& elem = arr.ExpandOne();
                Deserialize<T>(stream, elem);
            }
        }

        static const LocalArraySerializer<T, C>& Instance()
        {
            static LocalArraySerializer<T, C> serializer;
            return serializer;
        }
    };

    template<typename T>
    class ArraySerializer : public CustomObjectSerializer
    {
    public:
        void SerializeObject(BufferedFileStream& stream, const void* object) const override
        {
            const Array<T>& arr = *(static_cast<const Array<T>*>(object));
            const uint size = arr.Size();
            Serialize<uint>(stream, size);
            for (const T& elem : arr)
            {
                Serialize<T>(stream, elem);
            }     
        }

        void DeserializeObject(BufferedFileStream& stream, void* object) const override
        {
            Array<T>& arr = *(static_cast<Array<T>*>(object));
            arr.Clear();
            uint size;
            Deserialize<uint>(stream, size);
            arr.Reserve(size);
            for (uint i = 0; i < size; ++i)
            {
                T elem;
                Deserialize<T>(stream, elem);
                arr.Add(std::move(elem));
            }
        }

        static const ArraySerializer<T>& Instance()
        {
            static ArraySerializer<T> serializer;
            return serializer;
        }
    };

    template<typename K, typename V, bool FC>
    class HashMapSerializer : public CustomObjectSerializer
    {
    public:
        void SerializeObject(BufferedFileStream& stream, const void* object) const override
        {
            const HashMap<K, V, FC>& map = *(static_cast<const HashMap<K, V, FC>*>(object));
            const uint size = map.Size();
            Serialize<uint>(stream, size);
            for (const auto& pair : map)
            {
                Serialize<K>(stream, pair.first);
                Serialize<V>(stream, pair.second);
            }
        }

        void DeserializeObject(BufferedFileStream& stream, void* object) const override
        {
            HashMap<K, V>& map = *(static_cast<HashMap<K, V>*>(object));
            map.Clear();
            uint size;
            Deserialize<uint>(stream, size);
            // This Reserve must not cause an increase in capacity if it's a fixed capacity hash map
            map.Reserve(size);
            for (uint i = 0; i < size; ++i)
            {
                K key;
                V value;
                Deserialize<K>(stream, key);
                Deserialize<V>(stream, value);
                map[key] = value;
            }
        }

        static const HashMapSerializer<K, V, FC>& Instance()
        {
            static HashMapSerializer<K, V, FC> serializer;
            return serializer;
        }
    };

    template<uint N>
    class LocalStringSerializer : public CustomObjectSerializer
    {
    public:
        void SerializeObject(BufferedFileStream& stream, const void* object) const override
        {
            const LocalString<N>& str = *(static_cast<const LocalString<N>*>(object));
            const uint size = str.Size();
            Serialize<uint>(stream, size);
            stream.Write(str.CStr(), str.Size());
        }

        void DeserializeObject(BufferedFileStream& stream, void* object) const override
        {
            LocalString<N>& str = *(static_cast<LocalString<N>*>(object));
            str.Reset();
            uint size;
            Deserialize<uint>(stream, size);
            char data[LocalString<N>::c_Capacity];
            stream.Read(data, size);
            data[size] = '\0';
            str = data;
        }

        static const LocalStringSerializer<N>& Instance()
        {
            static LocalStringSerializer<N> serializer;
            return serializer;
        }
    };

    template<typename T, T offsetBasis, T prime>
    class IdentifierSerializer : public CustomObjectSerializer
    {
    public:
        void SerializeObject(BufferedFileStream& stream, const void* object) const override
        {
            const Identifier<T, offsetBasis, prime>& id = *(static_cast<const Identifier<T, offsetBasis, prime>*>(object));
            // Important to create a copy of the hash here as passing GetHash() directly will lead to undefined behaviour
            const T hash = id.GetHash();
            Serialize<T>(stream, hash);
        }

        void DeserializeObject(BufferedFileStream& stream, void* object) const override
        {
            Identifier<T, offsetBasis, prime>& id = *(static_cast<Identifier<T, offsetBasis, prime>*>(object));
            T hash;
            Deserialize<T>(stream, hash);
            id = hash;
        }

        static const IdentifierSerializer<T, offsetBasis, prime>& Instance()
        {
            static IdentifierSerializer<T, offsetBasis, prime> serializer;
            return serializer;
        }
    };
}