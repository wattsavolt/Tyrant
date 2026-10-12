#include "McpReflection.h"
#include "McpJson.h"
#include "Reflection/TypeRegistry.h"
#include "Reflection/CustomObjectPropertiesReflector.h"
#include "AssetSystem/AssetID.h"
#include "AssetSystem/AssetRegistry.h"
#include "AssetSystem/AssetUtil.h"
#include "AssetSystem/AssetConstants.h"
#include "Math/Vector2.h"
#include "Math/Vector3.h"
#include "Math/Vector4.h"
#include "Math/Quaternion.h"
#include "Math/Math.h"
#include <cstdio>
#include <cstring>

namespace tyr
{
	namespace McpReflection
	{
		namespace
		{
			using namespace simdjson;

			void WriteValue(JsonWriter& writer, const void* data, const Id64& typeID, const CustomObjectPropertiesReflector* reflector);
			bool ReadValue(const dom::element& value, void* data, const Id64& typeID, const CustomObjectPropertiesReflector* reflector, const char* name, char* error, size_t errorSize);

			template<typename T>
			bool WriteNumber(JsonWriter& writer, const void* data, const Id64& typeID)
			{
				if (typeID != GetTypeID<T>())
				{
					return false;
				}
				T value;
				memcpy(&value, data, sizeof(T));
				if constexpr (std::is_floating_point_v<T>)
				{
					writer.Number(static_cast<double>(value));
				}
				else if constexpr (std::is_signed_v<T>)
				{
					writer.Int(static_cast<int64>(value));
				}
				else
				{
					writer.UInt(static_cast<uint64>(value));
				}
				return true;
			}

			// False if typeID isn't T. Otherwise sets handled, and fails if the value isn't a number.
			template<typename T>
			bool ReadNumber(const dom::element& value, void* data, const Id64& typeID, bool& handled)
			{
				if (typeID != GetTypeID<T>())
				{
					return false;
				}
				handled = true;
				double number;
				if (value.get_double().get(number))
				{
					return false;
				}
				const T converted = static_cast<T>(number);
				memcpy(data, &converted, sizeof(T));
				return true;
			}

			bool ReadFloats(const dom::element& value, float* out, uint count)
			{
				dom::array array;
				if (value.get_array().get(array) || array.size() != count)
				{
					return false;
				}
				uint i = 0;
				for (dom::element element : array)
				{
					double number;
					if (element.get_double().get(number))
					{
						return false;
					}
					out[i++] = static_cast<float>(number);
				}
				return true;
			}

			const TypeInfo* FindStruct(const Id64& typeID)
			{
				const TypeInfo* typeInfo = TypeRegistry::Instance().FindType(typeID);
				return typeInfo && typeInfo->fieldCount > 0 ? typeInfo : nullptr;
			}

			void WriteFloats(JsonWriter& writer, const float* values, uint count)
			{
				writer.BeginArray();
				for (uint i = 0; i < count; ++i)
				{
					writer.Number(values[i]);
				}
				writer.EndArray();
			}

			void WriteValue(JsonWriter& writer, const void* data, const Id64& typeID, const CustomObjectPropertiesReflector* reflector)
			{
				if (typeID == GetTypeID<Vector2>())
				{
					WriteFloats(writer, &static_cast<const Vector2*>(data)->x, 2);
					return;
				}
				if (typeID == GetTypeID<Vector3>())
				{
					WriteFloats(writer, &static_cast<const Vector3*>(data)->x, 3);
					return;
				}
				if (typeID == GetTypeID<Vector4>())
				{
					WriteFloats(writer, &static_cast<const Vector4*>(data)->x, 4);
					return;
				}
				if (typeID == GetTypeID<Quaternion>())
				{
					const Quaternion& rotation = *static_cast<const Quaternion*>(data);
					writer.BeginArray();
					writer.Number(rotation.x);
					writer.Number(rotation.y);
					writer.Number(rotation.z);
					writer.Number(rotation.w);
					writer.EndArray();
					return;
				}
				if (typeID == GetTypeID<AssetID>())
				{
					const AssetID& id = *static_cast<const AssetID*>(data);
					const AssetRegistry& registry = AssetRegistry::Instance();
					if (!AssetUtil::IsValidAssetID(id))
					{
						writer.Null();
					}
					else if (registry.GetAssets().Contains(id))
					{
						writer.String(registry.GetAssetData(id).filePath.CStr());
					}
					else
					{
						char missing[32];
						snprintf(missing, sizeof(missing), "missing:%016llx", static_cast<unsigned long long>(id.GetHash()));
						writer.String(missing);
					}
					return;
				}

				if (reflector && reflector->GetType() == CustomObjectPropertiesReflector::Type::LocalString)
				{
					writer.String(static_cast<const StringPropertiesReflector*>(reflector)->GetString(data));
					return;
				}
				if (reflector && reflector->GetType() == CustomObjectPropertiesReflector::Type::LocalArray)
				{
					const ArrayPropertiesReflector& arrayReflector = *static_cast<const ArrayPropertiesReflector*>(reflector);
					void* arrayData = const_cast<void*>(data);
					writer.BeginArray();
					for (uint i = 0; i < arrayReflector.GetSize(data); ++i)
					{
						WriteValue(writer, arrayReflector.GetElement(arrayData, i), arrayReflector.GetElementTypeID(), arrayReflector.GetElementReflector());
					}
					writer.EndArray();
					return;
				}

				if (typeID == GetTypeID<bool>())
				{
					writer.Bool(*static_cast<const bool*>(data));
					return;
				}
				if (WriteNumber<float>(writer, data, typeID) || WriteNumber<double>(writer, data, typeID)
					|| WriteNumber<int8>(writer, data, typeID) || WriteNumber<uint8>(writer, data, typeID)
					|| WriteNumber<int16>(writer, data, typeID) || WriteNumber<uint16>(writer, data, typeID)
					|| WriteNumber<int>(writer, data, typeID) || WriteNumber<uint>(writer, data, typeID)
					|| WriteNumber<int64>(writer, data, typeID) || WriteNumber<uint64>(writer, data, typeID))
				{
					return;
				}

				if (const TypeInfo* typeInfo = FindStruct(typeID))
				{
					WriteObject(writer, data, *typeInfo);
					return;
				}
				writer.Null();
			}

			bool Fail(char* error, size_t errorSize, const char* name, const char* expected)
			{
				snprintf(error, errorSize, "'%s' should be %s.", name, expected);
				return false;
			}

			bool ReadValue(const dom::element& value, void* data, const Id64& typeID, const CustomObjectPropertiesReflector* reflector, const char* name, char* error, size_t errorSize)
			{
				if (typeID == GetTypeID<Vector2>())
				{
					return ReadFloats(value, &static_cast<Vector2*>(data)->x, 2) || Fail(error, errorSize, name, "[x, y]");
				}
				if (typeID == GetTypeID<Vector3>())
				{
					return ReadFloats(value, &static_cast<Vector3*>(data)->x, 3) || Fail(error, errorSize, name, "[x, y, z]");
				}
				if (typeID == GetTypeID<Vector4>())
				{
					return ReadFloats(value, &static_cast<Vector4*>(data)->x, 4) || Fail(error, errorSize, name, "[x, y, z, w]");
				}
				if (typeID == GetTypeID<Quaternion>())
				{
					// Euler degrees are easier to write, a quaternion exact.
					float values[4];
					Quaternion& rotation = *static_cast<Quaternion*>(data);
					if (ReadFloats(value, values, 4))
					{
						rotation = Quaternion(values[3], values[0], values[1], values[2]);
						return true;
					}
					if (ReadFloats(value, values, 3))
					{
						rotation.FromEulerAngles(values[0] * Math::c_DegToRad, values[1] * Math::c_DegToRad, values[2] * Math::c_DegToRad);
						return true;
					}
					return Fail(error, errorSize, name, "[x, y, z, w] or Euler degrees [x, y, z]");
				}
				if (typeID == GetTypeID<AssetID>())
				{
					AssetID& id = *static_cast<AssetID*>(data);
					if (value.is_null())
					{
						id = AssetConstants::c_InvalidAssetID;
						return true;
					}
					std::string_view path;
					if (value.get_string().get(path))
					{
						return Fail(error, errorSize, name, "an asset path or null");
					}
					char pathText[PathConstants::c_MaxAssetPathTotalSize];
					snprintf(pathText, sizeof(pathText), "%.*s", static_cast<int>(path.size()), path.data());
					id = AssetRegistry::Instance().GetAssetIDSafe(pathText);
					if (!AssetUtil::IsValidAssetID(id) && pathText[0] != '\0')
					{
						snprintf(error, errorSize, "'%s': no asset at %s.", name, pathText);
						return false;
					}
					return true;
				}

				if (reflector && reflector->GetType() == CustomObjectPropertiesReflector::Type::LocalString)
				{
					const StringPropertiesReflector& stringReflector = *static_cast<const StringPropertiesReflector*>(reflector);
					std::string_view text;
					if (value.get_string().get(text))
					{
						return Fail(error, errorSize, name, "a string");
					}
					if (text.size() > stringReflector.GetCapacity())
					{
						snprintf(error, errorSize, "'%s' can't be longer than %u characters.", name, stringReflector.GetCapacity());
						return false;
					}
					char buffer[1024];
					snprintf(buffer, sizeof(buffer), "%.*s", static_cast<int>(text.size()), text.data());
					stringReflector.SetString(data, buffer);
					return true;
				}
				if (reflector && reflector->GetType() == CustomObjectPropertiesReflector::Type::LocalArray)
				{
					const ArrayPropertiesReflector& arrayReflector = *static_cast<const ArrayPropertiesReflector*>(reflector);
					dom::array array;
					if (value.get_array().get(array))
					{
						return Fail(error, errorSize, name, "an array");
					}
					if (array.size() > arrayReflector.GetCapacity())
					{
						snprintf(error, errorSize, "'%s' can't hold more than %u elements.", name, arrayReflector.GetCapacity());
						return false;
					}
					arrayReflector.Resize(data, static_cast<uint>(array.size()));
					uint i = 0;
					for (dom::element element : array)
					{
						if (!ReadValue(element, arrayReflector.GetElement(data, i++), arrayReflector.GetElementTypeID(), arrayReflector.GetElementReflector(), name, error, errorSize))
						{
							return false;
						}
					}
					return true;
				}

				if (typeID == GetTypeID<bool>())
				{
					bool flag;
					if (value.get_bool().get(flag))
					{
						return Fail(error, errorSize, name, "true or false");
					}
					*static_cast<bool*>(data) = flag;
					return true;
				}

				bool handled = false;
				const bool read = ReadNumber<float>(value, data, typeID, handled) || ReadNumber<double>(value, data, typeID, handled)
					|| ReadNumber<int8>(value, data, typeID, handled) || ReadNumber<uint8>(value, data, typeID, handled)
					|| ReadNumber<int16>(value, data, typeID, handled) || ReadNumber<uint16>(value, data, typeID, handled)
					|| ReadNumber<int>(value, data, typeID, handled) || ReadNumber<uint>(value, data, typeID, handled)
					|| ReadNumber<int64>(value, data, typeID, handled) || ReadNumber<uint64>(value, data, typeID, handled);
				if (handled)
				{
					return read || Fail(error, errorSize, name, "a number");
				}

				if (const TypeInfo* typeInfo = FindStruct(typeID))
				{
					dom::object object;
					if (value.get_object().get(object))
					{
						return Fail(error, errorSize, name, "an object");
					}
					return ReadObject(object, data, *typeInfo, error, errorSize);
				}

				snprintf(error, errorSize, "'%s' can't be set.", name);
				return false;
			}
		}

		void WriteObject(JsonWriter& writer, const void* object, const TypeInfo& typeInfo)
		{
			const uint8* bytes = static_cast<const uint8*>(object);
			writer.BeginObject();
			for (uint i = 0; i < typeInfo.fieldCount; ++i)
			{
				const Field& field = typeInfo.fields[i];
				if (!field.isVisible)
				{
					continue;
				}

				writer.Key(field.name);
				if (field.isCArray)
				{
					const TypeInfo* elementType = TypeRegistry::Instance().FindType(field.typeID);
					uint count;
					memcpy(&count, bytes + field.countOffset, sizeof(uint));
					writer.BeginArray();
					for (uint j = 0; elementType && j < count; ++j)
					{
						WriteValue(writer, bytes + field.dataOffset + j * elementType->size, field.typeID, nullptr);
					}
					writer.EndArray();
					continue;
				}
				WriteValue(writer, bytes + field.dataOffset, field.typeID, field.customPropertiesReflector);
			}
			writer.EndObject();
		}

		bool ReadObject(const simdjson::dom::object& values, void* object, const TypeInfo& typeInfo, char* error, size_t errorSize)
		{
			uint8* bytes = static_cast<uint8*>(object);
			for (simdjson::dom::key_value_pair member : values)
			{
				const Field* field = nullptr;
				for (uint i = 0; i < typeInfo.fieldCount; ++i)
				{
					const Field& candidate = typeInfo.fields[i];
					if (candidate.isVisible && member.key.size() == strlen(candidate.name) && member.key == candidate.name)
					{
						field = &candidate;
						break;
					}
				}

				if (!field)
				{
					snprintf(error, errorSize, "%s has no field '%.*s'.", typeInfo.name.CStr(), static_cast<int>(member.key.size()), member.key.data());
					return false;
				}
				if (!field->isEditable || field->isCArray)
				{
					snprintf(error, errorSize, "'%s' can't be edited.", field->name);
					return false;
				}
				if (!ReadValue(member.value, bytes + field->dataOffset, field->typeID, field->customPropertiesReflector, field->name, error, errorSize))
				{
					return false;
				}
			}
			return true;
		}
	}
}
