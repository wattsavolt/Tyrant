#ifndef PARTICLE_HLSli
#define PARTICLE_HLSli

// Matches ParticleInstance in Rendering/ParticleTypes.h byte for byte.
struct ParticleInstance
{
	float3 position;
	float size;
	float3 velocity;
	float stretch;
	uint colour;
	float rotation;
	float frame;
	uint textureIndex;
	uint flipbook;
	float brightness;
	float additive;
	float softness;
};

// Matches ParticlePushConstants in Renderer.cpp byte for byte.
struct PushConstants
{
	uint firstParticle;
	uint depthIndex;
	// A device depth d is depthScale / (d - depthOffset) metres in front of the camera.
	float depthScale;
	float depthOffset;
};
TYR_VK_PUSH_CONSTANT PushConstants g_PushConstants;

// Matches c_NoParticleTexture.
static const uint c_NoParticleTexture = 0xFFFFFFFFu;

struct PARTICLE_VS_OUTPUT
{
	float4 pos : SV_POSITION;
	float2 uv : TEXCOORD0;
	float viewDepth : TEXCOORD1;
	float4 colour : COLOR0;
	nointerpolation float frame : TEXCOORD2;
	nointerpolation uint textureIndex : TEXCOORD3;
	nointerpolation uint flipbook : TEXCOORD4;
	nointerpolation float brightness : TEXCOORD5;
	nointerpolation float additive : TEXCOORD6;
	nointerpolation float softness : TEXCOORD7;
};

float4 UnpackParticleColour(uint packed)
{
	return float4(
		(packed & 0xFF) / 255.0f,
		((packed >> 8) & 0xFF) / 255.0f,
		((packed >> 16) & 0xFF) / 255.0f,
		((packed >> 24) & 0xFF) / 255.0f);
}

#endif
