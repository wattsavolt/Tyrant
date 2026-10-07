#include "PropertiesPanel.h"
#include "EditorWidgets.h"
#include "World/WorldManager.h"
#include "World/World.h"
#include "ECS/Components.h"
#include "imgui.h"
#include <cstring>

namespace tyr
{
	PropertiesPanel::PropertiesPanel(WorldManager& worldManager, Handle levelWorld)
		: m_WorldManager(worldManager)
		, m_LevelWorld(levelWorld)
	{
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

			world.entities.ForEachComponentType(entity, [this, &world, entity, editing](ComponentTypeID typeID)
			{
				DrawComponent(world, entity, typeID, editing);
			});
			ImGui::PopID();
		}
		ImGui::EndDisabled();
	}

	void PropertiesPanel::DrawComponent(World& world, Entity entity, ComponentTypeID typeID, bool editing)
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
				changed = m_Reflector.ReflectObject(m_ComponentCopy, *typeInfo);
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
