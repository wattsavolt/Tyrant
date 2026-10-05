#pragma once

#include "String/Path.h"

namespace tyr
{
	// Draws the File menu and handles its items.
	class FileMenu final
	{
	public:
		void Draw();

	private:
		void Import();
		void DrawImportOptionsWindow();

		bool m_ShowImportOptions = false;
		char m_PendingFilePath[PathConstants::c_MaxPathTotalSize]{};
		char m_PendingModelName[PathConstants::c_MaxFileNameTotalSize]{};
		char m_PendingOutputFolder[PathConstants::c_MaxRelativePathTotalSize]{};

		// Options carried over between imports in the same editor session, so the user doesn't
		// have to re-enter them every time.
		bool m_GenerateStaticMeshLods = true;
		int m_LodCount = 3;
		bool m_ForceLodRegeneration = false;
	};
}
