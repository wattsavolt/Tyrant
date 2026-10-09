#include "PropertiesReflector.h"
#include "AssetBrowserPanel.h"
#include "AssetSystem/AssetRegistry.h"
#include "AssetSystem/AssetUtil.h"
#include "Utility/PathUtil.h"
#include "Math/Math.h"
#include "imgui.h"
#include <cfloat>
#include <cstdio>
#include <cstring>

namespace tyr
{
	namespace
	{
		constexpr uint c_MaxStringFieldSize = 512;
		constexpr float c_AxisBarWidth = 3.0f;
		const ImU32 c_AxisColours[3] = { IM_COL32(220, 60, 60, 255), IM_COL32(90, 200, 70, 255), IM_COL32(60, 110, 235, 255) };
		// Labels of values changed from the actor type's default.
		const ImU32 c_OverriddenColour = IM_COL32(255, 196, 70, 255);

		// The ImGui number type for a base type, or false if it isn't one.
		bool GetNumberType(const Id64& typeID, ImGuiDataType& dataType)
		{
			struct NumberType
			{
				Id64 typeID;
				ImGuiDataType dataType;
			};
			static const NumberType c_NumberTypes[] =
			{
				{ GetTypeID<int8>(), ImGuiDataType_S8 },
				{ GetTypeID<uint8>(), ImGuiDataType_U8 },
				{ GetTypeID<int16>(), ImGuiDataType_S16 },
				{ GetTypeID<uint16>(), ImGuiDataType_U16 },
				{ GetTypeID<int>(), ImGuiDataType_S32 },
				{ GetTypeID<uint>(), ImGuiDataType_U32 },
				{ GetTypeID<int64>(), ImGuiDataType_S64 },
				{ GetTypeID<uint64>(), ImGuiDataType_U64 },
				{ GetTypeID<float>(), ImGuiDataType_Float },
				{ GetTypeID<double>(), ImGuiDataType_Double }
			};

			for (const NumberType& numberType : c_NumberTypes)
			{
				if (numberType.typeID == typeID)
				{
					dataType = numberType.dataType;
					return true;
				}
			}
			return false;
		}

		// Three number boxes side by side, each marked with its axis' colour like Unreal's.
		bool DrawAxisFloats(float* values, float speed, const char* format)
		{
			const float spacing = ImGui::GetStyle().ItemInnerSpacing.x;
			const float width = (ImGui::GetContentRegionAvail().x - spacing * 2.0f) / 3.0f;

			bool changed = false;
			for (uint i = 0; i < 3; ++i)
			{
				if (i > 0)
				{
					ImGui::SameLine(0.0f, spacing);
				}
				ImGui::PushID(static_cast<int>(i));
				ImGui::SetNextItemWidth(width);
				changed |= ImGui::DragFloat("##axis", &values[i], speed, 0.0f, 0.0f, format);
				const ImVec2 min = ImGui::GetItemRectMin();
				const ImVec2 max = ImGui::GetItemRectMax();
				ImGui::GetWindowDrawList()->AddRectFilled(min, ImVec2(min.x + c_AxisBarWidth, max.y), c_AxisColours[i]);
				ImGui::PopID();
			}
			return changed;
		}
	}

	PropertiesReflector::PropertiesReflector()
	{
		RegisterCustomReflector(GetTypeID<Vector3>(), &ReflectVector3);
		RegisterCustomReflector(GetTypeID<Quaternion>(), &ReflectQuaternion);
		RegisterCustomReflector(GetTypeID<AssetID>(), &ReflectAssetID);
	}

	void PropertiesReflector::RegisterCustomReflector(const Id64& typeID, CustomReflectFunction function)
	{
		m_CustomReflectors[typeID] = function;
	}

	void PropertiesReflector::RegisterFieldReflector(const Id64& objectTypeID, const Id32& fieldKey, FieldReflectFunction function, void* userData)
	{
		m_FieldReflectors.Add({ objectTypeID, fieldKey, function, userData });
	}

	bool PropertiesReflector::TakeResetRequest()
	{
		const bool requested = m_ResetRequested;
		m_ResetRequested = false;
		return requested;
	}

	bool PropertiesReflector::ReflectObject(void* object, const TypeInfo& typeInfo, const void* defaults)
	{
		// The same ID the type was registered under, only worked out when a field reflector could match.
		const Id64 objectTypeID = m_FieldReflectors.IsEmpty() ? Id64() : Id64(typeInfo.name.CStr());

		bool changed = false;
		for (uint i = 0; i < typeInfo.fieldCount; ++i)
		{
			const Field& field = typeInfo.fields[i];
			if (!field.isVisible)
			{
				continue;
			}

			ImGui::PushID(field.name);
			ImGui::BeginDisabled(!field.isEditable);
			// Values that can't be edited can't be overridden either.
			const uint8* fieldDefaults = field.isEditable ? static_cast<const uint8*>(defaults) : nullptr;
			const FieldReflectorEntry* fieldReflector = nullptr;
			for (const FieldReflectorEntry& entry : m_FieldReflectors)
			{
				if (entry.objectTypeID == objectTypeID && entry.fieldKey == field.key)
				{
					fieldReflector = &entry;
					break;
				}
			}
			if (fieldReflector)
			{
				m_ResetRequested = false;
				changed |= fieldReflector->function(*this, field, static_cast<uint8*>(object), fieldDefaults, fieldReflector->userData);
				m_RowOverridden = false;
			}
			else
			{
				changed |= ReflectField(static_cast<uint8*>(object), field, fieldDefaults);
			}
			ImGui::EndDisabled();
			ImGui::PopID();
		}
		return changed;
	}

	bool PropertiesReflector::IsGroup(const Field& field) const
	{
		if (field.customPropertiesReflector || field.isCArray || m_CustomReflectors.Find(field.typeID))
		{
			return false;
		}
		const TypeInfo* typeInfo = TypeRegistry::Instance().FindType(field.typeID);
		return typeInfo && typeInfo->fieldCount > 0;
	}

	bool PropertiesReflector::ReflectField(uint8* object, const Field& field, const uint8* defaults)
	{
		uint8* data = object + field.dataOffset;
		if (field.isCArray)
		{
			uint count;
			memcpy(&count, object + field.countOffset, sizeof(uint));
			return ReflectCArray(field.name, data, count, field.typeID);
		}

		// A struct's own fields are marked individually, so only whole values are marked here.
		const uint8* fieldDefault = defaults ? defaults + field.dataOffset : nullptr;
		if (fieldDefault && !IsGroup(field))
		{
			m_RowOverridden = field.customPropertiesReflector
				? !field.customPropertiesReflector->Equals(data, fieldDefault)
				: memcmp(data, fieldDefault, field.size) != 0;
		}

		m_ResetRequested = false;
		bool changed = ReflectValue(field.name, data, field.typeID, field.customPropertiesReflector, fieldDefault);
		if (m_ResetRequested && fieldDefault)
		{
			memcpy(data, fieldDefault, field.size);
			changed = true;
		}
		m_ResetRequested = false;
		m_RowOverridden = false;
		return changed;
	}

	bool PropertiesReflector::ReflectValue(const char* label, void* data, const Id64& typeID, const CustomObjectPropertiesReflector* customReflector, const void* defaults)
	{
		if (const CustomReflectFunction* function = m_CustomReflectors.Find(typeID))
		{
			return (*function)(*this, label, data);
		}

		if (customReflector)
		{
			switch (customReflector->GetType())
			{
			case CustomObjectPropertiesReflector::Type::LocalArray:
				return ReflectArray(label, data, static_cast<const ArrayPropertiesReflector&>(*customReflector));
			case CustomObjectPropertiesReflector::Type::LocalString:
				return ReflectString(label, data, static_cast<const StringPropertiesReflector&>(*customReflector));
			}
		}

		if (typeID == GetTypeID<bool>())
		{
			BeginValueRow(label);
			return ImGui::Checkbox("##value", static_cast<bool*>(data));
		}

		// Whole numbers have no decimal places.
		ImGuiDataType dataType;
		if (GetNumberType(typeID, dataType))
		{
			BeginValueRow(label);
			const bool isDecimal = dataType == ImGuiDataType_Float || dataType == ImGuiDataType_Double;
			return ImGui::DragScalar("##value", dataType, data, isDecimal ? 0.01f : 1.0f, nullptr, nullptr, isDecimal ? "%.3f" : nullptr);
		}

		// A reflected struct shows its own fields underneath.
		const TypeInfo* typeInfo = TypeRegistry::Instance().FindType(typeID);
		if (typeInfo && typeInfo->fieldCount > 0)
		{
			bool changed = false;
			if (BeginGroupRow(label))
			{
				changed = ReflectObject(data, *typeInfo, defaults);
				EndGroupRow();
			}
			return changed;
		}

		BeginValueRow(label);
		ImGui::TextDisabled("Can't be edited");
		return false;
	}

	bool PropertiesReflector::ReflectArray(const char* label, void* data, const ArrayPropertiesReflector& reflector)
	{
		const uint size = reflector.GetSize(data);
		const bool open = BeginGroupRow(label);

		// The element count, with buttons to add one to the end or take the last one off.
		bool changed = false;
		ImGui::AlignTextToFramePadding();
		ImGui::Text("%u elements", size);
		ImGui::SameLine();
		ImGui::BeginDisabled(size >= reflector.GetCapacity());
		if (ImGui::SmallButton("+"))
		{
			reflector.Resize(data, size + 1);
			changed = true;
		}
		ImGui::EndDisabled();
		ImGui::SameLine();
		ImGui::BeginDisabled(size == 0);
		if (ImGui::SmallButton("-"))
		{
			reflector.Resize(data, size - 1);
			changed = true;
		}
		ImGui::EndDisabled();

		if (open)
		{
			const uint newSize = reflector.GetSize(data);
			for (uint i = 0; i < newSize; ++i)
			{
				char elementLabel[24];
				snprintf(elementLabel, sizeof(elementLabel), "Index %u", i);
				ImGui::PushID(static_cast<int>(i));
				changed |= ReflectValue(elementLabel, reflector.GetElement(data, i), reflector.GetElementTypeID(), reflector.GetElementReflector(), nullptr);
				ImGui::PopID();
			}
			EndGroupRow();
		}
		return changed;
	}

	bool PropertiesReflector::ReflectString(const char* label, void* data, const StringPropertiesReflector& reflector)
	{
		BeginValueRow(label);

		const uint bufferSize = reflector.GetCapacity() + 1;
		TYR_ASSERT(bufferSize <= c_MaxStringFieldSize);
		char buffer[c_MaxStringFieldSize];
		strcpy_s(buffer, bufferSize, reflector.GetString(data));
		if (ImGui::InputText("##value", buffer, bufferSize))
		{
			reflector.SetString(data, buffer);
			return true;
		}
		return false;
	}

	bool PropertiesReflector::ReflectCArray(const char* label, uint8* data, uint count, const Id64& elementTypeID)
	{
		const TypeInfo* elementType = TypeRegistry::Instance().FindType(elementTypeID);
		const bool open = BeginGroupRow(label);
		ImGui::AlignTextToFramePadding();
		ImGui::Text("%u elements", count);
		if (!open)
		{
			return false;
		}

		bool changed = false;
		if (elementType)
		{
			for (uint i = 0; i < count; ++i)
			{
				char elementLabel[24];
				snprintf(elementLabel, sizeof(elementLabel), "Index %u", i);
				ImGui::PushID(static_cast<int>(i));
				changed |= ReflectValue(elementLabel, data + i * elementType->size, elementTypeID, nullptr, nullptr);
				ImGui::PopID();
			}
		}
		EndGroupRow();
		return changed;
	}

	bool PropertiesReflector::ReflectVector3(PropertiesReflector& reflector, const char* label, void* data)
	{
		reflector.BeginValueRow(label);
		Vector3& vector = *static_cast<Vector3*>(data);
		return DrawAxisFloats(&vector.x, 0.01f, "%.3f");
	}

	bool PropertiesReflector::ReflectQuaternion(PropertiesReflector& reflector, const char* label, void* data)
	{
		reflector.BeginValueRow(label);
		Quaternion& rotation = *static_cast<Quaternion*>(data);

		// Unique to this field wherever it's drawn.
		const uint widgetID = ImGui::GetID("##rotation");
		RotationCacheEntry* entry = nullptr;
		for (RotationCacheEntry& cached : reflector.m_RotationCache)
		{
			if (cached.widgetID == widgetID)
			{
				entry = &cached;
				break;
			}
		}
		if (!entry)
		{
			// The oldest entry makes way once it's full.
			if (reflector.m_RotationCache.Size() == c_MaxRotationCacheEntries)
			{
				reflector.m_RotationCache.Erase(0);
			}
			entry = &reflector.m_RotationCache.ExpandOne();
			entry->widgetID = widgetID;
			entry->rotation = Quaternion(0.0f, 0.0f, 0.0f, 0.0f);
		}

		// Worked out again only when something else changed the rotation.
		if (!(entry->rotation == rotation))
		{
			Vector3 radians;
			rotation.ToEulerAngles(radians.x, radians.y, radians.z);
			entry->rotation = rotation;
			entry->degrees = radians * Math::c_RadToDeg;
		}

		Vector3 degrees = entry->degrees;
		if (!DrawAxisFloats(&degrees.x, 0.5f, "%.2f"))
		{
			return false;
		}

		const Vector3 radians = degrees * Math::c_DegToRad;
		rotation.FromEulerAngles(radians.x, radians.y, radians.z);
		entry->rotation = rotation;
		entry->degrees = degrees;
		return true;
	}

	bool PropertiesReflector::ReflectAssetID(PropertiesReflector& reflector, const char* label, void* data)
	{
		reflector.BeginValueRow(label);
		AssetID& assetID = *static_cast<AssetID*>(data);

		// Any kind of asset, since the field's type doesn't say which kind it wants.
		const float clearWidth = ImGui::GetFrameHeight();
		const float spacing = ImGui::GetStyle().ItemInnerSpacing.x;
		bool changed = reflector.m_AssetPicker.Draw(assetID, nullptr, ImGui::GetContentRegionAvail().x - clearWidth - spacing);

		ImGui::SameLine(0.0f, spacing);
		if (ImGui::Button("X##clear", ImVec2(clearWidth, 0.0f)))
		{
			assetID = {};
			changed = true;
		}
		ImGui::SetItemTooltip("Clear");
		return changed;
	}

	void PropertiesReflector::BeginValueRow(const char* label)
	{
		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
		ImGui::AlignTextToFramePadding();
		DrawRowLabel(label, ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_SpanFullWidth);
		ImGui::TableSetColumnIndex(1);
		ImGui::SetNextItemWidth(-FLT_MIN);
	}

	bool PropertiesReflector::BeginGroupRow(const char* label)
	{
		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
		ImGui::AlignTextToFramePadding();
		const bool open = DrawRowLabel(label, ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_DefaultOpen);
		ImGui::TableSetColumnIndex(1);
		return open;
	}

	bool PropertiesReflector::DrawRowLabel(const char* label, int treeNodeFlags)
	{
		const bool overridden = m_RowOverridden;
		m_RowOverridden = false;
		if (!overridden)
		{
			return ImGui::TreeNodeEx(label, treeNodeFlags);
		}

		ImGui::PushStyleColor(ImGuiCol_Text, c_OverriddenColour);
		const bool open = ImGui::TreeNodeEx(label, treeNodeFlags);
		ImGui::PopStyleColor();
		ImGui::SetItemTooltip("Changed from the actor type's default. Right-click to reset it.");
		if (ImGui::BeginPopupContextItem("##ResetMenu"))
		{
			if (ImGui::MenuItem("Reset to Default"))
			{
				m_ResetRequested = true;
			}
			ImGui::EndPopup();
		}
		return open;
	}

	void PropertiesReflector::EndGroupRow()
	{
		ImGui::TreePop();
	}
}
