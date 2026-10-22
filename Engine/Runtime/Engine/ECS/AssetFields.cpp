#include "AssetFields.h"
#include "Containers/LocalArray.h"

namespace tyr
{
	namespace
	{
		// Made on first use, as games add to it while their libraries load.
		LocalArray<AssetField, AssetFields::c_MaxAssetFields>& GetFields()
		{
			static LocalArray<AssetField, AssetFields::c_MaxAssetFields> s_Fields;
			return s_Fields;
		}
	}

	namespace AssetFields
	{
		void Add(const Id64& componentType, const Id32& fieldKey, const char* extension)
		{
			LocalArray<AssetField, c_MaxAssetFields>& fields = GetFields();
			TYR_ASSERT(fields.Size() < c_MaxAssetFields);
			if (fields.Size() < c_MaxAssetFields)
			{
				fields.Add(AssetField{ componentType, fieldKey, extension });
			}
		}

		const AssetField* GetAll(uint& outCount)
		{
			outCount = GetFields().Size();
			return GetFields().Data();
		}
	}
}
