#pragma once

#include "EditorMacros.h"
#include "PropertiesReflector.h"
#include "Actor/ActorTypes.h"

namespace tyr
{
	class WorldManager;
	struct World;

	// Right panel showing the selected actor's components, with their fields editable.
	class TYR_EDITOR_EXPORT PropertiesPanel final
	{
	public:
		PropertiesPanel(WorldManager& worldManager, Handle levelWorld);

		// Draws the panel's contents into the current window. selectedActor is the selected
		// actor's root entity, or c_InvalidEntity. Nothing can be changed unless editing.
		void Draw(bool editing, Entity selectedActor);

	private:
		void DrawComponent(World& world, Entity entity, ComponentTypeID typeID, bool editing);

		WorldManager& m_WorldManager;
		Handle m_LevelWorld;
		PropertiesReflector m_Reflector;
		Entity m_ShownActor = c_InvalidEntity;
		// A component is edited as a copy, so the world can compare it with what it had.
		alignas(16) uint8 m_ComponentCopy[c_MaxActorComponentDataSize];
	};
}
