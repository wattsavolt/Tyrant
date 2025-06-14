#pragma once

#include "Base/base.h"
#include "EngineMacros.h"
#include "Reflection/PrivateReflection.h"

namespace tyr
{
	class TYR_ENGINE_EXPORT AssetPath final
	{
	public:
		static constexpr uint c_MaxAssetPath = 130;
		// Include null-terminated character
		static constexpr uint c_MaxAssetPathDataSize = c_MaxAssetPath + 1;

		AssetPath();
		AssetPath(const char* data);
		~AssetPath();
		AssetPath& operator=(const char* data);
		bool operator==(const AssetPath& val);
		bool operator==(const char* data);

		operator const char* () const { return m_Data; }

		size_t Size() const { return m_Size; }

	private:
		TYR_REFL_PRIVATE_ACCESS(AssetPath);

		void CreateEmpty();
		void Copy(const char* data);
		
		

		uint m_Size;
		char m_Data[c_MaxAssetPathDataSize];
	};
	
}