/// Copyright (c) 2023 Aidan Clear 

#include "Editor.h"
#include "BuildConfig.h"
#include "AssetSystem/AssetModule.h"
#include "AssetSystem/AssetManager.h"
#include "AssetSystem/AssetRegistry.h"
#include "Window/WindowModule.h"
#include "Window/WindowDesc.h"
#include "World/WorldModule.h"
#include "World/WorldManager.h"
#include "World/World.h"
#include "Math/Vector2.h"
#include "World/Camera.h"
#include "AssetSystem/AssetUtil.h"
#include "ECS/Components.h"
#include "RendererModule.h"
#include "Rendering/RendererAPI.h"
#include "GUI/GUIModule.h"
#include "Input/InputModule.h"
#include "Input/InputManager.h"
#include "Importing/ModelImporter.h"
#include "Importing/MaterialImporter.h"
#include "Config/Config.h"
#include "Actor/ActorRegistry.h"
#include "Level/LevelFile.h"
#include "EditorSettings.h"
#include "Debug/DebugDraw.h"
#include "Gameplay/Doors.h"
#include "Gameplay/Noise.h"
#include "AI/EnemyBrains.h"
#include "AssetSystem/MeshAsset.h"
#include "Utility/PathUtil.h"
#include "Math/Math.h"
#include "Math/Matrix3.h"
#include "Mcp/McpServer.h"
#include "Mcp/McpTools.h"
#include "UndoHistory.h"
#include "imgui.h"
#include <algorithm>
#include <cfloat>
#include <cstring>
#include <utility>

namespace tyr
{
	namespace
	{
		// The animation clip made for the skeleton that comes first by name, or an invalid ID.
		AssetID FindFirstAnimation(AssetID skeleton)
		{
			AssetID found = AssetConstants::c_InvalidAssetID;
			const char* foundPath = nullptr;
			const AssetRegistry& registry = AssetRegistry::Instance();
			for (std::pair<const AssetID&, const RegAssetData&> asset : registry.GetAssets())
			{
				const char* path = asset.second.filePath.CStr();
				if (!AssetUtil::HasExtension(path, AssetConstants::c_AnimationFileExtension))
				{
					continue;
				}
				uint dependencyCount = 0;
				const AssetID* dependencies = registry.GetAssetDependencies(asset.first, dependencyCount);
				if (dependencyCount > 0 && dependencies[0] == skeleton && (!foundPath || strcmp(path, foundPath) < 0))
				{
					found = asset.first;
					foundPath = path;
				}
			}
			return found;
		}

		// How far in front of the camera a mesh dropped onto nothing is placed.
		constexpr float c_BackgroundDropDistance = 5.0f;
		// The level made when there aren't any yet.
		constexpr const char* c_FirstLevelName = "Main";
		// How long a directional light's direction is drawn, in metres.
		constexpr float c_DirLightArrowLength = 2.0f;
		// How far above the player's feet the editor's camera is taken to be, when the player
		// starts there.
		constexpr float c_FallbackEyeHeight = 1.6f;
		const Id64 c_PlayerStartType("PlayerStartActor");

		bool HasExtension(const char* path, const char* extension)
		{
			const char* pathExtension = strrchr(path, '.');
			return pathExtension && strcmp(pathExtension, extension) == 0;
		}

		float SnapToGrid(float value, float cellSize)
		{
			return Math::Round(value / cellSize) * cellSize;
		}

		struct RayHit
		{
			float distance;
			// The axis of the face the ray entered through, and which way that face points.
			uint axis;
			float normalSign;
		};

		// Where the ray enters the box. Fails when it misses or starts inside it.
		bool IntersectBox(const Vector3& origin, const Vector3& direction, const Vector3& boxMin, const Vector3& boxMax, RayHit& hit)
		{
			float tNear = -FLT_MAX;
			float tFar = FLT_MAX;
			uint nearAxis = 0;
			for (uint i = 0; i < 3; ++i)
			{
				if (Math::Abs(direction[i]) < 1e-8f)
				{
					if (origin[i] < boxMin[i] || origin[i] > boxMax[i])
					{
						return false;
					}
					continue;
				}

				const float invDirection = 1.0f / direction[i];
				float t0 = (boxMin[i] - origin[i]) * invDirection;
				float t1 = (boxMax[i] - origin[i]) * invDirection;
				if (t0 > t1)
				{
					std::swap(t0, t1);
				}
				if (t0 > tNear)
				{
					tNear = t0;
					nearAxis = i;
				}
				tFar = std::min(tFar, t1);
				if (tNear > tFar)
				{
					return false;
				}
			}

			if (tNear <= 0.0f)
			{
				return false;
			}
			hit.distance = tNear;
			hit.axis = nearAxis;
			hit.normalSign = direction[nearAxis] > 0.0f ? -1.0f : 1.0f;
			return true;
		}

		// World-space bounds enclosing a box component however its entity is rotated.
		void CalculateWorldBounds(const BoxComponent& box, const Transform& transform, Vector3& outMin, Vector3& outMax)
		{
			Matrix3 rotation;
			transform.rotation.ToRotationMatrix(rotation);
			const Vector3 scaledCenter = box.center * transform.scale;
			const Vector3 scaledHalfExtents = box.halfExtents * transform.scale;

			// Each row is a local axis in world space, the same as Matrix4::SetTRS.
			Vector3 center = transform.position;
			Vector3 extents = Vector3::c_Zero;
			for (uint row = 0; row < 3; ++row)
			{
				for (uint col = 0; col < 3; ++col)
				{
					center[col] += scaledCenter[row] * rotation[row][col];
					extents[col] += Math::Abs(scaledHalfExtents[row] * rotation[row][col]);
				}
			}
			outMin = center - extents;
			outMax = center + extents;
		}

		// The nearest actor box the ray hits.
		bool RaycastActors(World& world, const ViewportRay& ray, RayHit& nearestHit, Entity& hitEntity)
		{
			nearestHit.distance = FLT_MAX;
			hitEntity = c_InvalidEntity;
			world.entities.ForEach<BoxComponent>([&](Entity entity, BoxComponent& box)
			{
				if (!world.entities.HasComponent<ComponentTransform>(entity))
				{
					return;
				}

				Vector3 boxMin;
				Vector3 boxMax;
				CalculateWorldBounds(box, world.entities.GetComponent<ComponentTransform>(entity).world, boxMin, boxMax);
				RayHit hit;
				if (IntersectBox(ray.origin, ray.direction, boxMin, boxMax, hit) && hit.distance < nearestHit.distance)
				{
					nearestHit = hit;
					hitEntity = entity;
				}
			});
			return hitEntity != c_InvalidEntity;
		}
	}

	Editor::Editor(GUIModule& guiModule, AppBase& app)
		: m_App(app)
		, m_GUIModule(&guiModule)
	{
	}

	Editor::~Editor()
	{
		
	}

	void Editor::Initialize()
	{
		TYR_GET_MODULE(WindowModule, m_WindowModule);

		AssetModule* assetModule;
		TYR_GET_MODULE(AssetModule, assetModule);
		m_AssetManager = assetModule->GetAssetManager();

		WorldModule* worldModule;
		TYR_GET_MODULE(WorldModule, worldModule);
		m_WorldManager = worldModule->GetWorldManager();

		RendererModule* rendererModule;
		TYR_GET_MODULE(RendererModule, rendererModule);
		m_RendererAPI = rendererModule->GetRendererAPI();

		// Get project related properties from the config later
		WindowDesc desc;
		// SW_MAXIMIZE, so the editor opens filling the screen.
		desc.showFlag = 3;
		desc.name = c_AppName;
		Platform::GetMaxWindowResolution(desc.width, desc.height);

		// 10 metres above the grid and pulled back along -Z, looking down at the origin.
		m_Camera = MakeURef<Camera>(Vector3(0.0f, 10.0f, -10.0f), Vector3::c_Up, Vector3::c_Forward, 90.0f, 0.1f, 2000.0f);
		m_Camera->Target(Vector3::c_Zero);

		WorldConfig worldParams{};
		worldParams.camera = m_Camera.get();
		// The level is only being edited, so nothing in it simulates.
		worldParams.simulate = false;

		m_LevelEditorWorld = CreatePrimaryWorld(*m_WindowModule, *m_WorldManager, desc, worldParams, m_PrimaryWindow);
		m_WindowModule->SetCloseIntercepted(m_PrimaryWindow, true);
		m_GUIModule->SetPrimaryWindow(m_WindowModule, m_PrimaryWindow);

		InputModule* inputModule;
		TYR_GET_MODULE(InputModule, inputModule);
		m_InputManager = inputModule->GetInputManager();
		m_InputManager->SetWindow(m_WindowModule, m_PrimaryWindow);
		m_InputManager->SetGameInputEnabled(false);

		m_Settings = MakeURef<EditorSettings>();

		// Before the UI, which loads the editor icons it imports.
		ImportDefaultAssetsIfNeeded();

		m_EditorUI = MakeURef<EditorUI>(*m_GUIModule, *m_RendererAPI, *m_AssetManager, *m_WorldManager, m_LevelEditorWorld, *m_Settings);
		m_EditorViewport = MakeURef<EditorViewport>(*m_RendererAPI, m_EditorUI->GetIcons());
		m_EditorUI->ApplyRenderSettings(true);
		m_UndoHistory = MakeURef<UndoHistory>(*m_WorldManager, m_LevelEditorWorld);

		if (AssetRegistry::Instance().RemoveMissingAssets() > 0)
		{
			AssetRegistry::Instance().Save();
		}
		OpenStartLevel();

		const uint16 mcpPort = m_Settings->GetMcpPort();
		if (mcpPort != 0)
		{
			m_McpServer = MakeURef<McpServer>();
			if (m_McpServer->Start(mcpPort))
			{
				m_McpTools = MakeURef<McpTools>(*this);
				TYR_LOG_INFO("MCP server listening on http://127.0.0.1:%u/mcp", static_cast<uint>(mcpPort));
			}
			else
			{
				m_McpServer.reset();
			}
		}
	}

	void Editor::OpenStartLevel()
	{
		const AssetRegistry& registry = AssetRegistry::Instance();
		AssetPath defaultLevel;
		if (m_Settings->GetDefaultLevel(defaultLevel))
		{
			const AssetID level = registry.GetAssetID(defaultLevel.CStr());
			if (AssetUtil::IsValidAssetID(level))
			{
				OpenLevel(level);
				return;
			}
			TYR_LOG_WARNING("The default level %s no longer exists.", defaultLevel.CStr());
		}

		// Whatever opens instead becomes the default.
		for (const std::pair<const AssetID&, const RegAssetData&> asset : registry.GetAssets())
		{
			if (HasExtension(asset.second.filePath.CStr(), AssetConstants::c_LevelFileExtension))
			{
				OpenLevel(asset.first);
				SetDefaultLevel(m_Level);
				return;
			}
		}

		// The first level made becomes the default.
		NewLevel(c_FirstLevelName);
		if (AssetUtil::IsValidAssetID(m_Level))
		{
			SetDefaultLevel(m_Level);
		}
	}

	void Editor::SetDefaultLevel(AssetID level)
	{
		if (AssetUtil::IsValidAssetID(level))
		{
			m_Settings->SetDefaultLevel(AssetRegistry::Instance().GetAssetData(level).filePath.CStr());
		}
	}

	void Editor::NewLevel(const char* name)
	{
		m_WorldManager->ClearWorld(m_LevelEditorWorld);
		m_SelectedActor = c_InvalidEntity;

		if (const ActorTypeDesc* lightType = ActorRegistry::Instance().FindActorType(Id64("DirLightActor")))
		{
			AddActor(*lightType, "Sun", "");
		}
		SaveLevelAs(name);
		m_UndoHistory->Reset();
	}

	void Editor::OpenLevel(AssetID level)
	{
		m_WorldManager->ClearWorld(m_LevelEditorWorld);
		m_SelectedActor = c_InvalidEntity;

		const char* levelPath = AssetRegistry::Instance().GetAssetData(level).filePath.CStr();
		LevelFile::Load(levelPath, *m_WorldManager, m_LevelEditorWorld);
		m_Level = level;
		m_SavedChangeCount = m_WorldManager->GetWorld(m_LevelEditorWorld).changeCount;
		m_UndoHistory->Reset();
	}

	void Editor::RefreshAssets()
	{
		if (m_PlayState != PlayState::Editing || m_RefreshStage != RefreshStage::None)
		{
			return;
		}

		const World& world = m_WorldManager->GetWorld(m_LevelEditorWorld);
		constexpr uint c_ChunkSize = 1024 * 1024;
		m_RefreshData.Resize(c_ChunkSize);
		m_RefreshActorOffsets.Clear();
		uint size = LevelFile::WriteLevelState(world, m_RefreshData.Data(), m_RefreshData.Size());
		TYR_ASSERT(size > 0);
		for (const ActorInstance& actor : world.actorInstances)
		{
			uint written = 0;
			while ((written = LevelFile::WriteActorState(world, actor, m_RefreshData.Data() + size, m_RefreshData.Size() - size)) == 0)
			{
				m_RefreshData.Resize(m_RefreshData.Size() + c_ChunkSize);
			}
			m_RefreshActorOffsets.Add(size);
			size += written;
		}
		m_RefreshActorOffsets.Add(size);
		m_RefreshDirty = IsLevelDirty();
		m_RefreshSelectedActor = {};
		for (const ActorInstance& actor : world.actorInstances)
		{
			if (actor.RootEntity() == m_SelectedActor)
			{
				m_RefreshSelectedActor = actor.id;
			}
		}

		// Everything the level holds is let go of, so it can be loaded again from its file.
		m_EditorUI->CloseMaterialEditor();
		m_WorldManager->ClearWorld(m_LevelEditorWorld);
		m_SelectedActor = c_InvalidEntity;
		m_RefreshStage = RefreshStage::Cleared;
	}

	void Editor::UpdateAssetRefresh()
	{
		if (m_RefreshStage == RefreshStage::Cleared)
		{
			m_RefreshStage = RefreshStage::Restoring;
			return;
		}
		if (m_RefreshStage != RefreshStage::Restoring)
		{
			return;
		}
		m_AssetManager->EvictAllUnreferencedAssets();
		if (m_AssetManager->HasPendingDeletes())
		{
			return;
		}

		World& world = m_WorldManager->GetWorld(m_LevelEditorWorld);
		LevelFile::ReadLevelState(m_RefreshData.Data(), m_RefreshActorOffsets.IsEmpty() ? 0 : m_RefreshActorOffsets[0], world);
		for (uint i = 0; i + 1 < m_RefreshActorOffsets.Size(); ++i)
		{
			const uint offset = m_RefreshActorOffsets[i];
			const Entity root = LevelFile::AddActorFromState(m_RefreshData.Data() + offset, m_RefreshActorOffsets[i + 1] - offset, *m_WorldManager, m_LevelEditorWorld);
			for (const ActorInstance& actor : world.actorInstances)
			{
				if (actor.RootEntity() == root && actor.id == m_RefreshSelectedActor)
				{
					m_SelectedActor = root;
				}
			}
		}

		// Restoring counts as edits, so the saved state and undo history are put back as they were.
		m_SavedChangeCount = m_RefreshDirty ? world.changeCount - 1 : world.changeCount;
		m_UndoHistory->Reset();
		m_RefreshData.Clear();
		m_RefreshActorOffsets.Clear();
		m_RefreshStage = RefreshStage::None;
	}

	void Editor::SaveAll()
	{
		// Only the level can have unsaved changes for now.
		SaveLevel();
	}

	void Editor::SaveLevel()
	{
		if (AssetUtil::IsValidAssetID(m_Level))
		{
			WriteLevel(AssetRegistry::Instance().GetAssetData(m_Level).filePath.CStr());
		}
	}

	void Editor::SaveLevelAs(const char* name)
	{
		char levelPath[PathConstants::c_MaxAssetPathTotalSize];
		snprintf(levelPath, sizeof(levelPath), "%s/%s%s", AssetConstants::c_LevelFolderName, name, AssetConstants::c_LevelFileExtension);
		WriteLevel(levelPath);
	}

	bool Editor::WriteLevel(const char* levelPath)
	{
		// Copied, since saving can move the registry's own copy of the path.
		const AssetPath path = levelPath;
		const World& world = m_WorldManager->GetWorld(m_LevelEditorWorld);
		const AssetID level = LevelFile::Save(path.CStr(), world);
		if (!AssetUtil::IsValidAssetID(level))
		{
			return false;
		}

		AssetRegistry::Instance().Save();
		m_Level = level;
		m_SavedChangeCount = world.changeCount;
		return true;
	}

	bool Editor::IsLevelDirty() const
	{
		return m_WorldManager->GetWorld(m_LevelEditorWorld).changeCount != m_SavedChangeCount;
	}

	void Editor::ImportDefaultAssetsIfNeeded()
	{
		if (!m_Settings->GetImportDefaultAssets())
		{
			return;
		}

		// Imported from separate source images like any other material, so the importer does
		// the channel packing.
		char albedoPath[TYR_MAX_PATH_TOTAL_SIZE];
		char normalPath[TYR_MAX_PATH_TOTAL_SIZE];
		char heightPath[TYR_MAX_PATH_TOTAL_SIZE];
		char occlusionPath[TYR_MAX_PATH_TOTAL_SIZE];
		char roughnessMetallicPath[TYR_MAX_PATH_TOTAL_SIZE];
		AssetUtil::CreateFullSourceAssetPath(albedoPath, "Default/DefaultAlbedo.png");
		AssetUtil::CreateFullSourceAssetPath(normalPath, "Default/DefaultNormal.png");
		AssetUtil::CreateFullSourceAssetPath(heightPath, "Default/DefaultHeight.png");
		AssetUtil::CreateFullSourceAssetPath(occlusionPath, "Default/DefaultOcclusion.png");
		AssetUtil::CreateFullSourceAssetPath(roughnessMetallicPath, "Default/DefaultRoughnessMetallic.png");

		PbrMaterialImportDesc defaultMaterialDesc;
		defaultMaterialDesc.outputFolderPath = AssetConstants::c_DefaultMaterialFolderName;
		defaultMaterialDesc.materialName = AssetConstants::c_DefaultMaterialName;
		defaultMaterialDesc.albedoSource = TextureSource{ albedoPath };
		defaultMaterialDesc.normalSource = TextureSource{ normalPath };
		defaultMaterialDesc.heightSource = TextureSource{ heightPath };
		defaultMaterialDesc.occlusionSource = TextureSource{ occlusionPath };
		defaultMaterialDesc.roughnessMetallicSource = TextureSource{ roughnessMetallicPath };
		bool imported = true;
		AssetID defaultMaterialID;
		if (!MaterialImporter::Instance().ImportPbrMaterial(defaultMaterialDesc, defaultMaterialID))
		{
			TYR_LOG_ERROR("Failed to import the default material from SourceAssets.");
			imported = false;
		}

		EditorIcons::Import();

		char sourcePath[TYR_MAX_PATH_TOTAL_SIZE];
		AssetUtil::CreateFullSourceAssetPath(sourcePath, "Cube/Cube.glb");
		if (!ModelImporter::Instance().ImportModel(sourcePath, "Models/Cube", "Cube"))
		{
			TYR_LOG_ERROR("Failed to import the test cube from SourceAssets.");
			imported = false;
		}

		// Saved straight away so what was imported is never lost if the editor doesn't close cleanly.
		AssetRegistry::Instance().Save();

		// Cleared so this doesn't repeat on every subsequent startup.
		if (imported)
		{
			m_Settings->SetImportDefaultAssets(false);
		}
	}

	void Editor::Update(float deltaTime)
	{
		// First, so the UI drawn below already shows what the tools changed.
		if (m_McpTools)
		{
			m_McpTools->Update(*m_McpServer);
		}

		UpdateAssetRefresh();

		PlayState requestedState = m_PlayState;
		EditorRequests requests;
		const PanelRect viewportRect = m_EditorUI->Draw(requestedState, m_SelectedActor, m_Level, IsLevelDirty(), m_UndoHistory->CanUndo(), m_UndoHistory->CanRedo(), requests);
		// Closing the window asks about unsaved changes the same way File > Exit does.
		if (m_WindowModule->ConsumeCloseRequested(m_PrimaryWindow))
		{
			m_EditorUI->RequestExit(m_PlayState == PlayState::Editing && IsLevelDirty(), requests);
		}
		if (requestedState != m_PlayState)
		{
			SetPlayState(requestedState);
		}
		HandleRequests(requests);
		HandleLevelRequests(requests);
		if (requests.refreshAssets)
		{
			RefreshAssets();
		}

		if (m_PlayState == PlayState::Playing)
		{
			m_App.Update(deltaTime);
		}

		UpdateGrid();
		if (m_PlayState == PlayState::Editing)
		{
			DrawDebugOverlays();
		}
		if (m_EditorUI->GetViewSettings().showColliders)
		{
			DrawColliders(m_WorldManager->GetWorld(m_WorldManager->GetActiveWorld()));
		}
		if (m_EditorUI->GetViewSettings().showNoise)
		{
			DrawNoises(m_WorldManager->GetWorld(m_WorldManager->GetActiveWorld()));
		}
		if (m_EditorUI->GetViewSettings().showNavmesh)
		{
			DrawNavmesh(m_WorldManager->GetWorld(m_WorldManager->GetActiveWorld()));
		}
		if (m_EditorUI->GetViewSettings().showAgentPaths)
		{
			DrawAgentPaths(m_WorldManager->GetWorld(m_WorldManager->GetActiveWorld()));
		}
		if (m_EditorUI->GetViewSettings().showAIDebug)
		{
			DrawAIDebug(m_WorldManager->GetWorld(m_WorldManager->GetActiveWorld()));
		}

		const ViewSettings& viewSettings = m_EditorUI->GetViewSettings();
		EditorViewport::EditState editState;
		editState.editing = m_PlayState == PlayState::Editing;
		editState.selectedActor = m_SelectedActor;
		editState.snapToGrid = viewSettings.snapToGrid;
		editState.gridCellSize = m_Grid.cellSize;
		editState.showStats = viewSettings.showStats;

		// Shows whichever world is active - the game's while playing, the level's otherwise.
		World& activeWorld = m_WorldManager->GetWorld(m_WorldManager->GetActiveWorld());
		EditorViewport::Events events;
		m_EditorViewport->Draw(viewportRect, activeWorld, *m_InputManager, deltaTime, editState, events);

		if (m_PlayState == PlayState::Playing)
		{
			if (events.playClicked)
			{
				SetGameInputCaptured(true);
			}
			else if (m_GameInputCaptured && ImGui::IsKeyPressed(ImGuiKey_Escape, false))
			{
				SetGameInputCaptured(false);
			}
		}

		if (events.meshDropped)
		{
			AddMeshDrop(events.drop, "");
		}
		if (events.clicked)
		{
			m_SelectedActor = PickActor(events.clickRay);
		}
		if (events.transformChanged)
		{
			m_WorldManager->SetActorTransform(m_LevelEditorWorld, m_SelectedActor, events.transform);
		}
		ProcessPendingMeshDrops();
		CommitUndoWhenIdle();
	}

	void Editor::CommitUndoWhenIdle()
	{
		// A drag or a value being typed becomes one step once it's finished.
		if (m_PlayState == PlayState::Editing && !ImGui::IsAnyItemActive() && !m_EditorViewport->IsDragging())
		{
			m_UndoHistory->Commit();
		}
	}

	bool Editor::ApplyUndo(bool redo)
	{
		// Undo can make actors again, so the selection is found again by its ID.
		World& world = m_WorldManager->GetWorld(m_LevelEditorWorld);
		Id64 selectedID;
		bool hasSelection = false;
		for (const ActorInstance& actor : world.actorInstances)
		{
			if (actor.RootEntity() == m_SelectedActor)
			{
				selectedID = actor.id;
				hasSelection = true;
				break;
			}
		}

		if (!(redo ? m_UndoHistory->Redo() : m_UndoHistory->Undo()))
		{
			return false;
		}

		m_SelectedActor = c_InvalidEntity;
		for (const ActorInstance& actor : world.actorInstances)
		{
			if (hasSelection && actor.id == selectedID)
			{
				m_SelectedActor = actor.RootEntity();
				break;
			}
		}
		return true;
	}

	Entity Editor::PickActor(const ViewportRay& ray)
	{
		World& world = m_WorldManager->GetWorld(m_LevelEditorWorld);
		RayHit hit;
		Entity hitEntity;
		if (!RaycastActors(world, ray, hit, hitEntity))
		{
			return c_InvalidEntity;
		}

		// Any of an actor's entities selects the whole actor.
		for (const ActorInstance& actor : world.actorInstances)
		{
			for (Entity entity : actor.entities)
			{
				if (entity == hitEntity)
				{
					return actor.RootEntity();
				}
			}
		}
		return c_InvalidEntity;
	}

	void Editor::UpdateGrid()
	{
		const bool showGrid = m_EditorUI->GetViewSettings().showGrid;
		if (showGrid == m_Grid.enabled)
		{
			return;
		}

		m_Grid.enabled = showGrid;
		const World& levelWorld = m_WorldManager->GetWorld(m_LevelEditorWorld);
		m_RendererAPI->SetRenderViewportGrid(levelWorld.renderViewportHandle, m_Grid);
	}

	void Editor::DrawColliders(World& world) const
	{
		world.entities.ForEach<ColliderComponent>([&world, this](Entity entity, ColliderComponent& collider)
		{
			if (!world.entities.HasComponent<ComponentTransform>(entity))
			{
				return;
			}

			uint colour = DebugColour::c_Grey;
			if (world.entities.HasComponent<RigidBodyComponent>(entity))
			{
				const MotionType motionType = static_cast<MotionType>(world.entities.GetComponent<RigidBodyComponent>(entity).motionType);
				colour = motionType == MotionType::Dynamic ? DebugColour::c_Orange : (motionType == MotionType::Kinematic ? DebugColour::c_Cyan : colour);
			}

			// Sized the same way the physics bodies are.
			const Transform& transform = world.entities.GetComponent<ComponentTransform>(entity).world;
			const Vector3 scale(Math::Abs(transform.scale.x), Math::Abs(transform.scale.y), Math::Abs(transform.scale.z));
			const Vector3 center = transform.position + transform.rotation.Rotate(collider.center * transform.scale);
			switch (static_cast<ColliderShape>(collider.shape))
			{
			case ColliderShape::Sphere:
				DebugDraw::Sphere(center, collider.radius * std::max(scale.x, std::max(scale.y, scale.z)), colour);
				break;
			case ColliderShape::Capsule:
				DebugDraw::Capsule(center, collider.radius * std::max(scale.x, scale.z), collider.halfHeight * scale.y, transform.rotation, colour);
				break;
			case ColliderShape::Mesh:
			case ColliderShape::ConvexHull:
			{
				const CollisionMesh* collisionMesh = m_AssetManager->GetCollisionMesh(collider.collisionMesh);
				if (!collisionMesh)
				{
					break;
				}
				// Matches the shape PhysicsManager gives a dynamic body.
				const bool dynamic = colour == DebugColour::c_Orange;
				const Array<Vector3>& lines = static_cast<ColliderShape>(collider.shape) == ColliderShape::Mesh && !dynamic ? collisionMesh->triangleLines : collisionMesh->convexLines;
				const Matrix4 localToWorld = Matrix4::CreateTRS(transform.position, transform.rotation, transform.scale);
				for (uint i = 0; i + 1 < lines.Size(); i += 2)
				{
					DebugDraw::Line(localToWorld.MultiplyAffine(lines[i]), localToWorld.MultiplyAffine(lines[i + 1]), colour);
				}
				break;
			}
			default:
				DebugDraw::Box(center, collider.halfExtents * scale, transform.rotation, colour);
				break;
			}
		});

		world.entities.ForEach<CharacterComponent>([&world](Entity entity, CharacterComponent& character)
		{
			if (!world.entities.HasComponent<ComponentTransform>(entity))
			{
				return;
			}
			const float height = character.crouched ? character.crouchHeight : character.height;
			const Vector3 centre = world.entities.GetComponent<ComponentTransform>(entity).world.position + Vector3(0.0f, height * 0.5f, 0.0f);
			DebugDraw::Capsule(centre, character.radius, std::max(height * 0.5f - character.radius, 0.0f), Quaternion::c_Identity, DebugColour::c_Orange);
		});
	}

	void Editor::DrawDebugOverlays()
	{
		const ViewSettings& viewSettings = m_EditorUI->GetViewSettings();
		World& world = m_WorldManager->GetWorld(m_LevelEditorWorld);

		// Navmesh bounds show with the navmesh, and while selected.
		world.entities.ForEach<NavmeshBoundsComponent>([&world, &viewSettings, this](Entity entity, NavmeshBoundsComponent& bounds)
		{
			if ((!viewSettings.showNavmesh && entity != m_SelectedActor) || !world.entities.HasComponent<ComponentTransform>(entity))
			{
				return;
			}
			const Transform& transform = world.entities.GetComponent<ComponentTransform>(entity).world;
			DebugDraw::Box(transform.position, bounds.halfExtents * transform.scale, transform.rotation,
				entity == m_SelectedActor ? DebugColour::c_Yellow : DebugColour::c_Green);
		});

		if (viewSettings.showActorBounds)
		{
			world.entities.ForEach<BoxComponent>([&world, this](Entity entity, BoxComponent& box)
			{
				if (!world.entities.HasComponent<ComponentTransform>(entity))
				{
					return;
				}
				const Transform& transform = world.entities.GetComponent<ComponentTransform>(entity).world;
				const Vector3 center = transform.position + transform.rotation.Rotate(box.center * transform.scale);
				const uint colour = entity == m_SelectedActor ? DebugColour::c_Yellow : DebugColour::c_Cyan;
				DebugDraw::Box(center, box.halfExtents * transform.scale, transform.rotation, colour);
			});
		}

		if (viewSettings.showLightRanges)
		{
			world.entities.ForEach<PointLightComponent>([&world](Entity entity, PointLightComponent& light)
			{
				if (world.entities.HasComponent<ComponentTransform>(entity))
				{
					DebugDraw::Sphere(world.entities.GetComponent<ComponentTransform>(entity).world.position, light.range, DebugColour::c_Yellow);
				}
			});

			world.entities.ForEach<SpotLightComponent>([&world](Entity entity, SpotLightComponent& light)
			{
				if (!world.entities.HasComponent<ComponentTransform>(entity))
				{
					return;
				}
				// The edge is where the falloff, cos(angle) to the power of coneFalloff, drops to a tenth.
				const Transform& transform = world.entities.GetComponent<ComponentTransform>(entity).world;
				const float halfAngle = light.coneFalloff > 0.0f ? Math::Acos(Math::Pow(0.1f, 1.0f / light.coneFalloff)) : Math::c_HalfPi;
				DebugDraw::Cone(transform.position, transform.rotation.Rotate(Vector3::c_Forward), light.range, halfAngle, DebugColour::c_Yellow);
			});

			// A directional light has no position, so its direction is shown from its actor.
			world.entities.ForEach<DirLightComponent>([&world](Entity entity, DirLightComponent& light)
			{
				if (world.entities.HasComponent<ComponentTransform>(entity))
				{
					const Vector3 start = world.entities.GetComponent<ComponentTransform>(entity).world.position;
					DebugDraw::Arrow(start, start + Vector3::SafeNormalize(light.direction) * c_DirLightArrowLength, DebugColour::c_Orange);
				}
			});
		}

		// A decal's box is all there is to see of it before it has a material.
		world.entities.ForEach<DecalComponent>([&world, this](Entity entity, DecalComponent& /*decal*/)
		{
			if (!world.entities.HasComponent<ComponentTransform>(entity))
			{
				return;
			}
			const Transform& transform = world.entities.GetComponent<ComponentTransform>(entity).world;
			const uint colour = entity == m_SelectedActor ? DebugColour::c_Yellow : DebugColour::c_Green;
			DebugDraw::Box(transform.position, transform.scale * 0.5f, transform.rotation, colour);
			// Points the way it projects.
			const Vector3 down = transform.rotation.Rotate(-Vector3::c_Up) * (Math::Abs(transform.scale.y) * 0.5f);
			DebugDraw::Arrow(transform.position, transform.position + down, colour);
		});

		// A selected door shows how it opens, and a selected panel points at its door.
		if (m_SelectedActor != c_InvalidEntity && world.entities.HasComponent<ComponentTransform>(m_SelectedActor))
		{
			const Transform& transform = world.entities.GetComponent<ComponentTransform>(m_SelectedActor).world;
			if (world.entities.HasComponent<DoorComponent>(m_SelectedActor))
			{
				DrawDoorMotion(world, m_SelectedActor, transform);
			}
			if (world.entities.HasComponent<DoorPanelComponent>(m_SelectedActor))
			{
				const Entity door = Doors::Find(world, world.entities.GetComponent<DoorPanelComponent>(m_SelectedActor).door);
				if (door != c_InvalidEntity)
				{
					DebugDraw::Arrow(transform.position, world.entities.GetComponent<ComponentTransform>(door).world.position, DebugColour::c_Cyan);
				}
			}
			if (world.entities.HasComponent<LightSwitchComponent>(m_SelectedActor))
			{
				const CircuitName& circuit = world.entities.GetComponent<LightSwitchComponent>(m_SelectedActor).circuit;
				for (const ActorInstance& actor : world.actorInstances)
				{
					const Entity root = actor.RootEntity();
					if (world.entities.HasComponent<LightFixtureComponent>(root) && world.entities.GetComponent<LightFixtureComponent>(root).circuit == circuit)
					{
						DebugDraw::Arrow(transform.position, world.entities.GetComponent<ComponentTransform>(root).world.position, DebugColour::c_Yellow);
					}
				}
			}
		}

		// A player start shows the player's size and which way they'll face.
		for (const ActorInstance& actor : world.actorInstances)
		{
			const Entity root = actor.RootEntity();
			if (actor.typeID != c_PlayerStartType || !world.entities.HasComponent<ComponentTransform>(root))
			{
				continue;
			}
			const Transform& transform = world.entities.GetComponent<ComponentTransform>(root).world;
			const uint colour = root == m_SelectedActor ? DebugColour::c_Yellow : DebugColour::c_Cyan;
			const Vector3 centre = transform.position + Vector3(0.0f, 0.9f, 0.0f);
			DebugDraw::Capsule(centre, 0.3f, 0.6f, Quaternion::c_Identity, colour);
			DebugDraw::Arrow(centre, centre + transform.rotation.Rotate(Vector3::c_Forward) * 0.8f, colour);
		}

		if (viewSettings.showActorNames)
		{
			for (const ActorInstance& actor : world.actorInstances)
			{
				const Entity root = actor.RootEntity();
				if (world.entities.HasComponent<ComponentTransform>(root))
				{
					DebugDraw::Text(world.entities.GetComponent<ComponentTransform>(root).world.position, actor.name.CStr(), DebugColour::c_White);
				}
			}
		}
	}

	void Editor::DrawDoorMotion(const World& world, Entity entity, const Transform& transform)
	{
		const DoorComponent& door = world.entities.GetComponent<DoorComponent>(entity);
		Vector3 leafMin(0.0f, 0.0f, 0.0f);
		Vector3 leafMax(1.0f, 2.0f, 0.0f);
		if (world.entities.HasComponent<BoxComponent>(entity))
		{
			const BoxComponent& box = world.entities.GetComponent<BoxComponent>(entity);
			leafMin = box.center - box.halfExtents;
			leafMax = box.center + box.halfExtents;
		}

		if (static_cast<DoorMotion>(door.motion) == DoorMotion::Slide)
		{
			const Vector3 centre = transform.position + transform.rotation.Rotate((leafMin + leafMax) * 0.5f);
			DebugDraw::Arrow(centre, centre + transform.rotation.Rotate(door.slideOffset), DebugColour::c_Yellow);
			return;
		}

		// The far edge's path at the bottom, both ways when it opens away from whoever uses it.
		const float edgeX = Math::Abs(leafMin.x) > Math::Abs(leafMax.x) ? leafMin.x : leafMax.x;
		const Vector3 edge(edgeX, 0.0f, 0.0f);
		constexpr uint c_Segments = 12;
		const uint directions = door.opensAway ? 2 : 1;
		for (uint direction = 0; direction < directions; ++direction)
		{
			const float sign = direction == 0 ? 1.0f : -1.0f;
			Vector3 previous = transform.position + transform.rotation.Rotate(edge);
			for (uint i = 1; i <= c_Segments; ++i)
			{
				const float angle = door.openAngle * Math::c_DegToRad * sign * (float)i / (float)c_Segments;
				const Vector3 point = transform.position + transform.rotation.Rotate(Quaternion(Vector3::c_Up, angle).Rotate(edge));
				DebugDraw::Line(previous, point, DebugColour::c_Yellow);
				previous = point;
			}
			DebugDraw::Line(transform.position, previous, DebugColour::c_Yellow);
		}
	}

	void Editor::HandleRequests(const EditorRequests& requests)
	{
		if (m_PlayState != PlayState::Editing)
		{
			return;
		}

		if (requests.frameSelected)
		{
			FrameSelectedActor();
		}
		if (requests.buildNavmesh)
		{
			BuildNavmesh();
		}

		if (requests.undo)
		{
			ApplyUndo(false);
		}
		else if (requests.redo)
		{
			ApplyUndo(true);
		}

		if (requests.placeActor)
		{
			PlaceActor(requests.actorType, requests.folder.CStr());
		}

		// A mesh dropped onto a hierarchy folder goes where the camera is looking.
		if (requests.placeMesh)
		{
			EditorViewport::MeshDrop drop;
			drop.mesh = requests.mesh;
			drop.ray = GetCameraRay();
			AddMeshDrop(drop, requests.folder.CStr());
		}
	}

	void Editor::HandleLevelRequests(const EditorRequests& requests)
	{
		if (m_PlayState != PlayState::Editing)
		{
			if (requests.exit)
			{
				Platform::Exit(true);
			}
			return;
		}

		if (requests.levelDeleted)
		{
			m_Level = {};
			OpenStartLevel();
		}

		// Saving comes first, so choosing Save when asked about unsaved changes saves before exiting.
		if (requests.saveAll)
		{
			SaveAll();
		}
		else if (requests.saveLevel)
		{
			SaveLevel();
		}
		if (requests.saveLevelAs)
		{
			SaveLevelAs(requests.levelName.CStr());
		}
		if (requests.newLevel)
		{
			NewLevel(requests.levelName.CStr());
		}
		if (requests.openLevel)
		{
			OpenLevel(requests.level);
		}
		if (requests.setDefaultLevel)
		{
			SetDefaultLevel(requests.level);
		}
		if (requests.exit)
		{
			Platform::Exit(true);
		}
	}

	void Editor::AddMeshDrop(const EditorViewport::MeshDrop& drop, const char* folderPath)
	{
		if (m_PendingMeshDrops.Size() == c_MaxPendingMeshDrops)
		{
			TYR_LOG_WARNING("Too many meshes are still loading to place another.");
			return;
		}

		// Placing the actor needs the mesh's bounds, which come with its header.
		m_AssetManager->LoadMesh(drop.mesh);
		PendingMeshDrop& pending = m_PendingMeshDrops.ExpandOne();
		pending.drop = drop;
		pending.folder = folderPath;
		pending.entity = c_InvalidEntity;
	}

	void Editor::PlaceActor(const Id64& actorType, const char* folderPath)
	{
		const ActorTypeDesc* actorTypeDesc = ActorRegistry::Instance().FindActorType(actorType);
		if (!actorTypeDesc)
		{
			return;
		}

		// The renderer only supports so many of each type of light, and so many decals.
		if (!m_WorldManager->HasRoomForActorLights(m_LevelEditorWorld, *actorTypeDesc))
		{
			TYR_LOG_WARNING("Can't add a %s, the level already has as many of its lights or decals as the renderer supports.", actorTypeDesc->name.CStr());
			return;
		}

		World& world = m_WorldManager->GetWorld(m_LevelEditorWorld);
		const Entity rootEntity = AddActor(*actorTypeDesc, actorTypeDesc->name.CStr(), folderPath);
		if (rootEntity == c_InvalidEntity)
		{
			return;
		}

		if (world.entities.HasComponent<ComponentTransform>(rootEntity))
		{
			Transform transform = world.entities.GetComponent<ComponentTransform>(rootEntity).local;
			transform.position = GetBackgroundDropPosition(GetCameraRay());
			m_WorldManager->SetActorTransform(m_LevelEditorWorld, rootEntity, transform);
		}
		m_SelectedActor = rootEntity;
	}

	Entity Editor::AddActor(const ActorTypeDesc& actorType, const char* name, const char* folderPath)
	{
		World& world = m_WorldManager->GetWorld(m_LevelEditorWorld);
		const LocalArray<Entity, c_MaxActorTypeEntities> entities = ActorRegistry::Instance().InstantiateActor(actorType.typeId, world.entities);
		if (entities.IsEmpty())
		{
			return c_InvalidEntity;
		}

		const ActorName uniqueName = MakeUniqueActorName(world, name);
		m_WorldManager->AddActorInstance(m_LevelEditorWorld, WorldManager::CreateActorInstanceID(), actorType.typeId, uniqueName.CStr(), folderPath, entities);
		return entities[0];
	}

	Vector3 Editor::GetBackgroundDropPosition(const ViewportRay& ray) const
	{
		Vector3 position = ray.origin + ray.direction * c_BackgroundDropDistance;
		if (m_EditorUI->GetViewSettings().snapToGrid)
		{
			for (uint i = 0; i < 3; ++i)
			{
				position[i] = SnapToGrid(position[i], m_Grid.cellSize);
			}
		}
		return position;
	}

	void Editor::DrawNavmesh(const World& world)
	{
		// Only around the camera, since the whole level's would be more lines than can be drawn.
		constexpr float c_DrawRadius = 30.0f;
		m_NavmeshLines.Clear();
		m_WorldManager->GetNavmeshComponentsManager().AddDebugLines(world, world.camera->GetPosition(), c_DrawRadius, m_NavmeshLines);
		for (uint i = 0; i + 1 < m_NavmeshLines.Size(); i += 2)
		{
			DebugDraw::Line(m_NavmeshLines[i], m_NavmeshLines[i + 1], DebugColour::c_Cyan);
		}
	}

	void Editor::DrawAgentPaths(World& world)
	{
		const Vector3 lift(0.0f, 0.1f, 0.0f);
		world.entities.ForEach<NavAgentComponent>([&world, &lift](Entity entity, NavAgentComponent& agent)
		{
			if (!agent.hasTarget || !world.entities.HasComponent<ComponentTransform>(entity))
			{
				return;
			}
			Vector3 previous = world.entities.GetComponent<ComponentTransform>(entity).world.position + lift;
			for (uint i = agent.nextCorner; i < agent.path.points.Size(); ++i)
			{
				const Vector3 corner = agent.path.points[i] + lift;
				DebugDraw::Line(previous, corner, DebugColour::c_Orange);
				previous = corner;
			}
			DebugDraw::Sphere(agent.target + lift, 0.2f, agent.stuck ? DebugColour::c_Red : DebugColour::c_Yellow);
		});
	}

	void Editor::DrawAIDebug(World& world)
	{
		static const char* c_SenseNames[] = { "", "sight", "hearing", "smell", "touch" };
		world.entities.ForEach<EnemyComponent>([&world](Entity entity, EnemyComponent& enemy)
		{
			if (!world.entities.HasComponent<ComponentTransform>(entity) || !world.entities.HasComponent<PerceptionComponent>(entity))
			{
				return;
			}
			const PerceptionComponent& perception = world.entities.GetComponent<PerceptionComponent>(entity);
			const Vector3 head = world.entities.GetComponent<ComponentTransform>(entity).world.position + Vector3(0.0f, 2.0f, 0.0f);
			char label[DebugText::c_MaxLength + 1];
			if (perception.targetKnown)
			{
				snprintf(label, sizeof(label), "%s (%s, %.1fs)", EnemyBrains::GetStateName(enemy.state),
					c_SenseNames[std::min<uint>(perception.targetSense, 4)], perception.targetAge);
				DebugDraw::Line(head, perception.targetPosition + Vector3(0.0f, 0.1f, 0.0f), DebugColour::c_Red);
			}
			else
			{
				snprintf(label, sizeof(label), "%s", EnemyBrains::GetStateName(enemy.state));
			}
			DebugDraw::Text(head, label, DebugColour::c_White);
		});

		for (const ScentMarker& marker : world.scents)
		{
			const float freshness = 1.0f - std::min(marker.age / std::max(marker.lifetime, 0.001f), 1.0f);
			const uint colour = (DebugColour::c_Green & 0x00FFFFFFu) | (static_cast<uint>(freshness * 255.0f) << 24);
			const Vector3 point = marker.position + Vector3(0.0f, 0.05f, 0.0f);
			DebugDraw::Line(point - Vector3(0.1f, 0.0f, 0.0f), point + Vector3(0.1f, 0.0f, 0.0f), colour);
			DebugDraw::Line(point - Vector3(0.0f, 0.0f, 0.1f), point + Vector3(0.0f, 0.0f, 0.1f), colour);
		}
	}

	bool Editor::BuildNavmesh()
	{
		const bool built = m_WorldManager->GetNavmeshComponentsManager().Build(m_WorldManager->GetWorld(m_LevelEditorWorld), *m_AssetManager);
		TYR_LOG_INFO(built ? "Built the level's navmesh." : "The level has nothing to walk on, so it has no navmesh.");
		return built;
	}

	void Editor::DrawNoises(const World& world)
	{
		constexpr uint c_Segments = 32;
		for (const NoiseEvent& noise : world.noises)
		{
			// Brightest when made, and gone by the time it's forgotten.
			const float strength = 1.0f - noise.age / Noise::c_Lifetime;
			const uint alpha = static_cast<uint>(Math::Clamp(strength, 0.0f, 1.0f) * 255.0f);
			const uint colour = (DebugColour::c_Orange & 0x00FFFFFFu) | (alpha << 24);
			const Vector3 centre = noise.position + Vector3(0.0f, 0.05f, 0.0f);
			Vector3 previous = centre + Vector3(noise.radius, 0.0f, 0.0f);
			for (uint i = 1; i <= c_Segments; ++i)
			{
				const float angle = static_cast<float>(i) / c_Segments * Math::c_TwoPi;
				const Vector3 point = centre + Vector3(Math::Cos(angle) * noise.radius, 0.0f, Math::Sin(angle) * noise.radius);
				DebugDraw::Line(previous, point, colour);
				previous = point;
			}
		}
	}

	void Editor::FrameSelectedActor()
	{
		World& world = m_WorldManager->GetWorld(m_LevelEditorWorld);
		const ActorInstance* selected = nullptr;
		for (const ActorInstance& actor : world.actorInstances)
		{
			if (actor.RootEntity() == m_SelectedActor)
			{
				selected = &actor;
				break;
			}
		}
		if (!selected || !world.camera || !world.entities.HasComponent<ComponentTransform>(m_SelectedActor))
		{
			return;
		}

		// The box around every bounded entity's corners, or a small one at the actor's position.
		const Vector3 position = world.entities.GetComponent<ComponentTransform>(m_SelectedActor).world.position;
		Vector3 boundsMin = position - Vector3(0.5f, 0.5f, 0.5f);
		Vector3 boundsMax = position + Vector3(0.5f, 0.5f, 0.5f);
		bool hasBounds = false;
		for (Entity entity : selected->entities)
		{
			const bool hasBox = world.entities.HasComponent<BoxComponent>(entity);
			if ((!hasBox && !world.entities.HasComponent<NavmeshBoundsComponent>(entity)) || !world.entities.HasComponent<ComponentTransform>(entity))
			{
				continue;
			}
			BoxComponent box;
			if (hasBox)
			{
				box = world.entities.GetComponent<BoxComponent>(entity);
			}
			else
			{
				box.center = Vector3::c_Zero;
				box.halfExtents = world.entities.GetComponent<NavmeshBoundsComponent>(entity).halfExtents;
			}
			const Transform& transform = world.entities.GetComponent<ComponentTransform>(entity).world;
			for (uint corner = 0; corner < 8; ++corner)
			{
				const Vector3 offset((corner & 1) ? box.halfExtents.x : -box.halfExtents.x, (corner & 2) ? box.halfExtents.y : -box.halfExtents.y,
					(corner & 4) ? box.halfExtents.z : -box.halfExtents.z);
				const Vector3 point = transform.position + transform.rotation.Rotate((box.center + offset) * transform.scale);
				boundsMin = hasBounds ? Vector3(std::min(boundsMin.x, point.x), std::min(boundsMin.y, point.y), std::min(boundsMin.z, point.z)) : point;
				boundsMax = hasBounds ? Vector3(std::max(boundsMax.x, point.x), std::max(boundsMax.y, point.y), std::max(boundsMax.z, point.z)) : point;
				hasBounds = true;
			}
		}

		const Vector3 centre = (boundsMin + boundsMax) * 0.5f;
		const Vector3 extent = boundsMax - centre;
		const float radius = std::max(std::sqrt(extent.Dot(extent)), 0.25f);
		// The field of view is in degrees. A little extra distance leaves a margin around the actor.
		const float halfAngle = world.camera->GetFOV() * 0.5f * Math::c_Pi / 180.0f;
		const float distance = radius / std::sin(halfAngle) * 1.1f;
		world.camera->SetPosition(centre - world.camera->GetForward() * distance);
	}

	ViewportRay Editor::GetCameraRay() const
	{
		ViewportRay ray;
		ray.origin = m_Camera->GetPosition();
		ray.direction = m_Camera->GetForward();
		return ray;
	}

	void Editor::ProcessPendingMeshDrops()
	{
		World& world = m_WorldManager->GetWorld(m_LevelEditorWorld);
		for (uint i = m_PendingMeshDrops.Size(); i-- > 0;)
		{
			PendingMeshDrop& pending = m_PendingMeshDrops[i];
			if (pending.entity == c_InvalidEntity)
			{
				const MeshHeader* header = m_AssetManager->GetMeshHeader(pending.drop.mesh);
				if (!header)
				{
					continue;
				}
				pending.entity = SpawnMeshActor(pending, *header);
				m_SelectedActor = pending.entity;
			}

			// Released once the actor's mesh instance has its own reference, or the actor is gone.
			const bool requested = world.entities.HasComponent<MeshComponent>(pending.entity)
				? world.entities.GetComponent<MeshComponent>(pending.entity).meshInstanceRequested
				: world.entities.HasComponent<SkeletalMeshComponent>(pending.entity)
					&& world.entities.GetComponent<SkeletalMeshComponent>(pending.entity).meshInstanceRequested;
			const bool removed = !world.entities.HasComponent<MeshComponent>(pending.entity) && !world.entities.HasComponent<SkeletalMeshComponent>(pending.entity);
			if (removed || requested)
			{
				m_AssetManager->DeleteMesh(pending.drop.mesh);
				m_PendingMeshDrops.SwapAndPopBack(i);
			}
		}
	}

	Entity Editor::SpawnMeshActor(const PendingMeshDrop& pending, const MeshHeader& header)
	{
		World& world = m_WorldManager->GetWorld(m_LevelEditorWorld);
		const bool snap = m_EditorUI->GetViewSettings().snapToGrid;
		const float cellSize = m_Grid.cellSize;
		const EditorViewport::MeshDrop& drop = pending.drop;
		const ViewportRay& ray = drop.ray;

		RayHit nearestHit;
		Entity hitEntity;
		Vector3 position;
		if (RaycastActors(world, ray, nearestHit, hitEntity))
		{
			// Flush against the face that was hit, sliding along it in grid steps.
			position = ray.origin + ray.direction * nearestHit.distance;
			for (uint i = 0; i < 3; ++i)
			{
				if (snap && i != nearestHit.axis)
				{
					position[i] = SnapToGrid(position[i], cellSize);
				}
			}
			const uint axis = nearestHit.axis;
			position[axis] -= nearestHit.normalSign > 0.0f ? header.aabbMin[axis] : header.aabbMax[axis];
		}
		else
		{
			position = GetBackgroundDropPosition(ray);
		}

		const bool skinned = header.IsSkinned();
		const ActorTypeDesc* actorType = ActorRegistry::Instance().FindActorType(Id64(skinned ? "SkeletalMeshActor" : "StaticMeshActor"));
		if (!actorType)
		{
			return c_InvalidEntity;
		}

		// Named after the mesh file.
		const char* meshPath = AssetRegistry::Instance().GetAssetData(drop.mesh).filePath.CStr();
		char fileName[PathConstants::c_MaxFileNameTotalSize];
		PathUtil::GetFileNameWithoutExtension(meshPath, fileName);
		const Entity rootEntity = AddActor(*actorType, fileName, pending.folder.CStr());
		if (rootEntity == c_InvalidEntity)
		{
			return c_InvalidEntity;
		}

		if (skinned)
		{
			world.entities.GetComponent<SkeletalMeshComponent>(rootEntity).mesh = drop.mesh;
			// Starts playing the first clip made for its skeleton, if there is one.
			world.entities.GetComponent<AnimatorComponent>(rootEntity).clip = FindFirstAnimation(header.skeleton);
		}
		else
		{
			world.entities.GetComponent<MeshComponent>(rootEntity).mesh = drop.mesh;
		}
		BoxComponent& box = world.entities.GetComponent<BoxComponent>(rootEntity);
		box.center = (header.aabbMin + header.aabbMax) * 0.5f;
		box.halfExtents = (header.aabbMax - header.aabbMin) * 0.5f;

		Transform transform = world.entities.GetComponent<ComponentTransform>(rootEntity).local;
		transform.position = position;
		m_WorldManager->SetActorTransform(m_LevelEditorWorld, rootEntity, transform);
		return rootEntity;
	}

	ActorName Editor::MakeUniqueActorName(const World& world, const char* baseName)
	{
		// Like Unreal, "Cube", then "Cube2", "Cube3" and so on, cut short to fit.
		char name[ActorName::c_Capacity];
		for (uint number = 1;; ++number)
		{
			char suffix[12] = {};
			if (number > 1)
			{
				snprintf(suffix, sizeof(suffix), "%u", number);
			}
			const size_t suffixLength = strlen(suffix);
			const size_t baseLength = std::min(strlen(baseName), c_MaxActorName - suffixLength);
			snprintf(name, sizeof(name), "%.*s%s", static_cast<int>(baseLength), baseName, suffix);

			bool taken = false;
			for (const ActorInstance& actor : world.actorInstances)
			{
				taken |= actor.name == name;
			}
			if (!taken)
			{
				return ActorName(name);
			}
		}
	}

	void Editor::SetPlayState(PlayState state)
	{
		if (m_PlayState == PlayState::Editing)
		{
			// The game loads the level from its file, so it's saved first.
			if (IsLevelDirty())
			{
				SaveLevel();
			}
			m_App.SetStartLevel(AssetUtil::IsValidAssetID(m_Level) ? AssetRegistry::Instance().GetAssetData(m_Level).filePath.CStr() : "");
			// Without a player start, the player stands where the editor's camera is, facing the same way.
			const Vector3 cameraForward = m_Camera->GetForward();
			m_App.SetFallbackStart(m_Camera->GetPosition() - Vector3(0.0f, c_FallbackEyeHeight, 0.0f), Math::Atan2(cameraForward.x, cameraForward.z));
			m_EditorUI->ApplyRenderSettings(false);
			m_App.Initialize();
		}
		else if (state == PlayState::Editing)
		{
			m_App.Shutdown();
			m_WorldManager->SetActiveWorld(m_LevelEditorWorld);
			m_EditorUI->ApplyRenderSettings(true);
		}
		m_PlayState = state;

		// The game's world is the active one while it runs, and holds still while paused.
		if (state != PlayState::Editing)
		{
			m_WorldManager->GetWorld(m_WorldManager->GetActiveWorld()).paused = state == PlayState::Paused;
		}
		// Like Unreal, the game takes the mouse as soon as it starts.
		SetGameInputCaptured(state == PlayState::Playing);
	}

	void Editor::SetGameInputCaptured(bool captured)
	{
		m_GameInputCaptured = captured;
		m_InputManager->SetGameInputEnabled(captured);
		m_InputManager->SetUIInputBlocked(captured);
		m_InputManager->SetMouseCaptured(captured);
	}

	void Editor::Shutdown()
	{
		// Before anything its tools use goes away.
		m_McpTools.reset();
		m_McpServer.reset();

		if (m_PlayState != PlayState::Editing)
		{
			SetPlayState(PlayState::Editing);
		}

		for (const PendingMeshDrop& pending : m_PendingMeshDrops)
		{
			m_AssetManager->DeleteMesh(pending.drop.mesh);
		}
		m_PendingMeshDrops.Clear();

		// Destroyed here, while the asset manager the UI releases icons back to still exists. The
		// viewport goes first since it uses the UI's icons.
		m_UndoHistory.reset();
		m_EditorViewport.reset();
		m_EditorUI.reset();

		DestroyPrimaryWorld(*m_WindowModule, *m_WorldManager, m_LevelEditorWorld, m_PrimaryWindow);
		m_LevelEditorWorld = {};
		m_WorldManager = nullptr;
	}

	bool Editor::WantsExit() const
	{
		// TODO: Handle headless case
		return m_WindowModule->IsWindowActive(m_PrimaryWindow);
	}
}