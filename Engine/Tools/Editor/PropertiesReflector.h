#pragma once

#include "EditorMacros.h"
#include "Core.h"
#include "Reflection/Reflection.h"
#include "Math/Quaternion.h"
#include "AssetPicker.h"

namespace tyr
{
	struct TypeInfo;
	struct Field;

	// Draws an editable widget for each reflected field of an object, chosen by the field's type,
	// in Unreal's two column details layout. Must be used inside a two column ImGui table.
	class TYR_EDITOR_EXPORT PropertiesReflector final
	{
	public:
		// Draws the value at data with its own widget. Returns true when it was changed.
		using CustomReflectFunction = bool (*)(PropertiesReflector& reflector, const char* label, void* data);
		// Draws one field of an object, which it can read the rest of. defaults is the whole default
		// object or null. Returns true when the field was changed.
		using FieldReflectFunction = bool (*)(PropertiesReflector& reflector, const Field& field, uint8* object, const uint8* defaults, void* userData);

		PropertiesReflector();

		// Gives a type its own widget, like a Vector3's coloured X, Y and Z boxes.
		void RegisterCustomReflector(const Id64& typeID, CustomReflectFunction function);

		// Draws one field of a type with its own function, such as one that needs other fields.
		void RegisterFieldReflector(const Id64& objectTypeID, const Id32& fieldKey, FieldReflectFunction function, void* userData);

		// Draws a row for each visible field. Returns true when any was changed. When defaults is
		// given, values that differ from it are marked and can be reset to it.
		bool ReflectObject(void* object, const TypeInfo& typeInfo, const void* defaults = nullptr);

		template<typename T>
		bool Reflect(const char* label, T& value)
		{
			return ReflectValue(label, &value, GetTypeID<T>(), GetBuiltInCustomObjectPropertiesReflector<T>(), nullptr);
		}

		// Forgets the rotation angles being shown, such as when the selection changes.
		void ResetRotationCache() { m_RotationCache.Clear(); }

		// A row with the label on the left. The value's widget goes on the right, after this.
		void BeginValueRow(const char* label);
		// A row with an expandable label for nested values. Returns true when expanded, and
		// must then be followed by EndGroupRow.
		bool BeginGroupRow(const char* label);
		static void EndGroupRow();
		// Marks the next row's label as changed from its default, with a reset option on it.
		void SetNextRowOverridden(bool overridden) { m_RowOverridden = overridden; }
		// True once after the reset option of a marked row was picked.
		bool TakeResetRequest();

		// Shared by every asset field, since only one asset list is open at a time.
		AssetPicker& GetAssetPicker() { return m_AssetPicker; }

	private:
		// Rotations are shown as angles about each axis. The angles are kept between frames, since
		// working them out again from the quaternion can give different but equal angles.
		struct RotationCacheEntry
		{
			uint widgetID;
			Quaternion rotation;
			Vector3 degrees;
		};

		bool ReflectField(uint8* object, const Field& field, const uint8* defaults);
		bool ReflectValue(const char* label, void* data, const Id64& typeID, const CustomObjectPropertiesReflector* customReflector, const void* defaults);
		// A reflected struct shown as its own fields, rather than one value.
		bool IsGroup(const Field& field) const;
		bool ReflectArray(const char* label, void* data, const ArrayPropertiesReflector& reflector);
		bool ReflectString(const char* label, void* data, const StringPropertiesReflector& reflector);
		bool ReflectCArray(const char* label, uint8* data, uint count, const Id64& elementTypeID);

		static bool ReflectVector3(PropertiesReflector& reflector, const char* label, void* data);
		static bool ReflectQuaternion(PropertiesReflector& reflector, const char* label, void* data);
		static bool ReflectAssetID(PropertiesReflector& reflector, const char* label, void* data);

		// Draws the row's label, marked with a reset menu if its value was overridden.
		bool DrawRowLabel(const char* label, int treeNodeFlags);

		struct FieldReflectorEntry
		{
			Id64 objectTypeID;
			Id32 fieldKey;
			FieldReflectFunction function;
			void* userData;
		};

		static constexpr uint c_MaxRotationCacheEntries = 8;
		static constexpr uint c_MaxFieldReflectors = 8;

		HashMap<Id64, CustomReflectFunction> m_CustomReflectors;
		LocalArray<FieldReflectorEntry, c_MaxFieldReflectors> m_FieldReflectors;
		AssetPicker m_AssetPicker;
		LocalArray<RotationCacheEntry, c_MaxRotationCacheEntries> m_RotationCache;
		// Whether the next row's value differs from its default, used up by that row's label.
		bool m_RowOverridden = false;
		bool m_ResetRequested = false;
	};
}
