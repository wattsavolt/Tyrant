#pragma once

#include "EditorMacros.h"
#include "Core.h"
#include "AssetSystem/AssetID.h"
#include "String/Path.h"

namespace tyr
{
	// A button showing an asset's name that opens a searchable list of assets to pick from, and
	// takes assets dragged from the Asset Browser. One list is open at a time, so one picker can
	// draw every asset field.
	class TYR_EDITOR_EXPORT AssetPicker final
	{
	public:
		// Lists only assets with extension, or every asset when it's null. Returns true when asset
		// was changed. dimmed shows the name greyed out, such as for a value that isn't overridden.
		bool Draw(AssetID& asset, const char* extension, float width, bool dimmed = false);

	private:
		struct Entry
		{
			AssetID assetID;
			char name[PathConstants::c_MaxFileNameTotalSize];
		};

		// Every asset with the extension, sorted by name, built when the list opens.
		void RebuildEntries(const char* extension);
		bool DrawList(AssetID& asset, const char* extension, float width);

		// Reused each time the list opens, rather than allocated.
		Array<Entry> m_Entries;
		uint m_RegistryVersion = ~0u;
		char m_Search[PathConstants::c_MaxFileNameTotalSize] = {};
		bool m_FocusSearch = false;
	};
}
