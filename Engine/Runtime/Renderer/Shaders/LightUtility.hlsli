#ifndef LIGHT_UTILITY_HLSli
#define LIGHT_UTILITY_HLSli

#include "Common.hlsli"
#include "PbrUtility.hlsli"

float3 ComputeDirectionalLightEffect(DirectionalLight light, Material mat, float3 pos, float3 normal, float3 viewDir)
{
	float3 radiance = PBR(mat, pos, normal, viewDir, light.colour, light.intensity, light.dir);
	return radiance;
}

float3 ComputePointLightEffect(PointLight light, Material mat, float3 pos, float3 normal, float3 viewDir)
{
	// The direction from the surface to the light.
	float3 lightVec = light.pos - pos;

	// The distance from the surface to the light.
	float dist = length(lightVec);

	float3 radiance = float3(0, 0, 0);
	// Only go further if the surface point is in the range of the light.
	[flatten]
	if (dist <= light.range)
	{
		// Normalize the light vector to get the direction
		lightVec /= dist;
		// Modify the light intensity based on the attentuation
		float lightIntensity = light.intensity / (light.att[0] + (light.att[1] * dist)) + (light.att[2] * (dist * dist));
		radiance = PBR(mat, pos, normal, viewDir, light.colour, lightIntensity, lightVec);
	}
	return radiance;
}

float3 ComputeSpotLightEffect(SpotLight light, Material mat, float3 pos, float3 normal, float3 viewDir)
{
	// The direction from the surface to the light.
	float3 lightVec = light.pos - pos;

	// The distance from the surface to the light.
	float dist = length(lightVec);

	float3 radiance = float3(0, 0, 0);
	// Only go further if the surface point is in the range of the light.
	[flatten]
	if (dist <= light.range)
	{
		// Normalize the light vector to get the direction
		lightVec /= dist;

		// Calculate falloff from center to edge of point light cone as light is less intense away from the cone centre
		float falloff = pow(max(dot(-lightVec, light.dir), 0.0f), light.cone);

		// Modify the light intensity based on the attentuation
		float lightIntensity = (light.intensity * falloff) / (light.att[0] + (light.att[1] * dist)) + (light.att[2] * (dist * dist));
		radiance = PBR(mat, pos, normal, viewDir, light.colour, lightIntensity, lightVec);
	}
	return radiance;
}

#endif

