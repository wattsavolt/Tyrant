#pragma once

#include "RenderAPI/Image.h"
#include "RenderAPI/DescriptorSet.h"
#include "RenderAPI/Pipeline.h"
#include "RenderAPI/ShaderModule.h"
#include "RenderAPI/AccelerationStructure.h"
#include "RenderBase/RenderHandles.h"
#include "RenderConstants.h"
#include "PostProcessSettings.h"
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
		// Skinned meshes' bone weights, and each skinned instance's bone matrices.
		RenderBufferHandle skinVertexBuffer;
		RenderBufferHandle boneMatrixBuffer;
		// Skins each skinned mesh instance's vertices into its own range of vertexBuffer.
		ComputePipelineHandle skinningPipeline;
		ShaderModuleHandle skinningComputeShader;
		// Default sampler for PBR and most material types
		SamplerHandle materialSampler;
		// Main descriptor resources for the long-lived pipelines
		DescriptorPoolHandle descriptorPool;
		DescriptorSetLayoutHandle descriptorSetLayout;
		DescriptorSetHandle descriptorSet;
		// The early pipelines draw what was visible last frame, the late ones what the early pass
		// missed - they differ only in their task shader, the late one also testing the Hi-Z. The
		// masked ones draw masked materials, from both sides and cut out by their alpha.
		GraphicsPipelineHandle geometryEarlyPipeline;
		GraphicsPipelineHandle geometryLatePipeline;
		GraphicsPipelineHandle geometryEarlyMaskedPipeline;
		GraphicsPipelineHandle geometryLateMaskedPipeline;
		ShaderModuleHandle geometryEarlyTaskShader;
		ShaderModuleHandle geometryLateTaskShader;
		ShaderModuleHandle geometryEarlyMaskedTaskShader;
		ShaderModuleHandle geometryLateMaskedTaskShader;
		ShaderModuleHandle geometryMeshShader;
		ShaderModuleHandle geometryPixelShader;
		ShaderModuleHandle geometryMaskedPixelShader;
		// Translucent surfaces, drawn after lighting into their own targets from both sides.
		GraphicsPipelineHandle translucentPipeline;
		ShaderModuleHandle translucentTaskShader;
		ShaderModuleHandle translucentPixelShader;
		// Blends the translucent targets over the lit image.
		ComputePipelineHandle translucentCompositePipeline;
		ShaderModuleHandle translucentCompositeComputeShader;

		// Shared vertex/index buffers every GUI draw submission (editor chrome, in-game HUD/menu)
		// is uploaded into for the frame.
		RenderBufferHandle guiVertexBuffer;
		RenderBufferHandle guiIndexBuffer;
		// One per DisplayOutput, each writing its swap chain's encoding.
		GraphicsPipelineHandle guiPipelines[static_cast<uint>(DisplayOutput::Count)];
		ShaderModuleHandle guiVertexShader;
		ShaderModuleHandle guiPixelShaders[static_cast<uint>(DisplayOutput::Count)];
		// Draws a viewport's image over the whole window, under the GUI, one per DisplayOutput.
		GraphicsPipelineHandle fullscreenPipelines[static_cast<uint>(DisplayOutput::Count)];
		ShaderModuleHandle fullscreenVertexShader;

		// Each viewport's ShaderExposureState, by viewport pool index.
		RenderBufferHandle exposureBuffer;
		// Counts the image into its viewport's histogram, then works out the exposure from it.
		ComputePipelineHandle exposureHistogramPipeline;
		ShaderModuleHandle exposureHistogramComputeShader;
		ComputePipelineHandle exposurePipeline;
		ShaderModuleHandle exposureComputeShader;
		// Exposes and tone maps the image into the viewport's display texture.
		ComputePipelineHandle tonemapPipeline;
		ShaderModuleHandle bloomDownsampleComputeShader;
		ShaderModuleHandle bloomUpsampleComputeShader;
		ComputePipelineHandle bloomDownsamplePipeline;
		ComputePipelineHandle bloomUpsamplePipeline;
		ShaderModuleHandle tonemapComputeShader;

		// Full-screen compute pass that reads the G-buffer + depth and writes the shaded result
		// into viewportColourTexture.
		ComputePipelineHandle lightingPipeline;
		ShaderModuleHandle lightingComputeShader;

		// GPU-driven culling. This frame's active mesh instances, uploaded as plain pool indices -
		// read-only input to the culling passes.
		RenderBufferHandle activeMeshInstanceIndexBuffer;
		// One ShaderMeshDraw per draw, read by the task shaders via SV_DrawIndex. The early pass's
		// draws come first, the late pass's from c_MaxMeshInstances, the same for the next two.
		RenderBufferHandle meshDrawBuffer;
		// One VkDrawMeshTasksIndirectCommandEXT-equivalent entry per draw, consumed directly by
		// GeometryPass's indirect draw call.
		RenderBufferHandle indirectDrawCommandBuffer;
		// Two atomic counters, the early and late pass's draw counts. Reset to 0 every frame.
		RenderBufferHandle drawCountBuffer;
		// One uint per mesh instance pool slot, whether it was visible last frame. Never reset -
		// a stale entry only costs one frame drawing something that ends up hidden.
		RenderBufferHandle instanceVisibilityBuffer;
		ComputePipelineHandle cullingEarlyPipeline;
		ComputePipelineHandle cullingLatePipeline;
		ShaderModuleHandle cullingEarlyComputeShader;
		ShaderModuleHandle cullingLateComputeShader;

		// The farthest depth over shrinking areas of the screen, built after the early geometry
		// pass for the late pass to test against. One per buffered RenderFrame slot, grown to fit
		// the largest viewport that slot has drawn - see HiZ.hlsli.
		RenderBufferHandle hizBuffers[RenderConstants::c_BufferedFrameCount];
		ComputePipelineHandle hizDepthPipeline;
		ComputePipelineHandle hizDownsamplePipeline;
		ShaderModuleHandle hizDepthComputeShader;
		ShaderModuleHandle hizDownsampleComputeShader;

		// Ray-traced shadow visibility - one dispatch per shadow-casting light, writing into the
		// active viewport slot's shadowMasksRaw array (see RenderViewportTextureData).
		ComputePipelineHandle shadowRTPipeline;
		ShaderModuleHandle shadowRTComputeShader;
		// Spatial + temporal denoise of shadowRTPipeline's raw output, one dispatch per active
		// shadow slot.
		ComputePipelineHandle shadowDenoisePipeline;
		ShaderModuleHandle shadowDenoiseComputeShader;
		// DeferredLightingRTCS's light-index -> shadow-slot lookup, CPU-built and re-uploaded every
		// tick (not buffered per RenderFrame slot - written and consumed within the same tick, no
		// different than activeMeshInstanceIndexBuffer).
		RenderBufferHandle shadowLightSlotMapBuffer;
		// The active scene's lights this frame, as slots in the light buffers.
		RenderBufferHandle lightIndexBuffer;
		RenderBufferHandle decalBuffer;
		// The active scene's visible decals this frame, as slots in the decal buffer.
		RenderBufferHandle decalIndexBuffer;

		// TAA resolve - optional (Renderer::m_TaaEnabled), blends this tick's lit colour with the
		// previous buffered viewport slot's own resolved history.
		ComputePipelineHandle taaResolvePipeline;
		ShaderModuleHandle taaResolveComputeShader;
		ComputePipelineHandle editorGridPipeline;
		ShaderModuleHandle editorGridComputeShader;

		// Debug lines drawn into the viewport's overlay after the grid, one pipeline tested against
		// the scene's depth and one drawn on top.
		RenderBufferHandle debugLineVertexBuffer;
		GraphicsPipelineHandle debugLineDepthTestedPipeline;
		GraphicsPipelineHandle debugLineOnTopPipeline;
		ShaderModuleHandle debugLineVertexShader;
		ShaderModuleHandle debugLinePixelShader;

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