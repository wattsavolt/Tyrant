#pragma once

#include "EditorMacros.h"
#include "PropertiesReflector.h"
#include "Actor/ActorTypes.h"
#include "AssetSystem/AssetID.h"

namespace tyr
{
	class WorldManager;
	class AssetManager;
	struct World;
	struct MeshComponent;

	// Right panel showing the selected actor's components, with their fields editable.
	class TYR_EDITOR_EXPORT PropertiesPanel final
	{
	public:
		PropertiesPanel(WorldManager& worldManager, AssetManager& assetManager, Handle levelWorld);

		// Draws the panel's contents into the current window. selectedActor is the selected
		// actor's root entity, or c_InvalidEntity. Nothing can be changed unless editing.
		void Draw(bool editing, Entity selectedActor);

	private:
		void DrawComponent(World& world, Entity entity, ComponentTypeID typeID, const void* defaults, bool editing);
		// The component's value in the actor type, which the instance's values are compared with.
		static const void* GetDefaults(const ActorEntityDesc* entityDesc, ComponentTypeID typeID, bool isRoot);

		// A mesh field that only lists meshes.
		static bool ReflectMesh(PropertiesReflector& reflector, const Field& field, uint8* object, const uint8* defaults, void* userData);
		// A mesh's materials as one fixed slot per submesh, each its override or the mesh's own.
		static bool ReflectMeshMaterials(PropertiesReflector& reflector, const Field& field, uint8* object, const uint8* defaults, void* userData);
		// One slot's row. Returns true when it was changed.
		bool DrawMaterialSlot(MeshComponent& meshComponent, AssetID meshMaterial, uint slot);

		WorldManager& m_WorldManager;
		AssetManager& m_AssetManager;
		Handle m_LevelWorld;
		PropertiesReflector m_Reflector;
		Entity m_ShownActor = c_InvalidEntity;
		// A component is edited as a copy, so the world can compare it with what it had.
		alignas(16) uint8 m_ComponentCopy[c_MaxActorComponentDataSize];
	};
}
