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
	// Where this point was last frame, for motion vectors.
	float4 prevWorldPos : POSITION1;
	float3 normal : NORMAL;
	// xyz = world-space tangent, w = bitangent sign (bitangent = cross(normal, tangent.xyz) * tangent.w)
	float4 tangent : TANGENT0;
	float2 uv : TEXCOORD0;
	nointerpolation uint materialIndex : MATERIAL0;
	nointerpolation float emissiveScale : EMISSIVE0;
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

// Unpacks a uint holding two snorm16 values (low 16 bits = x, high 16 bits = y) before
// decoding as an octahedral-encoded unit vector.
float3 DecodeOct(uint packed)
{
    int sx = (int)(packed << 16) >> 16;
    int sy = (int)packed >> 16;
    float2 e = max(float2(sx, sy) / 32767.0, -1.0);
    return DecodeOct(e);
}

// Matches MeshUtil::EncodeOctTangent: x as snorm16 in the low half, y as snorm15 above it, and
// the bitangent's sign in the top bit.
float3 DecodeOctTangent(uint packed, out float bitangentSign)
{
    bitangentSign = (packed & 0x80000000u) != 0 ? -1.0f : 1.0f;
    const int sx = (int)(packed << 16) >> 16;
    const int sy = (int)(packed << 1) >> 17;
    return DecodeOct(max(float2(sx / 32767.0, sy / 16383.0), -1.0));
}

// Inverse of DecodeOct(float2) - packs a unit vector into two [-1,1] components for the
// G-buffer's normal target.
float2 EncodeOct(float3 n)
{
    n /= (abs(n.x) + abs(n.y) + abs(n.z));
    float2 e = n.z >= 0.0 ? n.xy : (1.0 - abs(n.yx)) * (step(0.0, n.xy) * 2.0 - 1.0);
    return e;
}

// Matches MeshUtil::EncodeOct - two snorm16 values, x in the low 16 bits.
uint EncodeOctPacked(float3 n)
{
	const float2 e = EncodeOct(n);
	const int2 quantized = int2(round(clamp(e, -1.0, 1.0) * 32767.0));
	return ((uint)(quantized.y & 0xFFFF) << 16) | (uint)(quantized.x & 0xFFFF);
}

// The reverse of DecodeOctTangent.
uint EncodeOctTangentPacked(float3 t, float bitangentSign)
{
	const int2 quantized = int2(round(clamp(EncodeOct(t), -1.0, 1.0) * float2(32767.0, 16383.0)));
	return ((uint)(quantized.y & 0x7FFF) << 16) | (uint)(quantized.x & 0xFFFF) | (bitangentSign < 0.0f ? 0x80000000u : 0u);
}

#endif


