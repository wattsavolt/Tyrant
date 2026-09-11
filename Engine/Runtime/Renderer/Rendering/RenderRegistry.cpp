#include "RenderRegistry.h"
#include "RenderAPI/RenderAPI.h"
#include "RenderAPI/Device.h"
#include "RenderAPI/Buffer.h"
#include "RenderAPI/Image.h"
#include "Shaders/ShaderTypes.h"
#include "RenderResource/RenderResourceUtil.h"

namespace tyr
{
	RenderRegistry* RenderRegistry::s_Instance = nullptr;

	RenderRegistry::RenderRegistry(Device& device)
		: m_Device(device)
	{
		TYR_ASSERT(s_Instance == nullptr);
		s_Instance = this;	
	}

	RenderRegistry::~RenderRegistry()
	{
		s_Instance = nullptr;
	}

	RenderRegistry* RenderRegistry::Instance()
	{
		TYR_ASSERT(s_Instance != nullptr);
		return s_Instance;
	}

	RenderBufferHandle RenderRegistry::CreateBuffer(const RenderBufferDesc& desc)
	{
		const RenderBufferHandle handle(m_BufferPool.Create());
		RenderResourceUtil::InitializeRenderBuffer(m_BufferPool[handle.h], m_Device, desc);
		return handle;
	}

	void RenderRegistry::DeleteBuffer(RenderBufferHandle handle)
	{
		RenderResourceUtil::DeinitializeRenderBuffer(m_BufferPool[handle.h], m_Device);
		m_BufferPool.Delete(handle.h);
	}

	TextureHandle RenderRegistry::CreateTexture(const TextureDesc& desc)
	{
		const TextureHandle handle(m_TexturePool.Create());
		RenderResourceUtil::InitializeTexture(m_TexturePool[handle.h], m_Device, desc);
		return handle;
	}

	void RenderRegistry::DeleteTexture(TextureHandle handle)
	{
		RenderResourceUtil::DeinitializeTexture(m_TexturePool[handle.h], m_Device);
		m_TexturePool.Delete(handle.h);
	}

	MaterialHandle RenderRegistry::CreateMaterial(const MaterialDesc& desc)
	{
		const MaterialHandle handle(m_MaterialPool.Create());
		Material& material = m_MaterialPool[handle.h];
		for (Handle handle : desc.textures)
		{
			material.textures.Add(handle);
		}
		material.type = desc.type;
		return handle;
	}

	void RenderRegistry::DeleteMaterial(MaterialHandle handle)
	{
		m_MaterialPool.Delete(handle.h);
	}

	MeshHandle RenderRegistry::CreateMesh(const MeshDesc& desc, uint lodOffset)
	{
		const MeshHandle handle(m_MeshPool.Create());
		Mesh& mesh = m_MeshPool[handle.h];
		mesh.sphere = desc.sphere;
		mesh.aabbMin = desc.aabbMin;
		mesh.aabbMax = desc.aabbMax;
		mesh.aabbMax = desc.aabbMax;
		mesh.lodOffset = lodOffset;
		mesh.lodCount = desc.lodCount;
		return handle;
	}

	void RenderRegistry::DeleteMesh(MeshHandle handle)
	{
		m_MeshPool.Delete(handle.h);
	}

	SkeletalMeshHandle RenderRegistry::CreateSkeletalMesh(const SkeletalMeshDesc& desc, uint lodOffset)
	{
		const SkeletalMeshHandle handle(m_SkeletalMeshPool.Create());
		SkeletalMesh& mesh = m_SkeletalMeshPool[handle.h];
		mesh.sphere = desc.sphere;
		mesh.aabbMin = desc.aabbMin;
		mesh.aabbMax = desc.aabbMax;
		mesh.aabbMax = desc.aabbMax;
		mesh.lodOffset = lodOffset;
		mesh.lodCount = desc.lodCount;
		return handle;
	}

	void RenderRegistry::DeleteSkeletalMesh(SkeletalMeshHandle handle)
	{
		m_SkeletalMeshPool.Delete(handle.h);
	}

	MeshInstanceHandle RenderRegistry::CreateMeshInstance(const MeshInstanceDesc& desc)
	{
		const MeshInstanceHandle handle = MeshInstanceHandle(m_MeshInstancePool.Create());
		m_MeshInstancePool[handle.h].info = desc.info;
		return handle;
	}

	void RenderRegistry::DeleteMeshInstance(MeshInstanceHandle handle)
	{
		m_MeshInstancePool.Delete(handle.h);
	}

	SkeletalMeshInstanceHandle RenderRegistry::CreateSkeletalMeshInstance(const SkeletalMeshInstanceDesc& desc)
	{
		const SkeletalMeshInstanceHandle handle = SkeletalMeshInstanceHandle(m_SkeletalMeshInstancePool.Create());
		m_SkeletalMeshInstancePool[handle.h].info = desc.info;
		return handle;
	}

	void RenderRegistry::DeleteSkeletalMeshInstance(SkeletalMeshInstanceHandle handle)
	{
		m_SkeletalMeshInstancePool.Delete(handle.h);
	}

	DirLightHandle RenderRegistry::CreateDirectionalLight(const DirectionalLightDesc& desc)
	{
		const DirLightHandle handle = DirLightHandle(m_DirLightPool.Create());
		m_DirLightPool[handle.h].info = desc.info;
		return handle;
	}

	void RenderRegistry::DeleteDirectionalLight(DirLightHandle handle)
	{
		m_DirLightPool.Delete(handle.h);
	}

	PointLightHandle RenderRegistry::CreatePointLight(const PointLightDesc& desc)
	{
		const PointLightHandle handle = PointLightHandle(m_PointLightPool.Create());
		m_PointLightPool[handle.h].info = desc.info;
		return handle;
	}

	void RenderRegistry::DeletePointLight(PointLightHandle handle)
	{
		m_PointLightPool.Delete(handle.h);
	}

	SpotLightHandle RenderRegistry::CreateSpotLight(const SpotLightDesc& desc)
	{
		const SpotLightHandle handle = SpotLightHandle(m_SpotLightPool.Create());
		m_SpotLightPool[handle.h].info = desc.info;
		return handle;
	}

	void RenderRegistry::DeleteSpotLight(SpotLightHandle handle)
	{
		m_SpotLightPool.Delete(handle.h);
	}
}