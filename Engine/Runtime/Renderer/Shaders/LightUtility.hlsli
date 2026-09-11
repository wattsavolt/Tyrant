#ifndef LIGHT_UTILITY_HLSli
#define LIGHT_UTILITY_HLSli

#include "Common.hlsli"
#include "PbrUtility.hlsli"

float3 ComputeDirectionalLightEffect(DirectionalLight light, MaterialData mat, float3 position, float3 normal, float3 viewDir)
{
    float3 radiance = PBR(mat, position, normal, viewDir, light.colour, light.intensity, light.direction);
	return radiance;
}

float3 ComputePointLightEffect(PointLight light, MaterialData mat, float3 position, float3 normal, float3 viewDir)
{
	// The direction from the surface to the light.
    float3 lightVec = light.position - position;

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
        float lightIntensity = light.intensity / (light.attenuation[0] + (light.attenuation[1] * dist)) + (light.attenuation[2] * (dist * dist));
        radiance = PBR(mat, position, normal, viewDir, light.colour, lightIntensity, lightVec);
    }
	return radiance;
}

float3 ComputeSpotLightEffect(SpotLight light, MaterialData mat, float3 position, float3 normal, float3 viewDir)
{
	// The direction from the surface to the light.
    float3 lightVec = light.position - position;

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
		float falloff = pow(max(dot(-lightVec, light.direction), 0.0f), light.cone);

		// Modify the light intensity based on the attentuation
        float lightIntensity = (light.intensity * falloff) / (light.attenuation[0] + (light.attenuation[1] * dist)) + (light.attenuation[2] * (dist * dist));
        radiance = PBR(mat, position, normal, viewDir, light.colour, lightIntensity, lightVec);
    }
	return radiance;
}

#endif

