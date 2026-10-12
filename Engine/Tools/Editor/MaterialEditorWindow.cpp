#include "MaterialEditorWindow.h"
#include "AssetSystem/AssetManager.h"
#include "AssetSystem/AssetRegistry.h"
#include "AssetSystem/AssetUtil.h"
#include "AssetSystem/MaterialAsset.h"
#include "Utility/PathUtil.h"
#include "imgui.h"
#include <cstdio>

namespace tyr
{
	namespace
	{
		const char* const c_BlendModeNames[] = { "Opaque", "Masked", "Translucent" };
		const PropertiesReflector::EnumNames c_BlendModes = { c_BlendModeNames, 3 };
		const char* const c_TextureLabels[] = { "Albedo", "Normal Height", "AO Roughness Metallic", "Texture 3" };
		const MaterialParams c_DefaultParams;
	}

	MaterialEditorWindow::MaterialEditorWindow(AssetManager& assetManager)
		: m_AssetManager(assetManager)
	{
		m_Reflector.RegisterFieldReflector(GetTypeID<MaterialParams>(), MakeFieldKey("blendMode"), &PropertiesReflector::ReflectEnum, const_cast<PropertiesReflector::EnumNames*>(&c_BlendModes));
		m_Reflector.RegisterFieldReflector(GetTypeID<MaterialParams>(), MakeFieldKey("albedoTint"), &PropertiesReflector::ReflectColour, nullptr);
	}

	MaterialEditorWindow::~MaterialEditorWindow()
	{
		Close();
	}

	void MaterialEditorWindow::Open(AssetID material)
	{
		m_FocusPending = true;
		if (material == m_Material)
		{
			return;
		}
		Close();
		m_Material = material;
		m_AssetManager.LoadMaterial(material);
	}

	void MaterialEditorWindow::Close()
	{
		if (!AssetUtil::IsValidAssetID(m_Material))
		{
			return;
		}
		if (m_Unsaved)
		{
			m_AssetManager.SaveMaterial(m_Material);
		}
		m_AssetManager.DeleteMaterial(m_Material);
		m_Material = {};
		m_Unsaved = false;
	}

	void MaterialEditorWindow::Draw()
	{
		if (!AssetUtil::IsValidAssetID(m_Material))
		{
			return;
		}

		// The window's reference keeps the material loaded, so the asset browser can't delete it.
		const AssetRegistry& registry = AssetRegistry::Instance();
		char name[PathConstants::c_MaxFileNameTotalSize];
		PathUtil::GetFileNameWithoutExtension(registry.GetAssetData(m_Material).filePath.CStr(), name);
		char title[PathConstants::c_MaxFileNameTotalSize + 32];
		snprintf(title, sizeof(title), "Material - %s###MaterialEditor", name);

		if (m_FocusPending)
		{
			ImGui::SetNextWindowFocus();
			m_FocusPending = false;
		}
		ImGui::SetNextWindowSize(ImVec2(380.0f, 420.0f), ImGuiCond_FirstUseEver);
		bool open = true;
		if (ImGui::Begin(title, &open))
		{
			const MaterialAssetFile* file = m_AssetManager.GetMaterialFile(m_Material);
			if (!file)
			{
				ImGui::TextDisabled("Loading");
			}
			else
			{
				// Picks up changes made elsewhere, such as over MCP, whenever nothing is being edited here.
				if (!m_Unsaved)
				{
					m_Params = file->params;
				}

				// The textures are shown for reference. They're set when the material is imported.
				ImGui::SeparatorText("Textures");
				for (uint i = 0; i < file->textures.Size(); ++i)
				{
					char textureName[PathConstants::c_MaxFileNameTotalSize];
					PathUtil::GetFileNameWithoutExtension(registry.GetAssetData(file->textures[i]).filePath.CStr(), textureName);
					ImGui::TextDisabled("%s:", c_TextureLabels[i]);
					ImGui::SameLine();
					ImGui::TextUnformatted(textureName);
				}

				ImGui::SeparatorText("Parameters");
				constexpr ImGuiTableFlags tableFlags = ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV;
				if (ImGui::BeginTable("##MaterialParams", 2, tableFlags))
				{
					ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, 0.45f);
					ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch, 0.55f);
					const TypeInfo& typeInfo = TypeRegistry::Instance().GetType(GetTypeID<MaterialParams>());
					if (m_Reflector.ReflectObject(&m_Params, typeInfo, &c_DefaultParams))
					{
						m_AssetManager.SetMaterialParams(m_Material, m_Params);
						m_Unsaved = true;
					}
					ImGui::EndTable();
				}

				// Saved once nothing is being dragged or typed into, rather than every frame of a drag.
				if (m_Unsaved && !ImGui::IsAnyItemActive())
				{
					m_AssetManager.SaveMaterial(m_Material);
					m_Unsaved = false;
				}
			}
		}
		ImGui::End();

		if (!open)
		{
			Close();
		}
	}
}
