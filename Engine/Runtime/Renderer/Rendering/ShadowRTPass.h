#pragma once

#include "Core.h"
#include "RendererMacros.h"
#include "RenderAPI/Pipeline.h"
#include "RenderBase/RenderHandles.h"
#include "Math/Vector3.h"
#include "RenderConstants.h"
#include "RenderQualitySettings.h"

namespace tyr
{
	class CommandList;
	class RenderRegistry;
	class RenderGraphBuilder;
	struct Scene;
	struct RenderResources;

	struct ShadowRTPassArgs
	{
		RenderRegistry* registry;
		RenderResources* resources;
		ComputePipelineHandle pipeline;
	};

	// Matches PushConstants in ShadowRTCS.hlsl byte for byte. Declared here (not local to
	// ShadowRTPass.cpp) since Renderer::CreatePipelines also needs its exact size for the
	// pipeline's push-constant range.
	struct ShadowRTPushConstants
	{
		// 0 = Directional, 1 = Point, 2 = Spot - matches ShadowRTCS.hlsl's own light-type
		// constants.
		uint lightType;
		uint depthIndex;
		uint normalRoughMetalIndex;
		uint outputSlot;
		uint width;
		uint height;
		uint renderFrameIndex;
		uint raysPerPixel;
		// Direction for a directional light, position for point/spot.
		float lightX;
		float lightY;
		float lightZ;
		float range;
		float spotDirX;
		float spotDirY;
		float spotDirZ;
		float spotCone;
	};

	// One ray-traced visibility dispatch per shadow-casting light this tick: every active
	// directional light (up to RenderConstants::c_MaxDirLights - always traced, never selected
	// or evicted, since there are so few of them) plus up to this tick's quality-capped number
	// of nearest shadow-casting point/spot lights. Writes raw (not yet denoised) results into
	// the active viewport slot's shadowMasksRaw array, one layer per light.
	class ShadowRTPass final : public INonCopyable
	{
	public:
		struct SelectedLocalLight
		{
			bool isSpot;
			// Only one of these is valid, per isSpot - kept as real handles (not a bare pool
			// index) so Execute's registry lookup goes through the normal generation check.
			PointLightHandle pointHandle;
			SpotLightHandle spotHandle;
			// The flat index DeferredLightingCS's own point/spot light loops use - safe to use
			// as a bare index there since it's only ever an array position, never a registry
			// lookup key.
			uint lightIndex;
			uint slot;
		};

		ShadowRTPass(const ShadowRTPassArgs& args);
		~ShadowRTPass();

		void Recreate(const ShadowRTPassArgs& args);

		// Builds this tick's local-light selection and declares every resource this tick's
		// dispatches will touch. Also caches depthBuffer/gbufferNormalRoughMetal for Execute,
		// the same way RenderGraph's own Setup-then-Execute ordering lets other passes do.
		void Setup(RenderGraphBuilder& builder, const Scene& scene, Vector3 cameraPosition,
			const RenderQualitySettings& qualitySettings, uint renderFrameIndex,
			TextureHandle depthBuffer, TextureHandle gbufferNormalRoughMetal, TextureHandle shadowMasksRaw);

		void Execute(CommandList& cmdList, uint renderFrameIndex, uint width, uint height);

		// Valid after Setup - consumed by Renderer to build DeferredLightingCS's per-tick
		// light-to-slot lookup.
		const LocalArray<SelectedLocalLight, RenderConstants::c_MaxShadowCastingLocalLights>& GetSelectedLocalLights() const
		{
			return m_SelectedLocalLights;
		}
		bool IsDirLightActive(uint dirLightIndex) const { return (bool)m_ActiveDirLights[dirLightIndex]; }

	private:
		RenderRegistry* m_Registry;
		RenderResources* m_Resources;
		ComputePipelineHandle m_Pipeline;

		// This tick's selection - rebuilt fresh every Setup call, read back by Renderer (for the
		// lighting-integration slot lookup) and by this same pass's own Execute.
		LocalArray<SelectedLocalLight, RenderConstants::c_MaxShadowCastingLocalLights> m_SelectedLocalLights;
		// Indexed by pool slot - default-constructs invalid/falsy, set to the real handle (so
		// Execute's registry lookup goes through the normal generation check) when that slot's
		// directional light is active and shadow-casting this tick.
		DirLightHandle m_ActiveDirLights[RenderConstants::c_MaxDirLights] = {};
		TextureHandle m_DepthBuffer;
		TextureHandle m_GBufferNormalRoughMetal;
	};
}
