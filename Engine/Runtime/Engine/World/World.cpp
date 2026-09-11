#include "World.h"
#include "Camera.h"
#include "Math/Quaternion.h"
#include "RendererModule.h"
#include "RenderAPI/Device.h"
#include "Rendering/RenderConstants.h"
#include "Rendering/RendererAPI.h"
#include "AssetSystem/AssetModule.h"
#include "AssetSystem/AssetRegistry.h"
#include "AssetSystem/AssetManager.h"

// TODO: Remove below includes when finishing testing as shouldn't be needed here
#include "RenderTransfer/UploadRequest.h"
#include "RenderResource/MeshDesc.h"
#include "RenderResource/Shapes.h"
#include "RenderInstance/RenderInstanceDescs.h"
#include "Shaders/ShaderTypes.h"
/////////////////////////////////////////////////////////////////////////////////

namespace tyr
{
	World::World()
		: m_SceneIndex(RenderConstants::c_MaxScenes)
		, m_Camera(nullptr)
		, m_Active(true)
		, m_Visible(true)
		, m_Initialized(false)
	{

	}

	World::~World()
	{
		
	}

	// TODO : Remove this code. Just here for testing 
	bool firstFrame = true;
	SpotLightHandle spotLight;
	MeshHandle cube;
	MeshInstanceHandle cubeInstance;
	/// 

	void World::Initialize(const WorldConfig& config)
	{
		TYR_ASSERT(!m_Initialized);

		m_Name = config.name;
		m_Camera = config.camera;
		m_ViewArea = config.viewArea;

		m_SceneIndex = m_RendererAPI->AddScene(m_Name.CStr());

		RendererModule* rendererModule;
		TYR_GET_MODULE(RendererModule, rendererModule);

		m_Device = rendererModule->GetDevice();

		m_RendererAPI = rendererModule->GetRendererAPI();

		m_Initialized = true;
	}

	void World::Shutdown()
	{
		TYR_ASSERT(m_Initialized);

		// TODO : Remove this code. Just here temporarily for testing 
		if (!firstFrame)
		{
			m_RendererAPI->DeleteMeshInstance(cubeInstance);

			m_RendererAPI->DeleteSpotLight(spotLight);

			m_RendererAPI->DeleteMesh(cube);

			AssetModule* assetModule;
			TYR_GET_MODULE(AssetModule, assetModule);
			AssetManager* assetManager = assetModule->GetAssetManager();
			assetManager->DeleteMaterial(assetManager->GetDefaultMaterialAssetID());
		}
		///

		m_RendererAPI->RemoveScene(m_SceneIndex);

		m_Initialized = false;
	}

	void World::Update(float deltaTime)
	{
		if (!m_Active)
		{
			return;
		}

		m_RendererAPI->SetActiveSceneIndex(m_SceneIndex, m_Visible);

		SceneView view;
		view.viewArea = m_ViewArea;
		view.camera.position = m_Camera->GetPosition();
		view.camera.forward = m_Camera->GetForward();
		view.camera.up = m_Camera->GetUp();
		view.camera.fov = m_Camera->GetFOV();
		view.camera.nearZ = m_Camera->GetNearZ();
		view.camera.farZ = m_Camera->GetFarZ();

		m_RendererAPI->AddView(view);

		// TODO : Remove this code. Just here for testing 
		if (firstFrame)
		{
			{
				SpotLightDesc desc{};
				SpotLightInfo& info = desc.info;
				info.position = { 0, 5, 10 };
				info.range = 3.0f;
				info.cone = 1.0f;
				info.direction = { 0, -1, 0 };
				info.attenuation = { 1, 1, 1 };
				info.colour = { 1, 1, 1 };
				info.castsShadow = true;

				spotLight = m_RendererAPI->CreateSpotLight(desc);
			}

			{
				MeshDesc desc{};
				desc.sphere.SetCentre(Vector3(0, 0, 0));
				desc.sphere.SetRadius(0.866f);

				desc.aabbMin = Vector3(-0.5f, -0.5f, -0.5f);
				desc.aabbMax = Vector3(0.5f, 0.5f, 0.5f);

				desc.lodCount = 1;

				cube = m_RendererAPI->CreateMesh(desc);

				{
					const size_t size = Cube::c_NumVertices * MeshConstants::c_ShaderVertexSize;

					UploadBufferAllocation uploadAlloc;
					m_RendererAPI->RequestResourceUploadAllocation(size, uploadAlloc);

					memcpy(static_cast<uint8*>(uploadAlloc.cpuPtr) + uploadAlloc.offset, Cube::c_UnitCube.GetShaderVertices(), size);
					m_RendererAPI->FlushBufferUploadAllocation(uploadAlloc);

					GpuBufferAllocation gpuAlloc;
					m_RendererAPI->RequestVertexBufferAllocation(cube, 0, size, gpuAlloc);

					BufferUploadRequest req;
					req.srcBuffer = uploadAlloc.buffer;
					req.srcOffset = uploadAlloc.offset;
					req.size = size;
					req.dstBuffer = gpuAlloc.buffer;
					req.dstOffset = gpuAlloc.offset;

					m_RendererAPI->AddBufferUploadRequest(req);
				}

				{
					const size_t size = Cube::c_NumIndices * MeshConstants::c_ShaderIndexSize;

					UploadBufferAllocation uploadAlloc;
					m_RendererAPI->RequestResourceUploadAllocation(size, uploadAlloc);

					memcpy(static_cast<uint8*>(uploadAlloc.cpuPtr) + uploadAlloc.offset, Cube::c_Indices, size);
					m_RendererAPI->FlushBufferUploadAllocation(uploadAlloc);

					GpuBufferAllocation gpuAlloc;
					m_RendererAPI->RequestIndexBufferAllocation(cube, 0, size, gpuAlloc);

					BufferUploadRequest req;
					req.srcBuffer = uploadAlloc.buffer;
					req.srcOffset = uploadAlloc.offset;
					req.size = size;
					req.dstBuffer = gpuAlloc.buffer;
					req.dstOffset = gpuAlloc.offset;

					m_RendererAPI->AddBufferUploadRequest(req);
				}

				{
					const size_t size = MeshConstants::c_ShaderMeshletSize;

					UploadBufferAllocation uploadAlloc;
					m_RendererAPI->RequestResourceUploadAllocation(size, uploadAlloc);

					ShaderMeshlet meshlet;
					meshlet.vertexOffset = 0;
					meshlet.vertexCount = Cube::c_NumVertices;
					meshlet.indexOffset = 0;
					meshlet.indexCount = Cube::c_NumIndices;

					memcpy(static_cast<uint8*>(uploadAlloc.cpuPtr) + uploadAlloc.offset, &meshlet, size);
					m_RendererAPI->FlushBufferUploadAllocation(uploadAlloc);

					GpuBufferAllocation gpuAlloc;
					m_RendererAPI->RequestMeshletBufferAllocation(cube, 0, size, gpuAlloc);

					BufferUploadRequest req;
					req.srcBuffer = uploadAlloc.buffer;
					req.srcOffset = uploadAlloc.offset;
					req.size = size;
					req.dstBuffer = gpuAlloc.buffer;
					req.dstOffset = gpuAlloc.offset;

					m_RendererAPI->AddBufferUploadRequest(req);
				}
			}

			{
				AssetModule* assetModule;
				TYR_GET_MODULE(AssetModule, assetModule);
				AssetManager* assetManager = assetModule->GetAssetManager();
				assetManager->LoadMaterial(assetManager->GetDefaultMaterialAssetID());
				MeshInstanceDesc desc{};
				MeshInstanceInfo& info = desc.info;
				info.transform = Matrix4::CreateTRS({ 0,0,15 }, Quaternion::c_Identity, { 3,3,3 });
				info.mesh = cube;
				info.material = MaterialHandle(assetManager->GetDefaultMaterial());
				cubeInstance = m_RendererAPI->CreateMeshInstance(desc);
			}

			firstFrame = false;
		}
	}

	Camera* World::GetCamera() const
	{ 
		return m_Camera; 
	}

	void World::SetCamera(Camera* camera)
	{
		m_Camera = camera;
	}

	void World::SetActive(bool active)
	{
		m_Active = active;
	}

	void World::SetVisible(bool visible)
	{
		m_Visible = visible;
	}
}

