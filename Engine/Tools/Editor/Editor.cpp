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
#include "AssetSystem/MeshAsset.h"
#include "Utility/PathUtil.h"
#include "Math/Math.h"
#include "Math/Matrix3.h"
#include <algorithm>
#include <cfloat>
#include <cstring>
#include <utility>

namespace tyr
{
	namespace
	{
		// How far in front of the camera a mesh dropped onto nothing is placed.
		constexpr float c_BackgroundDropDistance = 5.0f;
		// The level made when there aren't any yet.
		constexpr const char* c_FirstLevelName = "Main";
		// How long a directional light's direction is drawn, in metres.
		constexpr float c_DirLightArrowLength = 2.0f;

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
		m_Camera = MakeURef<Camera>(Vector3(0.0f, 10.0f, -10.0f), Vector3::c_Up, Vector3::c_Forward, 90.0f, 1.0f, 2000.0f);
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

		m_Settings = MakeURef<EditorSettings>();

		// Before the UI, which loads the editor icons it imports.
		ImportDefaultAssetsIfNeeded();

		m_EditorUI = MakeURef<EditorUI>(*m_GUIModule, *m_RendererAPI, *m_AssetManager, *m_WorldManager, m_LevelEditorWorld, *m_Settings);
		m_EditorViewport = MakeURef<EditorViewport>(*m_RendererAPI, m_EditorUI->GetIcons());
		m_EditorUI->ApplyRenderSettings(true);

		OpenStartLevel();
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

		for (const std::pair<const AssetID&, const RegAssetData&> asset : registry.GetAssets())
		{
			if (HasExtension(asset.second.filePath.CStr(), AssetConstants::c_LevelFileExtension))
			{
				OpenLevel(asset.first);
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
	}

	void Editor::OpenLevel(AssetID level)
	{
		m_WorldManager->ClearWorld(m_LevelEditorWorld);
		m_SelectedActor = c_InvalidEntity;

		const char* levelPath = AssetRegistry::Instance().GetAssetData(level).filePath.CStr();
		LevelFile::Load(levelPath, *m_WorldManager, m_LevelEditorWorld);
		m_Level = level;
		m_SavedChangeCount = m_WorldManager->GetWorld(m_LevelEditorWorld).changeCount;
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
		PlayState requestedState = m_PlayState;
		EditorRequests requests;
		const PanelRect viewportRect = m_EditorUI->Draw(requestedState, m_SelectedActor, m_Level, IsLevelDirty(), requests);
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

		if (m_PlayState == PlayState::Playing)
		{
			m_App.Update(deltaTime);
		}

		UpdateGrid();
		if (m_PlayState == PlayState::Editing)
		{
			DrawDebugOverlays();
		}

		const ViewSettings& viewSettings = m_EditorUI->GetViewSettings();
		EditorViewport::EditState editState;
		editState.editing = m_PlayState == PlayState::Editing;
		editState.selectedActor = m_SelectedActor;
		editState.snapToGrid = viewSettings.snapToGrid;
		editState.gridCellSize = m_Grid.cellSize;

		// Shows whichever world is active - the game's while playing, the level's otherwise.
		World& activeWorld = m_WorldManager->GetWorld(m_WorldManager->GetActiveWorld());
		EditorViewport::Events events;
		m_EditorViewport->Draw(viewportRect, activeWorld, *m_InputManager, deltaTime, editState, events);

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

	void Editor::DrawDebugOverlays()
	{
		const ViewSettings& viewSettings = m_EditorUI->GetViewSettings();
		World& world = m_WorldManager->GetWorld(m_LevelEditorWorld);

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

	void Editor::HandleRequests(const EditorRequests& requests)
	{
		if (m_PlayState != PlayState::Editing)
		{
			return;
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

		// The renderer only supports so many of each type of light.
		if (!m_WorldManager->HasRoomForActorLights(m_LevelEditorWorld, *actorTypeDesc))
		{
			TYR_LOG_WARNING("Can't add a %s, the level already has as many of its lights as the renderer supports.", actorTypeDesc->name.CStr());
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
				pending.entity = SpawnStaticMeshActor(pending, *header);
				m_SelectedActor = pending.entity;
			}

			// Released once the actor's mesh instance has its own reference, or the actor is gone.
			if (!world.entities.HasComponent<MeshComponent>(pending.entity)
				|| world.entities.GetComponent<MeshComponent>(pending.entity).meshInstanceRequested)
			{
				m_AssetManager->DeleteMesh(pending.drop.mesh);
				m_PendingMeshDrops.SwapAndPopBack(i);
			}
		}
	}

	Entity Editor::SpawnStaticMeshActor(const PendingMeshDrop& pending, const MeshHeader& header)
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

		const ActorTypeDesc* actorType = ActorRegistry::Instance().FindActorType(Id64("StaticMeshActor"));
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

		world.entities.GetComponent<MeshComponent>(rootEntity).mesh = drop.mesh;
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
	}

	void Editor::Shutdown()
	{
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