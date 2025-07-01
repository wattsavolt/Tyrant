#pragma once

#include "Base/base.h"
#include "EngineMacros.h"
#include "Reflection/PrivateReflection.h"

namespace tyr
{
	// Class with fixed buffer designed to store asset file paths relative to the asset folder
	class TYR_ENGINE_EXPORT AssetPath final
	{
	public:
		static constexpr uint c_MaxAssetName = 63;
		// Include null-terminated character
		static constexpr uint c_MaxAssetNameStrSize = c_MaxAssetName + 1;

		static constexpr uint c_MaxAssetPath = 127;
		// Include null-terminated character
		static constexpr uint c_MaxAssetPathStrSize = c_MaxAssetPath + 1;

		AssetPath();
		AssetPath(const char* data);
		AssetPath(const AssetPath& other);
		~AssetPath();
		AssetPath& operator=(const char* data);
		AssetPath& operator=(const AssetPath& other);
		bool operator==(const AssetPath& val);
		bool operator==(const char* data);

		operator const char* () const { return m_Data; }

		size_t Size() const { return m_Size - 1; }

	private:
		TYR_REFL_PRIVATE_ACCESS(AssetPath);

		void CreateEmpty();
		void Copy(const char* data);
		
		uint m_Size;
		char m_Data[c_MaxAssetPathStrSize];
	};
	
}