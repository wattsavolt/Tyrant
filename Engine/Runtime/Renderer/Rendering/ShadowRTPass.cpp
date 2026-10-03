#include "ShadowRTPass.h"
#include "RenderAPI/CommandList.h"
#include "Rendering/Scene.h"
#include "Rendering/RenderRegistry.h"
#include "Rendering/RenderGraphBuilder.h"
#include "Rendering/RenderResources.h"
#include "RenderResource/RenderAccelerationStructure.h"
#include "RenderResource/Texture.h"

namespace tyr
{
	ShadowRTPass::ShadowRTPass(const ShadowRTPassArgs& args)
	{
		Recreate(args);
	}

	ShadowRTPass::~ShadowRTPass()
	{
	}

	void ShadowRTPass::Recreate(const ShadowRTPassArgs& args)
	{
		m_Registry = args.registry;
		m_Resources = args.resources;
		m_Pipeline = args.pipeline;
	}

	void ShadowRTPass::Setup(RenderGraphBuilder& builder, const Scene& scene, Vector3 cameraPosition,
		const RenderQualitySettings& qualitySettings, uint renderFrameIndex,
		TextureHandle depthBuffer, TextureHandle gbufferNormalRoughMetal, TextureHandle shadowMasksRaw)
	{
		m_DepthBuffer = depthBuffer;
		m_GBufferNormalRoughMetal = gbufferNormalRoughMetal;
		m_SelectedLocalLights.Clear();

		for (uint i = 0; i < RenderConstants::c_MaxDirLights; ++i)
		{
			m_ActiveDirLights[i] = {};
		}

		for (DirLightHandle handle : scene.content.dirLights)
		{
			if (m_Registry->GetDirectionalLight(handle).info.castsShadow)
			{
				m_ActiveDirLights[handle.h.index] = handle;
			}
		}

		// Nearest-N selection for local lights - unlike directional lights (always traced, at
		// most RenderConstants::c_MaxDirLights of them), there can be far more active point/spot
		// lights than affordable to ray-trace every frame, so only the closest
		// qualitySettings.maxShadowCastingLocalLights shadow-casting ones get a slot this tick.
		struct Candidate
		{
			bool isSpot;
			PointLightHandle pointHandle;
			SpotLightHandle spotHandle;
			uint lightIndex;
			float distanceSquared;
		};
		LocalArray<Candidate, RenderConstants::c_MaxPointLights + RenderConstants::c_MaxSpotLights> candidates;

		for (PointLightHandle handle : scene.content.pointLights)
		{
			const PointLightInfo& info = m_Registry->GetPointLight(handle).info;
			if (info.castsShadow)
			{
				Candidate& candidate = candidates.ExpandOne();
				candidate.isSpot = false;
				candidate.pointHandle = handle;
				candidate.lightIndex = handle.h.index;
				const Vector3 toLight = info.position - cameraPosition;
				candidate.distanceSquared = toLight.Dot(toLight);
			}
		}
		for (SpotLightHandle handle : scene.content.spotLights)
		{
			const SpotLightInfo& info = m_Registry->GetSpotLight(handle).info;
			if (info.castsShadow)
			{
				Candidate& candidate = candidates.ExpandOne();
				candidate.isSpot = true;
				candidate.spotHandle = handle;
				candidate.lightIndex = handle.h.index;
				const Vector3 toLight = info.position - cameraPosition;
				candidate.distanceSquared = toLight.Dot(toLight);
			}
		}

		const uint selectCount = std::min(qualitySettings.maxShadowCastingLocalLights, candidates.Size());
		for (uint pass = 0; pass < selectCount; ++pass)
		{
			uint nearestIndex = pass;
			for (uint i = pass + 1; i < candidates.Size(); ++i)
			{
				if (candidates[i].distanceSquared < candidates[nearestIndex].distanceSquared)
				{
					nearestIndex = i;
				}
			}
			if (nearestIndex != pass)
			{
				const Candidate temp = candidates[pass];
				candidates[pass] = candidates[nearestIndex];
				candidates[nearestIndex] = temp;
			}

			ShadowRTPass::SelectedLocalLight& selected = m_SelectedLocalLights.ExpandOne();
			selected.isSpot = candidates[pass].isSpot;
			selected.pointHandle = candidates[pass].pointHandle;
			selected.spotHandle = candidates[pass].spotHandle;
			selected.lightIndex = candidates[pass].lightIndex;
			selected.slot = RenderConstants::c_MaxDirLights + pass;
		}

		const PipelineStage computeStage = PIPELINE_STAGE_COMPUTE_SHADER_BIT;
		builder.ReadAccelerationStructure(m_Resources->tlas[renderFrameIndex], computeStage, BARRIER_ACCESS_ACCELERATION_STRUCTURE_READ_BIT);
		builder.ReadTexture(m_Registry->GetTexture(depthBuffer), computeStage, BARRIER_ACCESS_SHADER_READ_BIT);
		builder.ReadTexture(m_Registry->GetTexture(gbufferNormalRoughMetal), computeStage, BARRIER_ACCESS_SHADER_READ_BIT);
		builder.WriteTexture(m_Registry->GetTexture(shadowMasksRaw), computeStage, BARRIER_ACCESS_SHADER_WRITE_BIT);
	}

	void ShadowRTPass::Execute(CommandList& cmdList, uint renderFrameIndex, uint width, uint height)
	{
		if (m_SelectedLocalLights.IsEmpty())
		{
			bool anyDirActive = false;
			for (uint i = 0; i < RenderConstants::c_MaxDirLights; ++i)
			{
				anyDirActive |= (bool)m_ActiveDirLights[i];
			}
			if (!anyDirActive)
			{
				return;
			}
		}

		cmdList.BindComputePipeline(m_Pipeline);
		cmdList.BindDescriptorSet(m_Resources->descriptorSet, m_Pipeline);

		const uint groupsX = (width + 7) / 8;
		const uint groupsY = (height + 7) / 8;

		ShadowRTPushConstants pushConstants{};
		pushConstants.depthIndex = m_DepthBuffer.h.index;
		pushConstants.normalRoughMetalIndex = m_GBufferNormalRoughMetal.h.index;
		pushConstants.width = width;
		pushConstants.height = height;
		pushConstants.renderFrameIndex = renderFrameIndex;

		for (uint i = 0; i < RenderConstants::c_MaxDirLights; ++i)
		{
			if (!m_ActiveDirLights[i])
			{
				continue;
			}

			const DirectionalLightInfo& info = m_Registry->GetDirectionalLight(m_ActiveDirLights[i]).info;
			pushConstants.lightType = 0;
			pushConstants.outputSlot = i;
			pushConstants.raysPerPixel = 1;
			pushConstants.lightX = info.direction.x;
			pushConstants.lightY = info.direction.y;
			pushConstants.lightZ = info.direction.z;
			pushConstants.range = 0.0f;

			cmdList.PushConstants(m_Pipeline, SHADER_STAGE_COMPUTE_BIT, 0, sizeof(ShadowRTPushConstants), &pushConstants);
			cmdList.Dispatch(groupsX, groupsY, 1);
		}

		for (const SelectedLocalLight& selected : m_SelectedLocalLights)
		{
			pushConstants.outputSlot = selected.slot;
			pushConstants.raysPerPixel = 1;

			if (selected.isSpot)
			{
				const SpotLightInfo& info = m_Registry->GetSpotLight(selected.spotHandle).info;
				pushConstants.lightType = 2;
				pushConstants.lightX = info.position.x;
				pushConstants.lightY = info.position.y;
				pushConstants.lightZ = info.position.z;
				pushConstants.range = info.range;
				pushConstants.spotDirX = info.direction.x;
				pushConstants.spotDirY = info.direction.y;
				pushConstants.spotDirZ = info.direction.z;
				pushConstants.spotCone = info.cone;
			}
			else
			{
				const PointLightInfo& info = m_Registry->GetPointLight(selected.pointHandle).info;
				pushConstants.lightType = 1;
				pushConstants.lightX = info.position.x;
				pushConstants.lightY = info.position.y;
				pushConstants.lightZ = info.position.z;
				pushConstants.range = info.range;
			}

			cmdList.PushConstants(m_Pipeline, SHADER_STAGE_COMPUTE_BIT, 0, sizeof(ShadowRTPushConstants), &pushConstants);
			cmdList.Dispatch(groupsX, groupsY, 1);
		}
	}
}
