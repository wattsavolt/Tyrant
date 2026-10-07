#include "EditorPathUtil.h"
#include <cstring>

namespace tyr
{
	bool EditorPathUtil::PathLess(const char* a, const char* b)
	{
		for (;; ++a, ++b)
		{
			const uint8 charA = *a == '/' ? 1 : static_cast<uint8>(*a);
			const uint8 charB = *b == '/' ? 1 : static_cast<uint8>(*b);
			if (charA != charB)
			{
				return charA < charB;
			}
			if (charA == 0)
			{
				return false;
			}
		}
	}

	bool EditorPathUtil::IsDirectlyInFolder(const char* path, const char* folderPath, size_t folderLength)
	{
		if (folderLength > 0)
		{
			if (strncmp(path, folderPath, folderLength) != 0 || path[folderLength] != '/')
			{
				return false;
			}
			path += folderLength + 1;
		}
		return strchr(path, '/') == nullptr;
	}

	bool EditorPathUtil::IsSubfolder(const char* folderPath, const char* parentPath)
	{
		const size_t parentLength = strlen(parentPath);
		return strncmp(folderPath, parentPath, parentLength) == 0 && folderPath[parentLength] == '/';
	}

	uint EditorPathUtil::GetFolderDepth(const char* folderPath)
	{
		uint depth = 1;
		for (const char* c = folderPath; *c != '\0'; ++c)
		{
			if (*c == '/')
			{
				++depth;
			}
		}
		return depth;
	}

	const char* EditorPathUtil::GetLastPathPart(const char* path)
	{
		const char* lastSlash = strrchr(path, '/');
		return lastSlash ? lastSlash + 1 : path;
	}
}
