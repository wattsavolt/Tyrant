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

float3 ComputePointLightEffect(PointLight light, MaterialData mat, float3 position, float3 normal, float3 viewDir)
{
	// The direction from the surface to the light.
    float3 lightVec = light.position - position;

	// The distance from the surface to the light.
	float dist = length(lightVec);

	// Nothing past the light's range is lit, applied as a multiply rather than a branch.
	const float inRange = step(dist, light.range);
	lightVec /= max(dist, 0.0001f);
	const float attenuation = light.attenuation[0] + light.attenuation[1] * dist + light.attenuation[2] * dist * dist;
	const float lightIntensity = inRange * light.intensity / max(attenuation, 0.0001f);
	return PBR(mat, position, normal, viewDir, light.colour, lightIntensity, lightVec);
}

float3 ComputeSpotLightEffect(SpotLight light, MaterialData mat, float3 position, float3 normal, float3 viewDir)
{
	// The direction from the surface to the light.
    float3 lightVec = light.position - position;

	// The distance from the surface to the light.
	float dist = length(lightVec);

	// Nothing past the light's range is lit, applied as a multiply rather than a branch.
	const float inRange = step(dist, light.range);
	lightVec /= max(dist, 0.0001f);

	// Dimmer away from the centre of the cone.
	const float falloff = pow(max(dot(-lightVec, light.direction), 0.0f), light.cone);

	const float attenuation = light.attenuation[0] + light.attenuation[1] * dist + light.attenuation[2] * dist * dist;
	const float lightIntensity = inRange * light.intensity * falloff / max(attenuation, 0.0001f);
	return PBR(mat, position, normal, viewDir, light.colour, lightIntensity, lightVec);
}

#endif

