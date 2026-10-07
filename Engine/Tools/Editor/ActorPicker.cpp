#include "ActorPicker.h"
#include "EditorWidgets.h"
#include "Actor/ActorRegistry.h"
#include "imgui.h"
#include <algorithm>
#include <cctype>
#include <cfloat>
#include <cstring>

namespace tyr
{
	namespace
	{
		bool ContainsIgnoringCase(const char* text, const char* search)
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
	}

	ActorPicker::ActorPicker()
	{
		// Actor types are all registered before the editor starts, so the list never changes.
		const HashMap<Id64, ActorTypeDesc>& actorTypes = ActorRegistry::Instance().GetAll();
		m_Entries.Reserve(actorTypes.Size());
		for (std::pair<const Id64&, const ActorTypeDesc&> actorType : actorTypes)
		{
			Entry& entry = m_Entries.ExpandOne();
			entry.typeID = actorType.first;
			EditorWidgets::MakeDisplayName(actorType.second.name.CStr(), entry.displayName, sizeof(entry.displayName));
		}
		std::sort(m_Entries.begin(), m_Entries.end(), [](const Entry& a, const Entry& b)
		{
			return strcmp(a.displayName, b.displayName) < 0;
		});
	}

	bool ActorPicker::Draw(Id64& pickedType, bool focusSearch, float listHeight)
	{
		if (focusSearch)
		{
			ImGui::SetKeyboardFocusHere();
		}
		ImGui::SetNextItemWidth(-FLT_MIN);
		ImGui::InputTextWithHint("##Search", "Search actors", m_Search, sizeof(m_Search));

		bool picked = false;
		if (ImGui::BeginListBox("##Actors", ImVec2(-FLT_MIN, listHeight > 0.0f ? listHeight : ImGui::GetContentRegionAvail().y)))
		{
			for (const Entry& entry : m_Entries)
			{
				if (!ContainsIgnoringCase(entry.displayName, m_Search))
				{
					continue;
				}
				ImGui::PushID(entry.displayName);
				if (ImGui::Selectable(entry.displayName))
				{
					pickedType = entry.typeID;
					picked = true;
				}
				ImGui::PopID();
			}
			ImGui::EndListBox();
		}
		return picked;
	}
}
