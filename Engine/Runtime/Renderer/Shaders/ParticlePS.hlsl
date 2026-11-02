#ifndef PARTICLE_PS_HLSli
#define PARTICLE_PS_HLSli

#include "ShaderTypes.h"
#include "Particle.hlsli"

TYR_VK_BINDING(TYR_BINDING_TEXTURES, 0) Texture2D textures[] : register(t11);
TYR_VK_BINDING(TYR_BINDING_SAMPLERS, 0) SamplerState samplers[] : register(s12);

// Premultiplied: drawn with One and InvSrcAlpha, so a fully additive particle leaves alpha at 0 and
// only adds its light, and an opaque one covers what's behind it.
float4 main(PARTICLE_VS_OUTPUT input) : SV_TARGET
{
	// Hidden behind the scene, and fading out as it nears the surface in front of it.
	const float sceneDevice = textures[g_PushConstants.depthIndex].Load(int3(input.pos.xy, 0)).r;
	const float sceneDepth = g_PushConstants.depthScale / max(sceneDevice - g_PushConstants.depthOffset, 1e-7f);
	const float soft = saturate((sceneDepth - input.viewDepth) / max(input.softness, 1e-3f));

	float4 texel;
	if (input.textureIndex == c_NoParticleTexture)
	{
		// A soft round blob when there's no texture.
		const float2 centred = input.uv * 2.0f - 1.0f;
		const float fade = saturate(1.0f - dot(centred, centred));
		texel = float4(1.0f, 1.0f, 1.0f, fade * fade);
	}
	else
	{
		// Blends from one flipbook frame into the next.
		const uint columns = max(input.flipbook & 0xFF, 1u);
		const uint rows = max((input.flipbook >> 8) & 0xFF, 1u);
		const uint frameCount = columns * rows;
		const uint frame0 = (uint)input.frame % frameCount;
		const uint frame1 = (frame0 + 1) % frameCount;
		const float2 cell = 1.0f / float2(columns, rows);
		const float2 uv0 = (float2(frame0 % columns, frame0 / columns) + input.uv) * cell;
		const float2 uv1 = (float2(frame1 % columns, frame1 / columns) + input.uv) * cell;
		Texture2D flipbook = textures[NonUniformResourceIndex(input.textureIndex)];
		texel = lerp(flipbook.Sample(samplers[0], uv0), flipbook.Sample(samplers[0], uv1), frac(input.frame));
	}

	const float alpha = texel.a * input.colour.a * soft;
	const float3 light = texel.rgb * input.colour.rgb * input.brightness * alpha;
	return float4(light, alpha * (1.0f - input.additive));
}

#endif
