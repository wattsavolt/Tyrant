#pragma once

#include "RenderAPI/Image.h"
#include "RenderAPI/DescriptorSet.h"
#include "RenderAPI/Pipeline.h"
#include "RenderAPI/ShaderModule.h"
#include "RenderBase/RenderHandles.h"

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

		// Shared vertex/index buffers every GUIDrawData submission (editor chrome, in-game HUD/
		// menu) is uploaded into for the frame - see RendererAPI::SubmitGUIDrawData.
		RenderBufferHandle guiVertexBuffer;
		RenderBufferHandle guiIndexBuffer;
		GraphicsPipelineHandle guiPipeline;
		ShaderModuleHandle guiVertexShader;
		ShaderModuleHandle guiPixelShader;

		// Offscreen colour target the editor's 3D viewport panel renders into and displays via
		// ImGui::Image() - see EditorViewport and RendererAPI::GetOrCreateViewportTexture.
		// {}/0 until the panel has requested a size for the first time.
		TextureHandle viewportColourTexture;
		uint viewportWidth = 0;
		uint viewportHeight = 0;
		// Set whenever viewportColourTexture is (re)created, consumed by RecordGeometryPass -
		// unlike the swap chain, this image isn't cycled every frame, so the "undefined -> its
		// real layout" transition a fresh image needs before its first use as a colour
		// attachment must only run once per (re)creation, not every frame.
		bool viewportTextureIsNew = false;
	};

}