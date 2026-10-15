#ifndef LIGHT_UTILITY_HLSli
#define LIGHT_UTILITY_HLSli

#include "Common.hlsli"
#include "PbrUtility.hlsli"

float3 ComputeDirectionalLightEffect(DirectionalLight light, MaterialData mat, float3 position, float3 normal, float3 viewDir)
{
	// light.direction is the direction the light travels (light -> scene) - same convention
	// ComputeSpotLightEffect's falloff below relies on (dot(-lightVec, light.direction) peaks
	// when the light->surface ray matches the cone's aim axis). PBR()/ComputeReflectance()
	// need the opposite: lightDir pointing surface -> light, same as lightVec is used for the
	// other two light types - negate it.
    float3 radiance = PBR(mat, position, normal, viewDir, light.colour, light.intensity, -light.direction);
	return radiance;
}

// 1 near the light, easing to exactly 0 at its range, so the range never shows as an edge.
float RangeWindow(float dist, float range)
{
	const float ratio = dist / max(range, 0.0001f);
	const float ratio2 = ratio * ratio;
	const float window = saturate(1.0f - ratio2 * ratio2);
	return window * window;
}

// The light a point light sends to a position, with the direction and distance to the light.
float3 GetPointLightRadiance(PointLight light, float3 position, out float3 lightDir, out float dist)
{
	const float3 lightVec = light.position - position;
	dist = length(lightVec);
	lightDir = lightVec / max(dist, 0.0001f);

	// Nothing past the light's range is lit.
	const float inRange = RangeWindow(dist, light.range);
	const float attenuation = light.attenuation[0] + light.attenuation[1] * dist + light.attenuation[2] * dist * dist;
	return light.colour * (inRange * light.intensity / max(attenuation, 0.0001f));
}

// The light a spot light sends to a position, with the direction and distance to the light.
float3 GetSpotLightRadiance(SpotLight light, float3 position, out float3 lightDir, out float dist)
{
	const float3 lightVec = light.position - position;
	dist = length(lightVec);
	lightDir = lightVec / max(dist, 0.0001f);

	// Nothing past the light's range is lit, and it's dimmer away from the centre of the cone.
	const float inRange = RangeWindow(dist, light.range);
	const float falloff = pow(max(dot(-lightDir, light.direction), 0.0f), light.cone);
	const float attenuation = light.attenuation[0] + light.attenuation[1] * dist + light.attenuation[2] * dist * dist;
	return light.colour * (inRange * light.intensity * falloff / max(attenuation, 0.0001f));
}

float3 ComputePointLightEffect(PointLight light, MaterialData mat, float3 position, float3 normal, float3 viewDir)
{
	float3 lightDir;
	float dist;
	const float3 radiance = GetPointLightRadiance(light, position, lightDir, dist);
	return PBR(mat, position, normal, viewDir, radiance, 1.0f, lightDir);
}

float3 ComputeSpotLightEffect(SpotLight light, MaterialData mat, float3 position, float3 normal, float3 viewDir)
{
	float3 lightDir;
	float dist;
	const float3 radiance = GetSpotLightRadiance(light, position, lightDir, dist);
	return PBR(mat, position, normal, viewDir, radiance, 1.0f, lightDir);
}

#endif

