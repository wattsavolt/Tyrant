#include "AssetPicker.h"
#include "AssetBrowserPanel.h"
#include "EditorWidgets.h"
#include "AssetSystem/AssetRegistry.h"
#include "AssetSystem/AssetUtil.h"
#include "Utility/PathUtil.h"
#include "imgui.h"
#include <algorithm>
#include <cfloat>
#include <cstdio>
#include <cstring>

namespace tyr
{
	namespace
	{
		constexpr const char* c_ListPopup = "##AssetList";
		// Rows shown before the list scrolls, and the list's narrowest width in font sizes.
		constexpr float c_VisibleRows = 6.0f;
		constexpr float c_MinWidthInFontSizes = 16.0f;

		bool HasExtension(const char* path, const char* extension)
		{
			if (!extension)
			{
				return true;
			}
			const char* pathExtension = strrchr(path, '.');
			return pathExtension && strcmp(pathExtension, extension) == 0;
		}
	}

	bool AssetPicker::Draw(AssetID& asset, const char* extension, float width, bool dimmed)
	{
		char name[PathConstants::c_MaxFileNameTotalSize];
		const RegAssetData* assetData = AssetUtil::IsValidAssetID(asset) ? AssetRegistry::Instance().GetAssets().Find(asset) : nullptr;
		if (assetData)
		{
			PathUtil::GetFileNameWithoutExtension(assetData->filePath.CStr(), name);
		}
		else
		{
			strcpy_s(name, AssetUtil::IsValidAssetID(asset) ? "Missing" : "None");
		}

		char buttonLabel[PathConstants::c_MaxFileNameTotalSize + 16];
		snprintf(buttonLabel, sizeof(buttonLabel), "%s##asset", name);
		if (dimmed)
		{
			ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
		}
		const bool clicked = ImGui::Button(buttonLabel, ImVec2(width, 0.0f));
		if (dimmed)
		{
			ImGui::PopStyleColor();
		}
		ImGui::SetItemTooltip("%s", assetData ? assetData->filePath.CStr() : "Click to pick an asset, or drop one here from the Asset Browser");

		bool changed = false;
		if (ImGui::BeginDragDropTarget())
		{
			const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(AssetBrowserPanel::c_AssetPayload);
			if (!payload)
			{
				payload = ImGui::AcceptDragDropPayload(AssetBrowserPanel::c_MeshPayload);
			}
			if (payload)
			{
				const AssetID dropped = *static_cast<const AssetID*>(payload->Data);
				const RegAssetData* droppedData = AssetRegistry::Instance().GetAssets().Find(dropped);
				if (droppedData && HasExtension(droppedData->filePath.CStr(), extension))
				{
					asset = dropped;
					changed = true;
				}
				else
				{
					TYR_LOG_WARNING("Only a %s asset can go here.", extension ? extension : "registered");
				}
			}
			ImGui::EndDragDropTarget();
		}

		if (clicked)
		{
			ImGui::OpenPopup(c_ListPopup);
			m_Search[0] = '\0';
			m_FocusSearch = true;
			m_RegistryVersion = ~0u;
		}
		changed |= DrawList(asset, extension, width);
		return changed;
	}

	void AssetPicker::RebuildEntries(const char* extension)
	{
		m_Entries.Clear();
		for (std::pair<const AssetID&, const RegAssetData&> registered : AssetRegistry::Instance().GetAssets())
		{
			const char* path = registered.second.filePath.CStr();
			if (!HasExtension(path, extension))
			{
				continue;
			}
			Entry& entry = m_Entries.ExpandOne();
			entry.assetID = registered.first;
			PathUtil::GetFileNameWithoutExtension(path, entry.name);
		}
		std::sort(m_Entries.begin(), m_Entries.end(), [](const Entry& a, const Entry& b)
		{
			return _stricmp(a.name, b.name) < 0;
		});
	}

	bool AssetPicker::DrawList(AssetID& asset, const char* extension, float width)
	{
		ImGui::SetNextWindowSize(ImVec2(std::max(width, ImGui::GetFontSize() * c_MinWidthInFontSizes), 0.0f));
		if (!ImGui::BeginPopup(c_ListPopup))
		{
			return false;
		}

		// Built when the list opens, and again if assets are added or removed while it's open.
		const uint registryVersion = AssetRegistry::Instance().GetVersion();
		if (registryVersion != m_RegistryVersion)
		{
			RebuildEntries(extension);
			m_RegistryVersion = registryVersion;
		}

		if (m_FocusSearch)
		{
			ImGui::SetKeyboardFocusHere();
			m_FocusSearch = false;
		}
		ImGui::SetNextItemWidth(-FLT_MIN);
		const bool entered = ImGui::InputTextWithHint("##Search", "Search", m_Search, sizeof(m_Search), ImGuiInputTextFlags_EnterReturnsTrue);

		bool changed = false;
		const ImGuiStyle& style = ImGui::GetStyle();
		const float listHeight = ImGui::GetTextLineHeightWithSpacing() * c_VisibleRows + style.FramePadding.y * 2.0f;
		if (ImGui::BeginListBox("##Assets", ImVec2(-FLT_MIN, listHeight)))
		{
			bool anyShown = false;
			for (uint i = 0; i < m_Entries.Size(); ++i)
			{
				const Entry& entry = m_Entries[i];
				if (!EditorWidgets::ContainsIgnoreCase(entry.name, m_Search))
				{
					continue;
				}

				// Enter picks the first match.
				const bool pickFirst = entered && !anyShown;
				anyShown = true;
				ImGui::PushID(static_cast<int>(i));
				if (ImGui::Selectable(entry.name, entry.assetID == asset) || pickFirst)
				{
					changed = !(asset == entry.assetID);
					asset = entry.assetID;
					ImGui::CloseCurrentPopup();
				}
				if (const RegAssetData* data = AssetRegistry::Instance().GetAssets().Find(entry.assetID))
				{
					ImGui::SetItemTooltip("%s", data->filePath.CStr());
				}
				ImGui::PopID();
			}
			if (!anyShown)
			{
				ImGui::TextDisabled("No matches");
			}
			ImGui::EndListBox();
		}

		if (ImGui::IsKeyPressed(ImGuiKey_Escape, false))
		{
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
		return changed;
	}
}
