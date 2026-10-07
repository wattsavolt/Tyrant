#pragma once

#include "App/AppBase.h"
#include "EditorMacros.h"
#include "Window/WindowHandle.h"
#include "RenderBase/RenderHandles.h"
#include "EditorUI.h"
#include "EditorViewport.h"
#include "ECS/EntitySystem.h"
#include "Rendering/RenderViewport.h"
#include "String/Name.h"

namespace tyr
{
	struct MeshHeader;
	class WindowModule;
	class Camera;
	class AssetManager;
	class WorldManager;
	struct World;
	class RendererAPI;
	class GUIModule;
	class InputManager;
	class TYR_EDITOR_EXPORT Editor final : public AppBase
	{
	public:
		// app is the game, run inside the editor's viewport while playing.
		Editor(GUIModule& guiModule, AppBase& app);
		~Editor();

		void Initialize() override;
		void Update(float deltaTime) override;
		void Shutdown() override;
		bool WantsExit() const override;

	private:
		// Reads EditorConfig.ini's "ImportDefaultAssets" flag (treated as true if missing) and,
		// if set, imports everything in SourceAssets, then clears it.
		void ImportDefaultAssetsIfNeeded();

		// Starts or stops the game when moving to or from Editing. Pausing just stops updating it.
		void SetPlayState(PlayState state);

		// Sends the View menu's grid setting to the level's viewport when it changes.
		void UpdateGrid();

		// A mesh dropped into the viewport, waiting for its header before it can be placed.
		struct PendingMeshDrop
		{
			EditorViewport::MeshDrop drop;
			// The hierarchy folder the actor goes in, empty for the root.
			RelativePath folder;
			// Set once the actor is spawned. The drop holds a reference on the mesh until the
			// actor's own mesh instance has taken one.
			Entity entity = c_InvalidEntity;
		};

		void HandleRequests(const EditorRequests& requests);
		void AddMeshDrop(const EditorViewport::MeshDrop& drop, const char* folderPath);
		void ProcessPendingMeshDrops();
		// Places a static mesh actor where the drop's ray lands and returns its entity.
		Entity SpawnStaticMeshActor(const PendingMeshDrop& pending, const MeshHeader& header);
		// Places an actor of a registered type in front of the camera, and selects it.
		void PlaceActor(const Id64& actorType, const char* folderPath);
		// Like Unreal, where something dropped onto nothing goes - a set distance along the ray,
		// snapped to the grid when snapping is on.
		Vector3 GetBackgroundDropPosition(const ViewportRay& ray) const;
		ViewportRay GetCameraRay() const;
		static Name MakeUniqueActorName(const World& world, const char* baseName);

		// The root entity of the level actor the ray hits first, or c_InvalidEntity.
		Entity PickActor(const ViewportRay& ray);

		AppBase& m_App;
		PlayState m_PlayState = PlayState::Editing;
		WindowHandle m_PrimaryWindow{};
		URef<Camera> m_Camera;
		WindowModule* m_WindowModule{};
		AssetManager* m_AssetManager{};
		WorldManager* m_WorldManager{};
		RendererAPI* m_RendererAPI{};
		Handle m_LevelEditorWorld{};
		GUIModule* m_GUIModule{};
		InputManager* m_InputManager{};
		// The selected actor's root entity, or c_InvalidEntity.
		Entity m_SelectedActor = c_InvalidEntity;
		static constexpr uint c_MaxPendingMeshDrops = 8;
		LocalArray<PendingMeshDrop, c_MaxPendingMeshDrops> m_PendingMeshDrops;
		// Shared by the grid's rendering and snapping. Starts disabled so the first update sends it.
		ViewportGridDesc m_Grid;
		// Constructed in Initialize() once the modules they need are available.
		URef<EditorUI> m_EditorUI;
		URef<EditorViewport> m_EditorViewport;
	};

}
