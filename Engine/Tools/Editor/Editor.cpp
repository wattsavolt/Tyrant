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
#include "Actor/ActorUtil.h"
#include "Actor/ActorRegistry.h"
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
		m_GUIModule->SetPrimaryWindow(m_WindowModule, m_PrimaryWindow);

		InputModule* inputModule;
		TYR_GET_MODULE(InputModule, inputModule);
		m_InputManager = inputModule->GetInputManager();
		m_InputManager->SetWindow(m_WindowModule, m_PrimaryWindow);

		// Before the UI, which loads the editor icons it imports.
		ImportDefaultAssetsIfNeeded();

		m_EditorUI = MakeURef<EditorUI>(*m_GUIModule, *m_RendererAPI, *m_AssetManager, *m_WorldManager, m_LevelEditorWorld);
		m_EditorViewport = MakeURef<EditorViewport>(*m_RendererAPI, m_EditorUI->GetIcons());
		m_EditorUI->ApplyRenderSettings(true);

		// TODO: Temporary test light until lights can be placed from the editor.
		{
			World& world = m_WorldManager->GetWorld(m_LevelEditorWorld);
			Entity lightEntity = world.entities.CreateEntity();

			DirLightComponent lightComponent;
			// The direction the light travels, from the light towards the scene.
			lightComponent.direction = Vector3::Normalize(Vector3(-0.4f, -0.8f, 0.4f));
			lightComponent.colour = Vector3::c_One;
			lightComponent.intensity = 3.0f;
			lightComponent.castsShadow = false;
			world.entities.AddComponent<DirLightComponent>(lightEntity, lightComponent);
		}
	}

	void Editor::ImportDefaultAssetsIfNeeded()
	{
		char configPath[TYR_MAX_PATH_TOTAL_SIZE];
		AssetUtil::CreateFullConfigPath(configPath, "EditorConfig.ini");

		Config editorConfig(configPath);
		const bool shouldImport = !editorConfig.HasValue("ImportDefaultAssets") || editorConfig.GetValueAsBool("ImportDefaultAssets");
		if (!shouldImport)
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
			editorConfig.SetValueAsBool("ImportDefaultAssets", false);
			editorConfig.Save();
		}
	}

	void Editor::Update(float deltaTime)
	{
		PlayState requestedState = m_PlayState;
		EditorRequests requests;
		const PanelRect viewportRect = m_EditorUI->Draw(requestedState, m_SelectedActor, requests);
		if (requestedState != m_PlayState)
		{
			SetPlayState(requestedState);
		}
		HandleRequests(requests);

		if (m_PlayState == PlayState::Playing)
		{
			m_App.Update(deltaTime);
		}

		UpdateGrid();

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
		const LocalArray<Entity, c_MaxActorTypeEntities> entities = ActorRegistry::Instance().InstantiateActor(actorType, world.entities);
		if (entities.IsEmpty())
		{
			return;
		}

		const Name name = MakeUniqueActorName(world, actorTypeDesc->name.CStr());
		m_WorldManager->AddActorInstance(m_LevelEditorWorld, name.CStr(), folderPath, entities);

		const Entity rootEntity = entities[0];
		if (world.entities.HasComponent<ComponentTransform>(rootEntity))
		{
			Transform transform = world.entities.GetComponent<ComponentTransform>(rootEntity).local;
			transform.position = GetBackgroundDropPosition(GetCameraRay());
			m_WorldManager->SetActorTransform(m_LevelEditorWorld, rootEntity, transform);
		}
		m_SelectedActor = rootEntity;
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

		StaticMeshActorMeshDesc desc;
		desc.mesh = drop.mesh;
		desc.localTransform.position = position;
		desc.localTransform.rotation = Quaternion::c_Identity;
		desc.localTransform.scale = Vector3::c_One;
		desc.aabbMin = header.aabbMin;
		desc.aabbMax = header.aabbMax;

		LocalArray<Entity, c_MaxActorInstanceEntities> entities;
		entities.Add(ActorUtil::BuildStaticMeshActor(world.entities, desc));

		// Named after the mesh file.
		const char* meshPath = AssetRegistry::Instance().GetAssetData(drop.mesh).filePath.CStr();
		char fileName[PathConstants::c_MaxFileNameTotalSize];
		PathUtil::GetFileNameWithoutExtension(meshPath, fileName);
		const Name name = MakeUniqueActorName(world, fileName);

		m_WorldManager->AddActorInstance(m_LevelEditorWorld, name.CStr(), pending.folder.CStr(), entities);
		return entities[0];
	}

	Name Editor::MakeUniqueActorName(const World& world, const char* baseName)
	{
		// Like Unreal, "Cube", then "Cube2", "Cube3" and so on, cut short to fit.
		char name[Name::c_Capacity];
		for (uint number = 1;; ++number)
		{
			char suffix[12] = {};
			if (number > 1)
			{
				snprintf(suffix, sizeof(suffix), "%u", number);
			}
			const size_t suffixLength = strlen(suffix);
			const size_t baseLength = std::min(strlen(baseName), NameConstants::c_MaxName - suffixLength);
			snprintf(name, sizeof(name), "%.*s%s", static_cast<int>(baseLength), baseName, suffix);

			bool taken = false;
			for (const ActorInstance& actor : world.actorInstances)
			{
				taken |= actor.name == name;
			}
			if (!taken)
			{
				return Name(name);
			}
		}
	}

	void Editor::SetPlayState(PlayState state)
	{
		if (m_PlayState == PlayState::Editing)
		{
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