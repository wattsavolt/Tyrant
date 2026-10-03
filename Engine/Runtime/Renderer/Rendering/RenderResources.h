#pragma once

#include "RenderAPI/Image.h"
#include "RenderAPI/DescriptorSet.h"
#include "RenderAPI/Pipeline.h"
#include "RenderAPI/ShaderModule.h"
#include "RenderAPI/AccelerationStructure.h"
#include "RenderBase/RenderHandles.h"
#include "RenderConstants.h"
#include "RenderResource/RenderAccelerationStructure.h"

namespace tyr
{
	/*
	
	Meshes buffer
	-> Mesh{ lodOffset, lodCount }

	MeshLODs buffer
	-> MeshLOD{ meshletIndexOffset, meshletCount }

	meshletIndices buffer (add this later when we support streaming parts of a LOD in)
    -> logical -> physical mapping

	meshletBuffer
    -> actual meshlet data

	*/

	struct RenderResources
	{
		RenderBufferHandle materialBuffer;
		RenderBufferHandle vertexBuffer;
		RenderBufferHandle indexBuffer;
		RenderBufferHandle meshletBuffer;
		RenderBufferHandle meshLODBuffer;
		RenderBufferHandle meshBuffer;
		RenderBufferHandle meshInstanceBuffer;
		RenderBufferHandle directionalLightBuffer;
		RenderBufferHandle pointLightBuffer;
		RenderBufferHandle spotLightBuffer;
		RenderBufferHandle sceneInfoBuffer;
		// Default sampler for PBR and most material types
		SamplerHandle materialSampler;
		// Main descriptor resources for the long-lived pipelines
		DescriptorPoolHandle descriptorPool;
		DescriptorSetLayoutHandle descriptorSetLayout;
		DescriptorSetHandle descriptorSet;
		GraphicsPipelineHandle geometryGraphicsPipeline;
		ShaderModuleHandle geometryTaskShader;
		ShaderModuleHandle geometryMeshShader;
		ShaderModuleHandle geometryPixelShader;

		// Shared vertex/index buffers every GUI draw submission (editor chrome, in-game HUD/menu)
		// is uploaded into for the frame.
		RenderBufferHandle guiVertexBuffer;
		RenderBufferHandle guiIndexBuffer;
		GraphicsPipelineHandle guiPipeline;
		ShaderModuleHandle guiVertexShader;
		ShaderModuleHandle guiPixelShader;

		// Full-screen compute pass that reads the G-buffer + depth and writes the shaded result
		// into viewportColourTexture.
		ComputePipelineHandle lightingPipeline;
		ShaderModuleHandle lightingComputeShader;

		// GPU-driven instance frustum culling. This frame's active mesh instances, uploaded as
		// plain pool indices - read-only input to the culling pass.
		RenderBufferHandle activeMeshInstanceIndexBuffer;
		// Compacted pool indices of instances that passed culling, one entry per visible
		// instance, written by the culling pass and read by MeshAS.hlsl via SV_DrawIndex.
		RenderBufferHandle visibleInstanceIndexBuffer;
		// One VkDrawMeshTasksIndirectCommandEXT-equivalent entry per visible instance, written
		// by the culling pass and consumed directly by GeometryPass's indirect draw call.
		RenderBufferHandle indirectDrawCommandBuffer;
		// Single atomic counter - how many of the two buffers above are actually populated this
		// frame. Reset to 0 every frame before the culling pass increments it.
		RenderBufferHandle drawCountBuffer;
		ComputePipelineHandle cullingPipeline;
		ShaderModuleHandle cullingComputeShader;

		// Ray-traced shadow visibility - one dispatch per shadow-casting light, writing into the
		// active viewport slot's shadowMasksRaw array (see RenderViewportTextureData).
		ComputePipelineHandle shadowRTPipeline;
		ShaderModuleHandle shadowRTComputeShader;

		// Ray-traced shadows - one top-level acceleration structure per buffered RenderFrame
		// slot, rebuilt every frame from the active scene's mesh instances. Per-mesh bottom-level
		// structures live on Mesh::blas instead.
		RenderAccelerationStructure tlas[RenderConstants::c_BufferedFrameCount];
		// This frame's AccelerationStructureInstance entries, uploaded from CPU then read
		// directly by the TLAS build (not bound in the bindless descriptor set). One physical
		// buffer, each buffered slot confined to its own byte range.
		RenderBufferHandle tlasInstanceBuffer;
		// Sized once at startup from the TLAS build's scratch-size requirement (every slot needs
		// the same amount) - one physical buffer, each slot confined to its own byte range.
		RenderBufferHandle tlasScratchBuffer;
		// Grown on demand to fit the sum of one tick's whole BLAS build batch - each build gets
		// its own aligned sub-range within it, not a range shared/reused across builds.
		RenderBufferHandle blasScratchBuffer;
		// One shared buffer every mesh's BLAS is suballocated into, instead of each mesh getting
		// its own dedicated allocation. Not a per-slot/per-frame resource - a BLAS is built once
		// and persists, the same lifetime as vertex/index/meshlet mesh data.
		RenderBufferHandle blasStorageBuffer;

		// Fixed-size, per-renderFrame-slot staging region for data a worker thread uploads
		// itself after that frame's scene merge finishes - gives that one caller its own upload
		// memory with nothing else touching it, so no allocator/cursor/locking is needed.
		RenderBufferHandle rtCullingStagingBuffer;
	};

}