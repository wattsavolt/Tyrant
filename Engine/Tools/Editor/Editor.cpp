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

		m_LevelEditorWorld = CreatePrimaryWorld(*m_WindowModule, *m_WorldManager, desc, worldParams, m_PrimaryWindow);
		m_GUIModule->SetPrimaryWindow(m_WindowModule, m_PrimaryWindow);

		InputModule* inputModule;
		TYR_GET_MODULE(InputModule, inputModule);
		m_InputManager = inputModule->GetInputManager();
		m_InputManager->SetWindow(m_WindowModule, m_PrimaryWindow);

		// Before the test cube below, which needs it imported.
		ImportDefaultAssetsIfNeeded();

		m_EditorViewport = MakeURef<EditorViewport>(*m_RendererAPI);
		m_EditorUI = MakeURef<EditorUI>(*m_GUIModule, *m_RendererAPI, *m_AssetManager);

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
		AssetID defaultMaterialID;
		if (!MaterialImporter::Instance().ImportPbrMaterial(defaultMaterialDesc, defaultMaterialID))
		{
			TYR_LOG_ERROR("Failed to import the default material from SourceAssets.");
			return;
		}

		EditorIcons::Import();

		char sourcePath[TYR_MAX_PATH_TOTAL_SIZE];
		AssetUtil::CreateFullSourceAssetPath(sourcePath, "Cube/Cube.glb");
		if (!ModelImporter::Instance().ImportModel(sourcePath, "Models/Cube", "Cube"))
		{
			TYR_LOG_ERROR("Failed to import the test cube from SourceAssets.");
			return;
		}

		// Cleared so this doesn't repeat on every subsequent startup.
		editorConfig.SetValueAsBool("ImportDefaultAssets", false);
		editorConfig.Save();
	}

	void Editor::Update(float deltaTime)
	{
		PlayState requestedState = m_PlayState;
		const PanelRect viewportRect = m_EditorUI->Draw(requestedState);
		if (requestedState != m_PlayState)
		{
			SetPlayState(requestedState);
		}

		if (m_PlayState == PlayState::Playing)
		{
			m_App.Update(deltaTime);
		}

		UpdateGrid();

		// Shows whichever world is active - the game's while playing, the level's otherwise.
		const World& activeWorld = m_WorldManager->GetWorld(m_WorldManager->GetActiveWorld());
		EditorViewport::MeshDrop drop;
		if (m_EditorViewport->Draw(viewportRect, activeWorld, *m_InputManager, m_PlayState == PlayState::Editing, deltaTime, drop))
		{
			AddMeshDrop(drop);
		}
		ProcessPendingMeshDrops();
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

	void Editor::AddMeshDrop(const EditorViewport::MeshDrop& drop)
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
		pending.entity = c_InvalidEntity;
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
				pending.entity = SpawnStaticMeshActor(pending.drop, *header);
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

	Entity Editor::SpawnStaticMeshActor(const EditorViewport::MeshDrop& drop, const MeshHeader& header)
	{
		World& world = m_WorldManager->GetWorld(m_LevelEditorWorld);
		const bool snap = m_EditorUI->GetViewSettings().snapToGrid;
		const float cellSize = m_Grid.cellSize;

		// The nearest existing actor the ray hits.
		RayHit nearestHit;
		nearestHit.distance = FLT_MAX;
		bool hasHit = false;
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
			if (IntersectBox(drop.rayOrigin, drop.rayDirection, boxMin, boxMax, hit) && hit.distance < nearestHit.distance)
			{
				nearestHit = hit;
				hasHit = true;
			}
		});

		Vector3 position;
		if (hasHit)
		{
			// Flush against the face that was hit, sliding along it in grid steps.
			position = drop.rayOrigin + drop.rayDirection * nearestHit.distance;
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
			// Like Unreal, a drop onto nothing goes a set distance in front of the camera.
			position = drop.rayOrigin + drop.rayDirection * c_BackgroundDropDistance;
			for (uint i = 0; snap && i < 3; ++i)
			{
				position[i] = SnapToGrid(position[i], cellSize);
			}
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

		// Named after the mesh file, cut short if it doesn't fit.
		const char* meshPath = AssetRegistry::Instance().GetAssetData(drop.mesh).filePath.CStr();
		char fileName[PathConstants::c_MaxFileNameTotalSize];
		PathUtil::GetFileNameWithoutExtension(meshPath, fileName);
		fileName[std::min<size_t>(strlen(fileName), NameConstants::c_MaxName)] = '\0';

		m_WorldManager->AddActorInstance(m_LevelEditorWorld, fileName, "", entities);
		return entities[0];
	}

	void Editor::SetPlayState(PlayState state)
	{
		if (m_PlayState == PlayState::Editing)
		{
			m_App.Initialize();
		}
		else if (state == PlayState::Editing)
		{
			m_App.Shutdown();
			m_WorldManager->SetActiveWorld(m_LevelEditorWorld);
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

		// Destroyed here, while the asset manager it releases icons back to still exists.
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