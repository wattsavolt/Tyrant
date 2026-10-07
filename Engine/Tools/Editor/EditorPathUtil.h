#pragma once

#include "EditorMacros.h"
#include "Core.h"

namespace tyr
{
	// Helpers for the "Folder/Subfolder/Item" paths shown as trees in the editor's panels.
	class TYR_EDITOR_EXPORT EditorPathUtil final
	{
	public:
		// Orders paths with '/' below every other character, so a folder sorts directly before
		// its own subfolders and items.
		static bool PathLess(const char* a, const char* b);

		// True when path is directly inside the folder rather than in one of its subfolders. An
		// empty folder path is the root.
		static bool IsDirectlyInFolder(const char* path, const char* folderPath, size_t folderLength);

		// True when folderPath is anywhere inside parentPath.
		static bool IsSubfolder(const char* folderPath, const char* parentPath);

		// 1 for a top level folder.
		static uint GetFolderDepth(const char* folderPath);

		static const char* GetLastPathPart(const char* path);
	};
}
