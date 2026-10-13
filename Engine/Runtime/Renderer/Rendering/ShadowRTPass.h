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
	struct RenderFrame;
	struct SceneLights;
	struct RenderResources;

	struct ShadowRTPassArgs
	{
		RenderRegistry* registry;
		RenderResources* resources;
		ComputePipelineHandle pipeline;
	};

	// Matches ShadowRTCS.hlsl's own light-type constants exactly.
	static constexpr uint c_ShadowLightTypeDirectional = 0;
	static constexpr uint c_ShadowLightTypePoint = 1;
	static constexpr uint c_ShadowLightTypeSpot = 2;
	// No shadow-casting light occupies this slot right now - the shader writes a trivial "fully
	// lit" result and returns immediately, rather than leaving stale data from whatever light
	// last used this slot.
	static constexpr uint c_ShadowLightTypeNone = 3;

	// Matches PushConstants in ShadowRTCS.hlsl byte for byte. Declared here (not local to
	// ShadowRTPass.cpp) since Renderer::CreatePipelines also needs its exact size for the
	// pipeline's push-constant range.
	struct ShadowRTPushConstants
	{
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
		// The most a drawn LOD can stray from LOD0 per unit of distance from the camera.
		float lodErrorPerDistance;
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
			// The light's position in the scene's point or spot list, which is the index
			// DeferredLightingCS's own point/spot light loops use.
			uint lightIndex;
			uint slot;
		};

		ShadowRTPass(const ShadowRTPassArgs& args);
		~ShadowRTPass();

		void Recreate(const ShadowRTPassArgs& args);

		// Builds this tick's local-light selection and declares every resource this tick's
		// dispatches will touch. Also caches depthBuffer/gbufferNormalRoughMetal for Execute,
		// the same way RenderGraph's own Setup-then-Execute ordering lets other passes do.
		void Setup(RenderGraphBuilder& builder, const RenderFrame& renderFrame, const SceneLights& lights, Vector3 cameraPosition,
			const RenderQualitySettings& qualitySettings, uint renderFrameIndex,
			TextureHandle depthBuffer, TextureHandle gbufferNormalRoughMetal, TextureHandle shadowMasksRaw);

		void Execute(CommandList& cmdList, uint renderFrameIndex, uint width, uint height, float lodErrorPerDistance);

		// Valid after Setup - consumed by Renderer to build DeferredLightingCS's per-tick
		// light-to-slot lookup.
		const LocalArray<SelectedLocalLight, RenderConstants::c_MaxShadowCastingLocalLights>& GetSelectedLocalLights() const
		{
			return m_SelectedLocalLights;
		}
		bool IsDirLightActive(uint dirLightIndex) const { return (bool)m_ActiveDirLights[dirLightIndex]; }
		// Flat union of active directional slots (by list position) and selected local-light slots -
		// every layer ShadowDenoisePass needs to process this tick, with no need for it to repeat
		// the same directional/local iteration logic.
		const LocalArray<uint, RenderConstants::c_MaxShadowSlots>& GetActiveSlots() const { return m_ActiveSlots; }
		// Slots whose light changed this tick, so last tick's result there belongs to another light.
		const bool* GetNewSlots() const { return m_NewSlots; }

	private:
		// The light that owned each local slot last tick. A light keeps its slot while it stays
		// selected, so the denoiser's history for that slot is still its own.
		struct LocalSlotOwner
		{
			bool isSpot = false;
			PointLightHandle pointHandle;
			SpotLightHandle spotHandle;
		};

		RenderRegistry* m_Registry;
		RenderResources* m_Resources;
		ComputePipelineHandle m_Pipeline;

		// This tick's selection - rebuilt fresh every Setup call, read back by Renderer (for the
		// lighting-integration slot lookup) and by this same pass's own Execute.
		LocalArray<SelectedLocalLight, RenderConstants::c_MaxShadowCastingLocalLights> m_SelectedLocalLights;
		// Indexed by shadow slot, which is the light's position in the scene's list - default-
		// constructs invalid/falsy, set to the real handle (so Execute's registry lookup goes
		// through the normal generation check) when that light is shadow-casting this tick.
		DirLightHandle m_ActiveDirLights[RenderConstants::c_MaxDirLights] = {};
		// Every shadow slot with a directional light present this tick, shadow-casting or not -
		// Execute only ever dispatches for these (a real trace, or a cheap "fully lit" fallback),
		// never for a slot with no light at all, since DeferredLightingCS's dirLightCount-bounded
		// loop will never read a slot beyond the active light count anyway.
		LocalArray<uint, RenderConstants::c_MaxDirLights> m_PresentDirLightSlots;
		LocalArray<uint, RenderConstants::c_MaxShadowSlots> m_ActiveSlots;
		DirLightHandle m_DirSlotOwners[RenderConstants::c_MaxDirLights] = {};
		LocalSlotOwner m_LocalSlotOwners[RenderConstants::c_MaxShadowCastingLocalLights] = {};
		bool m_NewSlots[RenderConstants::c_MaxShadowSlots] = {};
		TextureHandle m_DepthBuffer;
		TextureHandle m_GBufferNormalRoughMetal;
	};
}
