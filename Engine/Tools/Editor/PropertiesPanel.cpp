#include "PropertiesPanel.h"
#include "EditorWidgets.h"
#include "World/WorldManager.h"
#include "World/World.h"
#include "ECS/Components.h"
#include "Actor/ActorRegistry.h"
#include "AssetBrowserPanel.h"
#include "AssetSystem/AssetManager.h"
#include "AssetSystem/AssetRegistry.h"
#include "AssetSystem/AssetUtil.h"
#include "AssetSystem/AssetConstants.h"
#include "AssetSystem/MeshAsset.h"
#include "Utility/PathUtil.h"
#include <cstdio>
#include "imgui.h"
#include <cstring>

namespace tyr
{
	namespace
	{
		AssetID GetEngineDefaultMaterial()
		{
			char path[PathConstants::c_MaxAssetPathTotalSize];
			snprintf(path, sizeof(path), "%s/%s%s", AssetConstants::c_DefaultMaterialFolderName, AssetConstants::c_DefaultMaterialName, AssetConstants::c_MaterialFileExtension);
			return AssetRegistry::Instance().GetAssetID(path);
		}

		// An invalid material makes the slot use the mesh's own. Trailing ones are dropped, so a
		// mesh with no overrides has an empty list, the same as its actor type.
		void SetMaterialSlot(MeshComponent& meshComponent, uint slot, AssetID material)
		{
			LocalArray<AssetID, MeshConstants::c_MaxSubmeshes>& materials = meshComponent.materials;
			if (AssetUtil::IsValidAssetID(material))
			{
				while (materials.Size() <= slot)
				{
					materials.Add(AssetConstants::c_InvalidAssetID);
				}
				materials[slot] = material;
				return;
			}

			if (slot < materials.Size())
			{
				materials[slot] = AssetConstants::c_InvalidAssetID;
			}
			while (!materials.IsEmpty() && !AssetUtil::IsValidAssetID(materials.Back()))
			{
				materials.PopBack();
			}
		}
	}

	PropertiesPanel::PropertiesPanel(WorldManager& worldManager, AssetManager& assetManager, Handle levelWorld)
		: m_WorldManager(worldManager)
		, m_AssetManager(assetManager)
		, m_LevelWorld(levelWorld)
	{
		m_Reflector.RegisterFieldReflector(GetTypeID<MeshComponent>(), MakeFieldKey("mesh"), &ReflectMesh, this);
		m_Reflector.RegisterFieldReflector(GetTypeID<MeshComponent>(), MakeFieldKey("materials"), &ReflectMeshMaterials, this);
	}

	bool PropertiesPanel::ReflectMesh(PropertiesReflector& reflector, const Field& field, uint8* object, const uint8* defaults, void* /*userData*/)
	{
		AssetID& mesh = *reinterpret_cast<AssetID*>(object + field.dataOffset);
		if (defaults)
		{
			reflector.SetNextRowOverridden(memcmp(&mesh, defaults + field.dataOffset, sizeof(AssetID)) != 0);
		}
		reflector.BeginValueRow(field.name);
		bool changed = false;
		if (reflector.TakeResetRequest() && defaults)
		{
			memcpy(&mesh, defaults + field.dataOffset, sizeof(AssetID));
			changed = true;
		}

		const float clearWidth = ImGui::GetFrameHeight();
		const float spacing = ImGui::GetStyle().ItemInnerSpacing.x;
		changed |= reflector.GetAssetPicker().Draw(mesh, AssetConstants::c_MeshFileExtension, ImGui::GetContentRegionAvail().x - clearWidth - spacing);
		ImGui::SameLine(0.0f, spacing);
		if (ImGui::Button("X##clear", ImVec2(clearWidth, 0.0f)))
		{
			mesh = AssetConstants::c_InvalidAssetID;
			changed = true;
		}
		ImGui::SetItemTooltip("Clear");
		return changed;
	}

	bool PropertiesPanel::ReflectMeshMaterials(PropertiesReflector& reflector, const Field& field, uint8* object, const uint8* defaults, void* userData)
	{
		PropertiesPanel& panel = *static_cast<PropertiesPanel*>(userData);
		MeshComponent& meshComponent = *reinterpret_cast<MeshComponent*>(object);
		const MeshHeader* header = AssetUtil::IsValidAssetID(meshComponent.mesh) ? panel.m_AssetManager.GetMeshHeader(meshComponent.mesh) : nullptr;

		// Marked when any slot differs from the actor type, and reset as a whole from there.
		if (defaults)
		{
			reflector.SetNextRowOverridden(!field.customPropertiesReflector->Equals(object + field.dataOffset, defaults + field.dataOffset));
		}
		const bool open = reflector.BeginGroupRow(field.name);
		bool changed = false;
		if (reflector.TakeResetRequest() && defaults)
		{
			memcpy(object + field.dataOffset, defaults + field.dataOffset, field.size);
			changed = true;
		}

		ImGui::AlignTextToFramePadding();
		if (!header)
		{
			ImGui::TextDisabled(AssetUtil::IsValidAssetID(meshComponent.mesh) ? "Loading" : "No mesh");
		}
		else
		{
			ImGui::Text("%u slots", header->materials.Size());
		}

		if (open)
		{
			if (header)
			{
				for (uint slot = 0; slot < header->materials.Size(); ++slot)
				{
					ImGui::PushID(static_cast<int>(slot));
					changed |= panel.DrawMaterialSlot(meshComponent, header->materials[slot], slot);
					ImGui::PopID();
				}
			}
			PropertiesReflector::EndGroupRow();
		}
		return changed;
	}

	bool PropertiesPanel::DrawMaterialSlot(MeshComponent& meshComponent, AssetID meshMaterial, uint slot)
	{
		char label[24];
		snprintf(label, sizeof(label), "Element %u", slot);
		m_Reflector.BeginValueRow(label);

		const AssetID overrideMaterial = slot < meshComponent.materials.Size() ? meshComponent.materials[slot] : AssetConstants::c_InvalidAssetID;
		const bool usesMeshMaterial = !AssetUtil::IsValidAssetID(overrideMaterial);

		const ImGuiStyle& style = ImGui::GetStyle();
		const float spacing = style.ItemInnerSpacing.x;
		const float clearWidth = ImGui::GetFrameHeight();
		const float defaultWidth = ImGui::CalcTextSize("Default").x + style.FramePadding.x * 2.0f;

		// The mesh's own material is shown dimmed, so overrides stand out.
		bool changed = false;
		AssetID material = usesMeshMaterial ? meshMaterial : overrideMaterial;
		const float pickerWidth = ImGui::GetContentRegionAvail().x - clearWidth - defaultWidth - spacing * 2.0f;
		if (m_Reflector.GetAssetPicker().Draw(material, AssetConstants::c_MaterialFileExtension, pickerWidth, usesMeshMaterial))
		{
			SetMaterialSlot(meshComponent, slot, material);
			changed = true;
		}

		ImGui::SameLine(0.0f, spacing);
		if (ImGui::Button("X##clear", ImVec2(clearWidth, 0.0f)))
		{
			SetMaterialSlot(meshComponent, slot, GetEngineDefaultMaterial());
			changed = true;
		}
		ImGui::SetItemTooltip("Use the engine's default material");

		ImGui::SameLine(0.0f, spacing);
		ImGui::BeginDisabled(usesMeshMaterial);
		if (ImGui::Button("Default##mesh"))
		{
			SetMaterialSlot(meshComponent, slot, AssetConstants::c_InvalidAssetID);
			changed = true;
		}
		ImGui::EndDisabled();
		ImGui::SetItemTooltip("Use the mesh's own material");
		return changed;
	}

	void PropertiesPanel::Draw(bool editing, Entity selectedActor)
	{
		World& world = m_WorldManager.GetWorld(m_LevelWorld);

		const ActorInstance* actor = nullptr;
		for (const ActorInstance& instance : world.actorInstances)
		{
			if (instance.RootEntity() == selectedActor)
			{
				actor = &instance;
				break;
			}
		}
		if (!actor)
		{
			ImGui::TextDisabled("Select an actor to see its properties.");
			return;
		}

		// The rotation angles shown belong to the previous actor.
		if (selectedActor != m_ShownActor)
		{
			m_Reflector.ResetRotationCache();
			m_ShownActor = selectedActor;
		}

		ImGui::TextUnformatted(actor->name.CStr());
		ImGui::Separator();

		const ActorTypeDesc* actorType = ActorRegistry::Instance().FindActorType(actor->typeID);
		ImGui::BeginDisabled(!editing);
		for (uint i = 0; i < actor->entities.Size(); ++i)
		{
			const Entity entity = actor->entities[i];
			ImGui::PushID(static_cast<int>(entity));

			// An actor with more than one entity shows each one's components separately.
			if (actor->entities.Size() > 1)
			{
				char entityLabel[32];
				snprintf(entityLabel, sizeof(entityLabel), i == 0 ? "Root" : "Entity %u", i);
				ImGui::SeparatorText(entityLabel);
			}

			const ActorEntityDesc* entityDesc = actorType && i < actorType->entities.Size() ? &actorType->entities[i] : nullptr;
			world.entities.ForEachComponentType(entity, [this, &world, entity, entityDesc, i, editing](ComponentTypeID typeID)
			{
				DrawComponent(world, entity, typeID, GetDefaults(entityDesc, typeID, i == 0), editing);
			});
			ImGui::PopID();
		}
		ImGui::EndDisabled();
	}

	const void* PropertiesPanel::GetDefaults(const ActorEntityDesc* entityDesc, ComponentTypeID typeID, bool isRoot)
	{
		if (!entityDesc)
		{
			return nullptr;
		}

		// The root's transform is where the actor was placed, not a change to its type.
		const Id64& reflectionTypeID = ComponentRegistry::Instance().GetReflectionTypeID(typeID);
		if (isRoot && reflectionTypeID == GetTypeID<ComponentTransform>())
		{
			return nullptr;
		}

		for (const ActorComponentDesc& componentDesc : entityDesc->components)
		{
			if (componentDesc.typeID == reflectionTypeID)
			{
				return componentDesc.data;
			}
		}
		return nullptr;
	}

	void PropertiesPanel::DrawComponent(World& world, Entity entity, ComponentTypeID typeID, const void* defaults, bool editing)
	{
		const TypeInfo* typeInfo = TypeRegistry::Instance().FindType(ComponentRegistry::Instance().GetReflectionTypeID(typeID));
		if (!typeInfo)
		{
			return;
		}
		TYR_ASSERT(typeInfo->size <= sizeof(m_ComponentCopy));

		// Room for a type name with a space between each word.
		char displayName[64];
		EditorWidgets::MakeDisplayName(typeInfo->name.CStr(), displayName, sizeof(displayName));

		ImGui::PushID(static_cast<int>(typeID));
		if (ImGui::CollapsingHeader(displayName, ImGuiTreeNodeFlags_DefaultOpen))
		{
			const void* component = world.entities.GetComponentData(entity, typeID);
			memcpy(m_ComponentCopy, component, typeInfo->size);

			// A mesh can't be swapped while the one it has is still loading.
			bool locked = false;
			if (typeID == ComponentRegistry::GetComponentTypeID<MeshComponent>())
			{
				const MeshComponent& meshComponent = *static_cast<const MeshComponent*>(component);
				locked = meshComponent.meshInstanceRequested && !meshComponent.meshInstance;
			}

			bool changed = false;
			ImGui::BeginDisabled(locked);
			constexpr ImGuiTableFlags tableFlags = ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV;
			if (ImGui::BeginTable("##Properties", 2, tableFlags))
			{
				ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, 0.4f);
				ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch, 0.6f);
				changed = m_Reflector.ReflectObject(m_ComponentCopy, *typeInfo, defaults);
				ImGui::EndTable();
			}
			ImGui::EndDisabled();

			if (changed && editing)
			{
				m_WorldManager.SetComponentData(m_LevelWorld, entity, typeID, m_ComponentCopy);
			}
		}
		ImGui::PopID();
	}
}
