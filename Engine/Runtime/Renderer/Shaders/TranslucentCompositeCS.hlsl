#ifndef TRANSLUCENT_COMPOSITE_CS_HLSli
#define TRANSLUCENT_COMPOSITE_CS_HLSli

#include "Common.hlsli"

// Blends the translucent surfaces drawn this frame over the lit image.

TYR_VK_BINDING(TYR_BINDING_TEXTURES, 0) Texture2D textures[] : register(t11);
TYR_VK_BINDING(TYR_BINDING_LIGHTING_OUTPUT, 0) RWTexture2D<float4> outputImages[TYR_BUFFERED_FRAME_COUNT] : register(u14);

// Matches TranslucentCompositePushConstants in Renderer.cpp byte for byte.
struct PushConstants
{
	uint accumulationIndex;
	uint weightIndex;
	uint width;
	uint height;
	uint renderFrameIndex;
};
TYR_VK_PUSH_CONSTANT PushConstants g_PushConstants;

[numthreads(8, 8, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
	if (dispatchThreadId.x >= g_PushConstants.width || dispatchThreadId.y >= g_PushConstants.height)
	{
		return;
	}

	const int3 pixelCoord = int3(dispatchThreadId.xy, 0);
	const float4 accumulation = textures[g_PushConstants.accumulationIndex].Load(pixelCoord);
	const float weight = textures[g_PushConstants.weightIndex].Load(pixelCoord).r;

	// The weighted average of the layers' colours, covering all but what still shows through.
	const float revealed = accumulation.a;
	const float3 layers = accumulation.rgb / max(weight, 1e-5f);
	const float4 background = outputImages[g_PushConstants.renderFrameIndex][dispatchThreadId.xy];
	outputImages[g_PushConstants.renderFrameIndex][dispatchThreadId.xy] = float4(background.rgb * revealed + layers * (1.0f - revealed), background.a);
}

#endif
