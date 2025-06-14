#include "Serializer.h"
#include "TypeRegistry.h"

namespace tyr
{
    void Serializer::SerializeVersion(BinaryStream& stream, int version)
    {
        stream.Write(&version, sizeof(version));
    }

    void Serializer::SerializeField(BinaryStream& stream, const uint8* data, const Field& field)
    {
        const TypeInfo& typeInfo = TypeRegistry::Instance().GetType(field.typeID);

        if (field.customSerializer)
        {
            field.customSerializer->Serialize(stream, &data[field.dataOffset]);
        }
        else if (field.isCArray)
        {
            uint count;
            memcpy(&count, &data[field.countOffset], sizeof(uint));
            SerializeCArray(stream, &data[field.dataOffset], count, typeInfo);
        }
        else if (typeInfo.fieldCount > 0)
        {
            SerializeObject(stream, &data[field.dataOffset], typeInfo);
        }
        else
        {
            // Must be a value type
            SerializeValue(stream, &data[field.dataOffset], typeInfo);
        }
    }

    void Serializer::SerializeCArray(BinaryStream& stream, const uint8* data, uint count, const TypeInfo& typeInfo)
    {
        stream.Write(&count, sizeof(count));
        if (typeInfo.fieldCount > 0)
        {
            for (uint i = 0; i < count; ++i)
            {
                SerializeObject(stream, &data[i * typeInfo.size], typeInfo);
            }
        }
        else
        {
            for (uint i = 0; i < count; ++i)
            {
                SerializeValue(stream, &data[i * typeInfo.size], typeInfo);
            }
        }
    }

    void Serializer::SerializeObject(BinaryStream& stream, const uint8* data, const TypeInfo& typeInfo)
    {
        SerializeVersion(stream, typeInfo.version);
        for (uint i = 0; i < typeInfo.fieldCount; ++i)
        {
            const Field& field = typeInfo.fields[i];
            SerializeField(stream, data, field);
        }
    }

    void Serializer::SerializeValue(BinaryStream& stream, const uint8* data, const TypeInfo& typeInfo)
    {
        stream.Write(data, typeInfo.size);
    }

    int Serializer::DeserializeVersion(BinaryStream& stream)
    {
        int version;
        stream.Read(&version, sizeof(version));
        return version;
    }

    void Serializer::DeserializeField(BinaryStream& stream, uint8* data, const Field& field)
    {
        const TypeInfo& typeInfo = TypeRegistry::Instance().GetType(field.typeID);

        if (field.customSerializer)
        {
            field.customSerializer->Deserialize(stream, &data[field.dataOffset]);
        }
        else if (field.isCArray)
        {
            DeserializeCArray(stream, &data[field.dataOffset], typeInfo);
        }
        else if (typeInfo.fieldCount > 0)
        {
            DeserializeObject(stream, &data[field.dataOffset], typeInfo);
        }
        else
        {
            // Must be a value type
            DeserializeValue(stream, &data[field.dataOffset], typeInfo);
        }
    }

    void Serializer::DeserializeCArray(BinaryStream& stream, uint8* data, const TypeInfo& typeInfo)
    {
        uint count;
        stream.Read(&count, sizeof(count));
        if (typeInfo.fieldCount > 0)
        {
            for (uint i = 0; i < count; ++i)
            {
                DeserializeObject(stream, &data[i * typeInfo.size], typeInfo);
            }
        }
        else
        {
            for (uint i = 0; i < count; ++i)
            {
                DeserializeValue(stream, &data[i * typeInfo.size], typeInfo);
            }
        }
    }
    

    void Serializer::DeserializeObject(BinaryStream& stream, uint8* data, const TypeInfo& typeInfo)
    {
        const int version = DeserializeVersion(stream);
        TYR_ASSERT(version == typeInfo.version);
        for (uint i = 0; i < typeInfo.fieldCount; ++i)
        {
            const Field& field = typeInfo.fields[i];
            DeserializeField(stream, data, field);
        }
    }

    void Serializer::DeserializeValue(BinaryStream& stream, uint8* data, const TypeInfo& typeInfo)
    {
        stream.Read(data, typeInfo.size);
    }

    uint8* Serializer::GetBuffer()
    {
        static TYR_THREADLOCAL uint8 buffer[c_BufferSize]; 
        return buffer;
    }
}