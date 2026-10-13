#ifndef TONEMAP_CS_HLSli
#define TONEMAP_CS_HLSli

#include "Common.hlsli"
#include "ColourUtility.hlsli"

TYR_VK_BINDING(TYR_BINDING_TEXTURES, 0) Texture2D textures[] : register(t11);
TYR_VK_BINDING(TYR_BINDING_EXPOSURE, 0) StructuredBuffer<ExposureState> exposureStates : register(t5, space1);
TYR_VK_BINDING(TYR_BINDING_DISPLAY_OUTPUT, 0) RWTexture2D<float4> outputImages[TYR_BUFFERED_FRAME_COUNT] : register(u34);

TYR_VK_PUSH_CONSTANT TonemapPushConstants g_PushConstants;

[numthreads(8, 8, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
	if (dispatchThreadId.x >= g_PushConstants.width || dispatchThreadId.y >= g_PushConstants.height)
	{
		return;
	}

	const float3 colour = textures[g_PushConstants.colourIndex].Load(int3(dispatchThreadId.xy, 0)).rgb;
	const float exposure = exposureStates[g_PushConstants.viewportIndex].exposure;
	const float3 mapped = TonemapUchimura(colour * exposure, g_PushConstants.peakBrightness);
	outputImages[g_PushConstants.renderFrameIndex][dispatchThreadId.xy] = float4(mapped * g_PushConstants.outputScale, 1.0f);
}

#endif
