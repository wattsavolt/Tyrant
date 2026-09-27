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
	// xyz = world-space tangent, w = bitangent sign (bitangent = cross(normal, tangent.xyz) * tangent.w)
	float4 tangent : TANGENT0;
	float2 uv : TEXCOORD0;
	nointerpolation uint materialIndex : MATERIAL0;
};

struct MaterialData
{
    float3 albedo;
    float ambientOcclusion;
    float roughness;
    float metallic;
};

float3 DecodeSRGB(float3 c)
{
    float3 lt = c / 12.92;
    float3 gt = pow((c + 0.055) / 1.055, 2.4);
    return clamp(lerp(lt, gt, step(0.04045, c)), 0.0, 1.0);
}

float3 EncodeSRGB(float3 c)
{
    float3 lt = c * 12.92;
    float3 gt = 1.055 * pow(c, 1.0 / 2.4) - 0.055;
    return clamp(lerp(lt, gt, step(0.0031308, c)), 0.0, 1.0);
}

float3 DecodeOct(float2 e)
{
    float3 v = float3(e.xy, 1.0 - abs(e.x) - abs(e.y));
    float2 t = saturate(-v.z).xx;
    v.xy += t * -sign(v.xy); // sign(0) returns 0 in HLSL, matching the >=0 branch's -t case closely enough in practice
    return normalize(v);
}

// Unpacks a uint holding two snorm16 values (low 16 bits = x, high 16 bits = y - see
// MeshUtil::EncodeOct) before decoding as an octahedral-encoded unit vector.
float3 DecodeOct(uint packed)
{
    int sx = (int)(packed << 16) >> 16;
    int sy = (int)packed >> 16;
    float2 e = max(float2(sx, sy) / 32767.0, -1.0);
    return DecodeOct(e);
}

// Same packing as DecodeOct(uint), except the top bit is the bitangent sign, not part of
// the oct encoding - see MeshUtil::VertexToShaderVertex, which packs it in after encoding.
float3 DecodeOctTangent(uint packed, out float bitangentSign)
{
    bitangentSign = (packed & 0x80000000u) != 0 ? -1.0f : 1.0f;
    return DecodeOct(packed & 0x7FFFFFFFu);
}

#endif


