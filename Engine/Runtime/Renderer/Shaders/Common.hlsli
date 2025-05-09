#ifndef COMMON_HLSli
#define COMMON_HLSli

#include "ShaderTypes.h"

static const float c_Pi = 3.14159265358979323846;
static const float c_TwoPi = 2.0 * c_Pi;
static const float c_HalfPi = 0.5 * c_Pi;
static const float c_QuarterPi = 0.25 * c_Pi;
static const float c_InvPi = 1.0 / c_Pi;

struct VS_OUTPUT
{
	float4 pos : SV_POSITION;
	float4 worldPos : POSITION;
	float3 normal : NORMAL;
};

struct Material
{
    float3 albedo;
    float ambientOcclusion;
    float roughness;
    float metallic;
};

float3 DecodeSRGB(float3 c) 
{
    float3 result;
    if (c.x <= 0.04045) 
    {
        result.x = c.x / 12.92;
    }
    else 
    {
        result.x = pow((c.x + 0.055) / 1.055, 2.4);
    }

    if (c.y <= 0.04045) 
    {
        result.y = c.y / 12.92;
    }
    else 
    {
        result.y = pow((c.y + 0.055) / 1.055, 2.4);
    }

    if (c.z <= 0.04045) 
    {
        result.z = c.z / 12.92;
    }
    else 
    {
        result.z = pow((c.z + 0.055) / 1.055, 2.4);
    }

    return clamp(result, 0.0, 1.0);
}

float3 EncodeSRGB(float3 c) 
{
    float3 result;
    if (c.x <= 0.0031308) 
    {
        result.x = c.x * 12.92;
    }
    else 
    {
        result.x = 1.055 * pow(c.x, 1.0 / 2.4) - 0.055;
    }

    if (c.y <= 0.0031308) 
    {
        result.y = c.y * 12.92;
    }
    else 
    {
        result.y = 1.055 * pow(c.y, 1.0 / 2.4) - 0.055;
    }

    if (c.z <= 0.0031308) 
    {
        result.z = c.z * 12.92;
    }
    else 
    {
        result.z = 1.055 * pow(c.z, 1.0 / 2.4) - 0.055;
    }

    return clamp(result, 0.0, 1.0);
}


//StructuredBuffer<PointLight> pointLights : register(t0);

#endif


