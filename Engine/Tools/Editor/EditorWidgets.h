#pragma once

#include "EditorMacros.h"
#include "Core.h"

namespace tyr
{
	// Small ImGui widgets shared by the editor's panels.
	class TYR_EDITOR_EXPORT EditorWidgets final
	{
	public:
		enum class RenameResult : uint8
		{
			Editing,
			Committed,
			Cancelled
		};

		// A text box for renaming something in place, filling the available width when width is
		// 0. Takes keyboard focus when focusPending is set, then clears it. Like Unreal, Enter or
		// clicking away keeps the new name and Escape drops it.
		static RenameResult DrawRenameBox(char* buffer, size_t bufferSize, bool& focusPending, float width = 0.0f);

		// Turns a type name into one for showing, like "StaticMeshActor" into "Static Mesh Actor".
		// "Component" is left out, so "MeshComponent" becomes "Mesh".
		static void MakeDisplayName(const char* typeName, char* displayName, size_t displayNameSize);

		// Whether search appears anywhere in text, ignoring case, so "ub" finds "Cube". An empty
		// search matches everything.
		static bool ContainsIgnoreCase(const char* text, const char* search);
	};
}
