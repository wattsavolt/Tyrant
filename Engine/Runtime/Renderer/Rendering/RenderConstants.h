#pragma once

#include "Core.h"
#include "RendererMacros.h"
#include "RenderResource/RenderBuffer.h"
#include "RenderResource/TextureUtil.h"
#include "RenderResource/MeshDesc.h"
#include "Shaders/ShaderTypes.h"
#include "RenderAPI/AccelerationStructure.h"
#include "Rendering/GUIDrawData.h"

namespace tyr
{
	class RenderConstants final
	{
	public:
		static constexpr size_t c_VertexBufferSize = 256 * 1024 * 1024; // 256 MB
		static constexpr size_t c_IndexBufferSize = 128 * 1024 * 1024; // 128 MB
		static constexpr size_t c_MeshletBufferSize = 64 * 1024 * 1024; // 64 MB
		// Per buffered RenderFrame slot - guiVertexBuffer/guiIndexBuffer are each one physical
		// buffer sized c_BufferedFrameCount times this. Rounded down to an exact multiple of
		// sizeof(GUIVertex) so each slot's byte offset lands exactly on a vertex boundary.
		static constexpr size_t c_GUIVertexBufferSize = (4 * 1024 * 1024 / sizeof(GUIVertex)) * sizeof(GUIVertex); // ~4 MB
		static constexpr size_t c_GUIIndexBufferSize = 1 * 1024 * 1024; // 1 MB
		// Per-frame BLAS build budget, in bytes of mesh geometry. Pending meshes are processed
		// in request order until adding the next would exceed this, with at least one always
		// processed so an oversized mesh alone can't stall the queue.
		static constexpr size_t c_MaxBLASBuildBytesPerFrame = 16 * 1024 * 1024; // 16 MB
		// 17 currently in use (13 original + activeMeshInstanceIndexBuffer/
		// visibleInstanceIndexBuffer/indirectDrawCommandBuffer/drawCountBuffer for GPU-driven
		// instance culling) - bumped to 32 for headroom while more buffers are still being added.
		static constexpr uint c_MaxBuffers = 32;
		static constexpr uint c_MaxTextures = 3000;
		static constexpr uint c_MaxMaterials = 1000;
		static constexpr uint c_MaxMeshes = 1000;
		static constexpr uint c_MaxMeshLODs = c_MaxMeshes * MeshConstants::c_MaxLods;
		static constexpr uint c_MaxSkeletalMeshes = 32;
		static constexpr uint c_MaxMeshInstances = 10000;
		// Per buffered RenderFrame slot - tlasInstanceBuffer is one physical buffer sized
		// c_BufferedFrameCount times this, since the TLAS is rebuilt every frame and each slot
		// needs its own instance data to avoid a cross-frame GPU race.
		static constexpr size_t c_TLASInstanceBufferSize = sizeof(AccelerationStructureInstance) * c_MaxMeshInstances;
		static constexpr uint c_MaxSkeletalMeshInstances = 96;
		static constexpr uint c_MaxDirLights = 4;
		static constexpr uint c_MaxPointLights = 16;
		static constexpr uint c_MaxSpotLights = 16;
		// Sized to Ultra's own ray-traced-shadow local-light cap - lower quality tiers just
		// select fewer lights into the same fixed-size slot array each tick.
		static constexpr uint c_MaxShadowCastingLocalLights = 8;
		// One shadow-mask array layer per directional light (a stable layer, matching that
		// light's own pool index - up to c_MaxDirLights of them, always traced when shadow-
		// casting, never selected/evicted) plus one per this tick's selected local light.
		static constexpr uint c_MaxShadowSlots = c_MaxDirLights + c_MaxShadowCastingLocalLights;
		static constexpr uint c_MaxTextureDimension = 4096;
		static constexpr uint c_MaxMips = TextureUtil::CalculateMaxMipsForBlockCompressed(c_MaxTextureDimension, c_MaxTextureDimension);
		static constexpr uint c_RowPitchAlignment = 256;
		// Alignment for each mip
		static constexpr uint c_SubresourceAlignment = 256;
		static constexpr uint c_UploadAlignment = c_SubresourceAlignment;
		// vkCreateAccelerationStructureKHR's offset into its backing buffer must be a multiple of
		// 256 - a fixed Vulkan spec requirement (VUID-VkAccelerationStructureCreateInfoKHR-offset-
		// 03734), not a device-queried property.
		static constexpr size_t c_AccelerationStructureAlignment = 256;
		// One shared buffer every mesh's BLAS is suballocated from instead of each mesh getting
		// its own dedicated buffer - with up to c_MaxMeshes meshes, one-buffer-per-BLAS risks
		// hitting a Vulkan implementation's maxMemoryAllocationCount.
		static constexpr size_t c_BLASStorageBufferSize = 256 * 1024 * 1024; // 256 MB
		static constexpr uint c_BufferedFrameCount = 3;
		static_assert(c_BufferedFrameCount == TYR_BUFFERED_FRAME_COUNT,
			"Keep in sync with ShaderTypes.h's TYR_BUFFERED_FRAME_COUNT - DeferredLightingCS.hlsl's "
			"outputImages[] array is sized from that, not this.");
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