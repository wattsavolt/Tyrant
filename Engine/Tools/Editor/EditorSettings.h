#pragma once

#include "EditorMacros.h"
#include "Config/Config.h"

namespace tyr
{
	// Editor choices kept in EditorConfig.ini between sessions. The one owner of that file, so
	// changes are never lost to another copy of it being saved.
	class TYR_EDITOR_EXPORT EditorSettings final
	{
	public:
		EditorSettings();

		// The level the editor opens on startup, relative to the assets folder. False if none is set.
		bool GetDefaultLevel(AssetPath& outLevelPath) const;
		void SetDefaultLevel(const char* levelPath);

		// Whether SourceAssets should be imported on startup. True when it's never been set.
		bool GetImportDefaultAssets() const;
		void SetImportDefaultAssets(bool import);

	private:
		Config m_Config;
	};
}
