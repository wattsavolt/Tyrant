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
    template<typename K, typename V>
    class HashMapSerializer;

    template <typename T>
    constexpr const CustomObjectSerializer* GetBuiltInCustomObjectSerializer()
    {
        if constexpr (IsLocalArray<T>::value)
        {
            using ElementType = typename LocalArrayTraits<T>::elementType;
            const uint C = LocalArrayTraits<T>::capacity;
            return LocalArraySerializer<ElementType, C>::Instance();
        }
        else if constexpr (IsArray<T>::value)
        {
            using ElementType = typename ArrayTraits<T>::elementType;
            return ArraySerializer<ElementType>::Instance();
        }
        else if constexpr (IsHashMap<T>::value)
        {
            using KeyType = typename HashMapTraits<T>::keyType;
            using ValueType = typename HashMapTraits<T>::valueType;
            return HashMapSerializer<KeyType, ValueType>::Instance();
        }
        return nullptr;
    }

    template <typename Class, typename FieldType>
    constexpr const CustomObjectSerializer* GetFieldBuiltInCustomObjectSerializer(FieldType Class::* fieldPtr)
    {
        using T = typename FieldTypeResolver<FieldType Class::*>::type;
        return GetBuiltInCustomObjectSerializer<T>();
    }

    struct Field;
    struct TypeInfo;
    class TYR_CORE_EXPORT Serializer 
    {
    public:
        static constexpr size_t c_BufferSize = 65536;
        static uint8* GetBuffer();

        template<typename T>
        static void Serialize(BinaryStream& stream, const T& data)
        {
            if constexpr (IsLocalArray<T>::value)
            {
                using ElementType = typename LocalArrayTraits<T>::elementType;
                const uint C = LocalArrayTraits<T>::capacity;
                LocalArraySerializer<ElementType, C>::Instance().Serialize(stream, &data);
            }
            else if constexpr (IsArray<T>::value)
            {
                using ElementType = typename ArrayTraits<T>::elementType;
                ArraySerializer<ElementType>::Instance().Serialize(stream, &data);
            }
            else if constexpr (IsHashMap<T>::value)
            {
                using KeyType = typename HashMapTraits<T>::keyType;
                using ValueType = typename HashMapTraits<T>::valueType;
                HashMapSerializer<KeyType, ValueType>::Instance().Serialize(stream, &data);
            }
            // One-dimensional arrays only supported currently
            else if constexpr (std::is_array<T>())
            { 
                using ElementType = typename CArrayTraits<T>::elementType;
                const String typeName = GetTypeName<ElementType>();
                const uint count = static_cast<uint>(CArrayTraits<T>::capacity);
                const TypeInfo& typeInfo = TypeRegistry::Instance().GetType(typeName.c_str());
                SerializeCArray(stream, reinterpret_cast<const uint8*>(&data), count, typeInfo);
            }
            else if constexpr (std::is_class<T>::value)
            {
                const String typeName = GetTypeName<T>();
                const TypeInfo& typeInfo = TypeRegistry::Instance().GetType(typeName.c_str());
                SerializeObject(stream, reinterpret_cast<const uint8*>(&data), typeInfo);
            }
            else
            {
                stream.Write(&data, sizeof(T));
            }
        }

        template<typename T>
        static void Deserialize(BinaryStream& stream, T& data)
        {
            if constexpr (IsLocalArray<T>::value)
            {
                using ElementType = typename LocalArrayTraits<T>::elementType;
                const uint C = LocalArrayTraits<T>::capacity;
                LocalArraySerializer<ElementType, C>::Instance().Deserialize(stream, &data);
            }
            else if constexpr (IsArray<T>::value)
            {
                using ElementType = typename ArrayTraits<T>::elementType;
                ArraySerializer<ElementType>::Instance().Deserialize(stream, &data);
            }
            else if constexpr (IsHashMap<T>::value)
            {
                using KeyType = typename HashMapTraits<T>::keyType;
                using ValueType = typename HashMapTraits<T>::valueType;
                HashMapSerializer<KeyType, ValueType>::Instance().Deserialize(stream, &data);
            }
            // One-dimensional arrays only supported currently
            else if constexpr (std::is_array<T>())
            {
                using ElementType = typename CArrayTraits<T>::elementType;
                const String typeName = GetTypeName<ElementType>();
                const uint count = static_cast<uint>(CArrayTraits<T>::capacity);
                const TypeInfo& typeInfo = TypeRegistry::Instance().GetType(typeName.c_str());
                DeserializeCArray(stream, reinterpret_cast<uint8*>(&data), count, typeInfo);
            }
            else if constexpr (std::is_class<T>::value)
            {
                const String typeName = GetTypeName<T>();
                const TypeInfo& typeInfo = TypeRegistry::Instance().GetType(typeName.c_str());
                DeserializeObject(stream, reinterpret_cast<uint8*>(&data), typeInfo);
            }
            else
            {
                stream.Read(&data, sizeof(T));
            }
        }

        template <typename T>
        static void SerializeToFile(const char* filePath, const T& data, bool overwrite = true)
        {
            BufferedFileStream stream(GetBuffer(), c_BufferSize, filePath, BinaryStream::Operation::Write, overwrite);
            Serialize<T>(stream, data);
        }

        template <typename T>
        static void DeserializeFromFile(const char* filePath, T& data)
        {
            BufferedFileStream stream(GetBuffer(), c_BufferSize, filePath, BinaryStream::Operation::Read);
            Deserialize<T>(stream, data);
        }

    private:
        static void SerializeVersion(BinaryStream& stream, int version);
        static void SerializeField(BinaryStream& stream, const uint8* data, const Field& field);
        static void SerializeCArray(BinaryStream& stream, const uint8* data, uint count, const TypeInfo& typeInfo);
        static void SerializeObject(BinaryStream& stream, const uint8* data, const TypeInfo& typeInfo);
        static void SerializeValue(BinaryStream& stream, const uint8* data, const TypeInfo& typeInfo);
        static int DeserializeVersion(BinaryStream& stream);
        static void DeserializeField(BinaryStream& stream, uint8* data, const Field& field);
        static void DeserializeCArray(BinaryStream& stream, uint8* data, const TypeInfo& typeInfo);
        static void DeserializeObject(BinaryStream& stream, uint8* data, const TypeInfo& typeInfo);
        static void DeserializeValue(BinaryStream& stream, uint8* data, const TypeInfo& typeInfo);
    };

    class CustomObjectSerializer
    {
    public:
        virtual void Serialize(BinaryStream& stream, const void* Object) const = 0;
        virtual void Deserialize(BinaryStream& stream, void* Object) const = 0;

    protected:
        virtual ~CustomObjectSerializer() = default;
    };

    template<typename T, uint C>
    class LocalrraySerializer : public CustomObjectSerializer
    {
    public:
        void Serialize(BinaryStream& stream, const void* object) const override
        {
            const LocalArray<T, C>& arr = *(static_cast<const LocalArray<T, C>*>(object));
            Serializer::Serialize<uint>(stream, arr.Size());
            for (const T& elem : arr)
            {
                Serializer::Serialize<T>(stream, elem);
            }
        }

        void Deserialize(BinaryStream& stream, void* object) const override
        {
            LocalArray<T, C>& arr = *(static_cast<LocalArray<T, C>*>(object));
            arr.Clear();
            uint size;
            Serializer::Deserialize<uint>(stream, size);
            for (uint i = 0; i < size; ++i)
            {
                T& elem = arr.ExpandOne();
                Serializer::Deserialize<T>(stream, elem);
            }
        }

        static const ArraySerializer<T>* Instance()
        {
            static ArraySerializer<T> serializer;
            return &serializer;
        }
    };

    template<typename T>
    class ArraySerializer : public CustomObjectSerializer
    {
    public:
        void Serialize(BinaryStream& stream, const void* object) const override
        {
            const Array<T>& arr = *(static_cast<const Array<T>*>(object));
            Serializer::Serialize<uint>(stream, arr.Size());
            for (const T& elem : arr)
            {
                Serializer::Serialize<T>(stream, elem);
            }     
        }

        void Deserialize(BinaryStream& stream, void* object) const override
        {
            Array<T>& arr = *(static_cast<Array<T>*>(object));
            arr.Clear();
            uint size;
            Serializer::Deserialize<uint>(stream, size);
            arr.Reserve(size);
            for (uint i = 0; i < size; ++i)
            {
                T elem;
                Serializer::Deserialize<T>(stream, elem);
                arr.Add(std::move(elem));
            }
        }

        static const ArraySerializer<T>* Instance()
        {
            static ArraySerializer<T> serializer;
            return &serializer;
        }
    };

    template<typename K, typename V>
    class HashMapSerializer : public CustomObjectSerializer
    {
    public:
        void Serialize(BinaryStream& stream, const void* object) const override
        {
            const HashMap<K, V>& map = *(static_cast<const HashMap<K, V>*>(object));
            Serializer::Serialize<size_t>(stream, map.size());
            for (const auto& pair : map)
            {
                Serializer::Serialize<K>(stream, pair.first);
                Serializer::Serialize<V>(stream, pair.second);
            }
        }

        void Deserialize(BinaryStream& stream, void* object) const override
        {
            HashMap<K, V>& map = *(static_cast<HashMap<K, V>*>(object));
            map.clear();
            size_t size;
            Serializer::Deserialize<size_t>(stream, size);
            map.reserve(size);
            for (size_t i = 0; i < size; ++i)
            {
                K key;
                V value;
                Serializer::Deserialize<K>(stream, key);
                Serializer::Deserialize<V>(stream, value);
                map[key] = value;
            }
        }

        static const HashMapSerializer<K, V>* Instance()
        {
            static HashMapSerializer<K, V> serializer;
            return &serializer;
        }
    };
}