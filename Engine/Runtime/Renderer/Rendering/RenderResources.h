#pragma once

#include "RenderAPI/Image.h"
#include "RenderAPI/DescriptorSet.h"
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
		ShaderModuleHandle geometryMeshShader;
		ShaderModuleHandle geometryPixelShader;
	};

}