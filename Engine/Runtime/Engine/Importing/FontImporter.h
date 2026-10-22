#pragma once

#include "EngineMacros.h"
#include "Core.h"
#include "AssetSystem/AssetID.h"

namespace tyr
{
	namespace FontImporter
	{
		// Imports a .ttf or .otf file as a font asset, outputFolder/name.font under the assets folder.
		// Importing to the same place again keeps its asset ID.
		TYR_ENGINE_API bool ImportFont(const char* sourcePath, const char* outputFolder, const char* name, AssetID& outFontID);
	}
}
