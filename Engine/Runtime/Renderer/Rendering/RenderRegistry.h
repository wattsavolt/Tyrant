#pragma once

#include "Core.h"
#include "RendererMacros.h"
#include "RenderConstants.h"
#include "RenderBase/RenderHandles.h"
#include "RenderResource/RenderBuffer.h"
#include "RenderResource/Texture.h"
#include "RenderResource/Material.h"
#include "RenderResource/Mesh.h"
#include "RenderInstance/RenderInstances.h"
#include "Scene.h"

namespace tyr
{
	class Device;
	class FrameUploadAllocator;

	// TODO: Add skeleton buffer and bone buffer
	
	// Class that manages long-lived render resources
	class RenderRegistry final : INonCopyable
	{
	public:
		RenderRegistry(Device& device);
		~RenderRegistry();

		static RenderRegistry* Instance();

		// Note: Do not modify resources in the pool outside the renderer after they have been added to a scene
		// to prevent race conditions as they will be accessed by async tasks
		RenderBufferHandle CreateBuffer(const RenderBufferDesc& desc);
		void DeleteBuffer(RenderBufferHandle handle);
		RenderBuffer& GetBuffer(RenderBufferHandle handle)
		{
			return m_BufferPool[handle.h];
		}
		const RenderBuffer& GetBuffer(RenderBufferHandle handle) const
		{
			return m_BufferPool[handle.h];
		}

		TextureHandle CreateTexture(const TextureDesc& desc);
		void DeleteTexture(TextureHandle handle);
		Texture& GetTexture(TextureHandle handle)
		{
			return m_TexturePool[handle.h];
		}
		const Texture& GetTexture(TextureHandle handle) const
		{
			return m_TexturePool[handle.h];
		}

		MaterialHandle CreateMaterial(const MaterialDesc& desc);
		void DeleteMaterial(MaterialHandle handle);
		Material& GetMaterial(MaterialHandle handle)
		{
			return m_MaterialPool[handle.h];
		}
		const Material& GetMaterial(MaterialHandle handle) const
		{
			return m_MaterialPool[handle.h];
		}

		MeshHandle CreateMesh(const MeshDesc& desc, uint lodOffset);
		void DeleteMesh(MeshHandle handle);
		const Mesh& GetMesh(MeshHandle handle) const
		{
			return m_MeshPool[handle.h];
		}

		SkeletalMeshHandle CreateSkeletalMesh(const SkeletalMeshDesc& desc, uint lodOffset);
		void DeleteSkeletalMesh(SkeletalMeshHandle handle);
		const SkeletalMesh& GetSkeletalMesh(SkeletalMeshHandle handle) const
		{
			return m_SkeletalMeshPool[handle.h];
		}

		MeshInstanceHandle CreateMeshInstance(const MeshInstanceDesc& desc);
		void DeleteMeshInstance(MeshInstanceHandle handle);
		MeshInstance& GetMeshInstance(MeshInstanceHandle handle)
		{
			return m_MeshInstancePool[handle.h];
		}
		const MeshInstance& GetMeshInstance(MeshInstanceHandle handle) const
		{
			return m_MeshInstancePool[handle.h];
		}

		SkeletalMeshInstanceHandle CreateSkeletalMeshInstance(const SkeletalMeshInstanceDesc& desc);
		void DeleteSkeletalMeshInstance(SkeletalMeshInstanceHandle handle);
		SkeletalMeshInstance& GetSkeletalMeshInstance(SkeletalMeshInstanceHandle handle)
		{
			return m_SkeletalMeshInstancePool[handle.h];
		}
		const SkeletalMeshInstance& GetSkeletalMeshInstance(SkeletalMeshInstanceHandle handle) const
		{
			return m_SkeletalMeshInstancePool[handle.h];
		}

		DirLightHandle CreateDirectionalLight(const DirectionalLightDesc& desc);
		void DeleteDirectionalLight(DirLightHandle handle);
		DirectionalLight& GetDirectionalLight(DirLightHandle handle)
		{
			return m_DirLightPool[handle.h];
		}
		const DirectionalLight& GetDirectionalLight(DirLightHandle handle) const
		{
			return m_DirLightPool[handle.h];
		}

		PointLightHandle CreatePointLight(const PointLightDesc& desc);
		void DeletePointLight(PointLightHandle handle);
		PointLight& GetPointLight(PointLightHandle handle)
		{
			return m_PointLightPool[handle.h];
		}
		const PointLight& GetPointLight(PointLightHandle handle) const
		{
			return m_PointLightPool[handle.h];
		}

		SpotLightHandle CreateSpotLight(const SpotLightDesc& desc);
		void DeleteSpotLight(SpotLightHandle handle);
		SpotLight& GetSpotLight(SpotLightHandle handle)
		{
			return m_SpotLightPool[handle.h];
		}
		const SpotLight& GetSpotLight(SpotLightHandle handle) const
		{
			return m_SpotLightPool[handle.h];
		}

		Device& GetDevice() { return m_Device; };

	private:
		Device& m_Device;

		LocalObjectPool<RenderBuffer, RenderConstants::c_MaxBuffers> m_BufferPool;
		LocalObjectPool<Texture, RenderConstants::c_MaxTextures> m_TexturePool;
		LocalObjectPool<Material, RenderConstants::c_MaxMaterials> m_MaterialPool;
		LocalObjectPool<Mesh, RenderConstants::c_MaxMeshes> m_MeshPool;
		LocalObjectPool<SkeletalMesh, RenderConstants::c_MaxSkeletalMeshes> m_SkeletalMeshPool;
		LocalObjectPool<MeshInstance, RenderConstants::c_MaxMeshInstances> m_MeshInstancePool;
		LocalObjectPool<SkeletalMeshInstance, RenderConstants::c_MaxSkeletalMeshInstances> m_SkeletalMeshInstancePool;
		LocalObjectPool<DirectionalLight, RenderConstants::c_MaxDirLights> m_DirLightPool;
		LocalObjectPool<PointLight, RenderConstants::c_MaxPointLights> m_PointLightPool;
		LocalObjectPool<SpotLight, RenderConstants::c_MaxSpotLights> m_SpotLightPool;

		static RenderRegistry* s_Instance;
	};
	
}