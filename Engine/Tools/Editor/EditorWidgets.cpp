#include "EditorWidgets.h"
#include "imgui.h"
#include <cctype>
#include <cfloat>
#include <cstring>

namespace tyr
{
	EditorWidgets::RenameResult EditorWidgets::DrawRenameBox(char* buffer, size_t bufferSize, bool& focusPending, float width)
	{
		if (focusPending)
		{
			ImGui::SetKeyboardFocusHere();
			focusPending = false;
		}

		ImGui::SetNextItemWidth(width > 0.0f ? width : -FLT_MIN);
		const bool entered = ImGui::InputText("##Rename", buffer, bufferSize, ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue);

		if (ImGui::IsKeyPressed(ImGuiKey_Escape))
		{
			return RenameResult::Cancelled;
		}
		if (entered || ImGui::IsItemDeactivated())
		{
			return RenameResult::Committed;
		}
		return RenameResult::Editing;
	}

	bool EditorWidgets::ContainsIgnoreCase(const char* text, const char* search)
	{
		const size_t searchLength = strlen(search);
		for (; *text != '\0'; ++text)
		{
			size_t i = 0;
			while (i < searchLength && text[i] != '\0' && tolower(static_cast<unsigned char>(text[i])) == tolower(static_cast<unsigned char>(search[i])))
			{
				++i;
			}
			if (i == searchLength)
			{
				return true;
			}
		}
		return searchLength == 0;
	}

	void EditorWidgets::MakeDisplayName(const char* typeName, char* displayName, size_t displayNameSize)
	{
		constexpr const char* c_ComponentWord = "Component";
		const size_t componentWordLength = strlen(c_ComponentWord);

		size_t length = 0;
		for (const char* c = typeName; *c != '\0' && length + 1 < displayNameSize;)
		{
			if (strncmp(c, c_ComponentWord, componentWordLength) == 0)
			{
				c += componentWordLength;
				continue;
			}

			// A space starts each new word, at a capital after a lower case letter.
			const bool startsWord = length > 0 && isupper(static_cast<unsigned char>(*c)) && islower(static_cast<unsigned char>(displayName[length - 1]));
			if (startsWord && length + 2 < displayNameSize)
			{
				displayName[length++] = ' ';
			}
			displayName[length++] = *c++;
		}

		// Nothing is left if the name was only "Component".
		if (length == 0)
		{
			strncpy_s(displayName, displayNameSize, typeName, _TRUNCATE);
			return;
		}
		displayName[length] = '\0';
	}
}
