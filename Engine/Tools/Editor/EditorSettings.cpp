#include "EditorSettings.h"
#include "AssetSystem/AssetUtil.h"

namespace tyr
{
	namespace
	{
		constexpr const char* c_DefaultLevelKey = "DefaultLevel";
		constexpr const char* c_ImportDefaultAssetsKey = "ImportDefaultAssets";

		Path GetConfigPath()
		{
			char configPath[TYR_MAX_PATH_TOTAL_SIZE];
			AssetUtil::CreateFullConfigPath(configPath, "EditorConfig.ini");
			return Path(configPath);
		}
	}

	EditorSettings::EditorSettings()
		: m_Config(GetConfigPath().CStr())
	{
	}

	bool EditorSettings::GetDefaultLevel(AssetPath& outLevelPath) const
	{
		if (!m_Config.HasValue(c_DefaultLevelKey))
		{
			return false;
		}
		outLevelPath = m_Config.GetValue(c_DefaultLevelKey).CStr();
		return outLevelPath.Size() > 0;
	}

	void EditorSettings::SetDefaultLevel(const char* levelPath)
	{
		m_Config.SetValue(c_DefaultLevelKey, levelPath);
		m_Config.Save();
	}

	bool EditorSettings::GetImportDefaultAssets() const
	{
		return !m_Config.HasValue(c_ImportDefaultAssetsKey) || m_Config.GetValueAsBool(c_ImportDefaultAssetsKey);
	}

	void EditorSettings::SetImportDefaultAssets(bool import)
	{
		m_Config.SetValueAsBool(c_ImportDefaultAssetsKey, import);
		m_Config.Save();
	}
}
