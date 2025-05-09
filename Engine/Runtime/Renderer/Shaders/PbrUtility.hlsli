#ifndef PBR_UTILITY_HLSli
#define PBR_UTILITY_HLSli

#include "Common.hlsli"

// Constant normal incidence Fresnel factor for all dielectrics (ie. Average F0 for dielectric materials).
static const float c_DielectricFresnal = 0.04f;
static const float c_RoughnessScale = 1.0f / 8.0f;

// Schlick’s approximation of the reflection coefficient
float3 FresnelSchlick(float3 F0, float cosTheta)
{
    return F0 + (1.0 - F0) * pow(1.0 - abs(cosTheta), 5.0);
}

// F0 is base reflectivity of the material
// H = (L + V) / abs(L + V)
float3 FresnelSchlick(float3 F0, float3 H, float3 V)
{
    float cosTheta = dot(H, V);
    return FresnelSchlick(F0, cosTheta);
}

// Normal distribution function (specular D)
float DistributionGGX(float3 N, float3 H, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float nom = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = c_Pi * denom * denom;

    return nom / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r * r) * c_RoughnessScale;

    float nom = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return nom / denom;
}


// N = normal
// L = light direction
// V = view direction from point to camera
float GeometrySmith(float3 N, float3 V, float3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);

    float ggx1 = GeometrySchlickGGX(NdotL, roughness);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);

    return ggx1 * ggx2;
}

// halfVec = (lightDir + viewDir) / abs(lightDir + viewDir)
// lightRadiance = light colour * light intensity
// See https://typhomnt.github.io/teaching/ray_tracing/pbr_intro/ 
float3 ComputeReflectance(Material mat, float3 normal, float3 viewDir, float3 lightDir, float3 halfVec, float3 lightRadiance)
{
    // F0 is base reflectivity of the material
    
    float3 dielectricF0 = c_DielectricFresnal.xxx; // Replicates float to float3
    // Change F0 if material is not dielectric
    float3 F0 = lerp(dielectricF0, mat.albedo, mat.metallic);

    // Cook-Torrance BRDF
    float NDF = DistributionGGX(normal, halfVec, mat.roughness);
    float G = GeometrySmith(normal, viewDir, lightDir, mat.roughness);
    float3 F = FresnelSchlick(F0, max(dot(halfVec, viewDir), 0.0));

    // KS is the amount of reflected light. Eg. a mirror would have a very high KS 
    float3 kS = F;
    // KD is the amount of reflected light that gets re-emitted
    float3 kD = float3(1.0, 1.0, 1.0) - kS;
    kD *= 1.0 - mat.metallic;

    float NdotL = max(dot(normal, lightDir), 0.0);

    float3 nominator = NDF * G * F;
    float denominator = 4 * max(dot(normal, viewDir), 0.0) * NdotL + 0.00001; // avoid division by zero
    float3 specular = nominator / denominator;

    // Divide by Pi for energy conservation
    float3 diffuse = kD * mat.albedo * c_InvPi;

    return (diffuse + specular) * lightRadiance * NdotL;
}

// viewDir = view direction from point to camera (ie. canera world position - pixel world position)
float3 PBR(Material mat, float3 pos, float3 normal, float3 viewDir, float3 lightColour, float lightIntensity, float3 lightDir)
{
    // TODO : Calculate ambient occlusion 
    float ao = 0.0;
    float maxAmbient = 0.03;
    float3 ambient = mat.albedo * maxAmbient * (1.0 - ao);

    float3 lightRadiance = lightColour * lightIntensity;
    float3 halfVec = normalize(viewDir + lightDir);
    return ambient + ComputeReflectance(mat, normal, viewDir, lightDir, halfVec, lightRadiance);
}

#endif


