#ifndef PARTICLE_VS_HLSli
#define PARTICLE_VS_HLSli

#include "ShaderTypes.h"
#include "Particle.hlsli"

TYR_VK_BINDING(TYR_BINDING_PARTICLE, 0) StructuredBuffer<ParticleInstance> particles : register(t36);

TYR_VK_BINDING(TYR_BINDING_SCENE_INFO, 0)
cbuffer SceneInfoCBuffer : register(b0)
{
	float4x4 ViewProj;
	float4x4 InvViewProj;
	float3 camPos;
	float ambient;
};

// Two triangles for each particle's square, as corners from -1 to 1.
static const float2 c_Corners[6] = { float2(-1.0f, -1.0f), float2(1.0f, -1.0f), float2(1.0f, 1.0f),
	float2(-1.0f, -1.0f), float2(1.0f, 1.0f), float2(-1.0f, 1.0f) };

PARTICLE_VS_OUTPUT main(uint vertexID : SV_VertexID)
{
	const ParticleInstance particle = particles[g_PushConstants.firstParticle + vertexID / 6];
	const float2 corner = c_Corners[vertexID % 6];

	// Faces the camera, turned about the line to it, upright unless looked at from straight above or below.
	const float3 toCamera = normalize(camPos - particle.position);
	const float3 reference = abs(toCamera.y) > 0.99f ? float3(0.0f, 0.0f, 1.0f) : float3(0.0f, 1.0f, 0.0f);
	const float3 right = normalize(cross(reference, toCamera));
	const float3 up = cross(toCamera, right);
	float sine, cosine;
	sincos(particle.rotation, sine, cosine);
	const float2 turned = float2(corner.x * cosine - corner.y * sine, corner.x * sine + corner.y * cosine);
	const float3 square = particle.position + (right * turned.x + up * turned.y) * particle.size;

	// A streak runs back from it along the way it's going, as seen from the camera, and is as wide as it.
	const float3 across = particle.velocity - toCamera * dot(particle.velocity, toCamera);
	const float acrossLength = length(across);
	const float streakLength = acrossLength * particle.stretch;
	const float3 along = across / max(acrossLength, 1e-6f);
	const float3 sideways = cross(toCamera, along);
	const float3 streak = particle.position - along * (streakLength * 0.5f)
		+ along * (corner.y * (particle.size + streakLength * 0.5f)) + sideways * (corner.x * particle.size);
	const float3 world = lerp(square, streak, step(1e-4f, streakLength));

	PARTICLE_VS_OUTPUT output;
	output.pos = mul(float4(world, 1.0f), ViewProj);
	output.uv = corner * float2(0.5f, -0.5f) + 0.5f;
	output.viewDepth = output.pos.w;
	output.colour = UnpackParticleColour(particle.colour);
	output.frame = particle.frame;
	output.textureIndex = particle.textureIndex;
	output.flipbook = particle.flipbook;
	output.brightness = particle.brightness;
	output.additive = particle.additive;
	output.softness = particle.softness;
	return output;
}

#endif
