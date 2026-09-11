#pragma once

#include "Core.h"
#include "RendererMacros.h"
#include "RenderResource/RenderBuffer.h"
#include "RenderResource/TextureUtil.h"
#include "RenderResource/MeshDesc.h"

namespace tyr
{
	class RenderConstants final
	{
	public:
		static constexpr size_t c_VertexBufferSize = 256 * 1024 * 1024; // 256 MB
		static constexpr size_t c_IndexBufferSize = 128 * 1024 * 1024; // 128 MB
		static constexpr size_t c_MeshletBufferSize = 64 * 1024 * 1024; // 64 MB
		static constexpr uint c_MaxBuffers = 16;
		static constexpr uint c_MaxTextures = 3000;
		static constexpr uint c_MaxMaterials = 1000;
		static constexpr uint c_MaxMeshes = 1000;
		static constexpr uint c_MaxMeshLODs = c_MaxMeshes * MeshConstants::c_MaxLods;
		static constexpr uint c_MaxSkeletalMeshes = 32;
		static constexpr uint c_MaxMeshInstances = 10000;
		static constexpr uint c_MaxSkeletalMeshInstances = 96;
		static constexpr uint c_MaxDirLights = 4;
		static constexpr uint c_MaxPointLights = 16;
		static constexpr uint c_MaxSpotLights = 16;
		static constexpr uint c_MaxTextureDimension = 4096;
		static constexpr uint c_MaxMips = TextureUtil::CalculateMaxMipsForBlockCompressed(c_MaxTextureDimension, c_MaxTextureDimension);
		static constexpr uint c_RowPitchAlignment = 256;
		// Alignment for each mip
		static constexpr uint c_SubresourceAlignment = 256;
		static constexpr uint c_UploadAlignment = c_SubresourceAlignment;
		static constexpr uint c_BufferedFrameCount = 3;
		static constexpr uint c_MaxViewsPerScene = 2;
		// This could be increased later
		static constexpr uint c_MaxViewsPerFrame = c_MaxViewsPerScene;
		static constexpr uint c_MaxViews = c_MaxViewsPerFrame * c_BufferedFrameCount;
#if TYR_EDITOR
		static constexpr uint c_MaxScenes = 4;
#else
		static constexpr uint c_MaxScenes = 1;
#endif
	};
	
}