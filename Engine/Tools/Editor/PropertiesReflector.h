#pragma once

#include "EditorMacros.h"
#include "Core.h"
#include "Reflection/Reflection.h"
#include "Math/Quaternion.h"

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

		PropertiesReflector();

		// Gives a type its own widget, like a Vector3's coloured X, Y and Z boxes.
		void RegisterCustomReflector(const Id64& typeID, CustomReflectFunction function);

		// Draws a row for each visible field. Returns true when any was changed.
		bool ReflectObject(void* object, const TypeInfo& typeInfo);

		template<typename T>
		bool Reflect(const char* label, T& value)
		{
			return ReflectValue(label, &value, GetTypeID<T>(), GetBuiltInCustomObjectPropertiesReflector<T>());
		}

		// Forgets the rotation angles being shown, such as when the selection changes.
		void ResetRotationCache() { m_RotationCache.Clear(); }

	private:
		// Rotations are shown as angles about each axis. The angles are kept between frames, since
		// working them out again from the quaternion can give different but equal angles.
		struct RotationCacheEntry
		{
			uint widgetID;
			Quaternion rotation;
			Vector3 degrees;
		};

		bool ReflectField(uint8* object, const Field& field);
		bool ReflectValue(const char* label, void* data, const Id64& typeID, const CustomObjectPropertiesReflector* customReflector);
		bool ReflectArray(const char* label, void* data, const ArrayPropertiesReflector& reflector);
		bool ReflectString(const char* label, void* data, const StringPropertiesReflector& reflector);
		bool ReflectCArray(const char* label, uint8* data, uint count, const Id64& elementTypeID);

		static bool ReflectVector3(PropertiesReflector& reflector, const char* label, void* data);
		static bool ReflectQuaternion(PropertiesReflector& reflector, const char* label, void* data);
		static bool ReflectAssetID(PropertiesReflector& reflector, const char* label, void* data);

		// A row with the label on the left. The value's widget goes on the right, after this.
		static void BeginValueRow(const char* label);
		// A row with an expandable label for nested values. Returns true when expanded, and
		// must then be followed by EndGroupRow.
		static bool BeginGroupRow(const char* label);
		static void EndGroupRow();

		static constexpr uint c_MaxRotationCacheEntries = 8;

		HashMap<Id64, CustomReflectFunction> m_CustomReflectors;
		LocalArray<RotationCacheEntry, c_MaxRotationCacheEntries> m_RotationCache;
	};
}
