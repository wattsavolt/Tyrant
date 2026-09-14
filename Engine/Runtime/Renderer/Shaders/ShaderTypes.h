#ifndef SHADER_TYPES_H
#define SHADER_TYPES_H

#ifdef __cplusplus

#include "RendererMacros.h"
#include "Core.h"
#include "Math/Vector3.h"
#include "Math/Vector4.h"
#include "Math/Quaternion.h"
#include "Math/Matrix4.h"
#include "Math/Half2.h"

namespace tyr
{
#define TYR_SHADER_STRUCT(name) struct Shader##name
	#define TYR_SHADER_HALF2 Half2
	#define TYR_SHADER_FLOAT2 Vector2
	#define TYR_SHADER_FLOAT3 Vector3 
	#define TYR_SHADER_FLOAT4 Vector4 
	#define TYR_SHADER_QUATERNION Quaternion
	#define TYR_SHADER_FLOAT3x3 Matrix3
	#define TYR_SHADER_FLOAT4x4 Matrix4
}
#else
	#define TYR_SHADER_STRUCT(name) struct name
	#define TYR_SHADER_HALF2 half2
	#define TYR_SHADER_FLOAT2 float2
	#define TYR_SHADER_FLOAT3 float3
	#define TYR_SHADER_FLOAT4 float4
	#define TYR_SHADER_QUATERNION float4
	#define TYR_SHADER_FLOAT3x3 float3x3
	#define TYR_SHADER_FLOAT4x4 float4x4
#endif

#ifdef __cplusplus
namespace tyr
{
#endif

	TYR_SHADER_STRUCT(View)
	{
		TYR_SHADER_FLOAT4x4 viewProj;
		TYR_SHADER_FLOAT3 camPos;
		uint prevViewIndex;
		uint flags;
	};

	TYR_SHADER_STRUCT(SceneInfo)
	{
		uint viewCount;
		float ambient;
		float padding0;
		float padding1;
	};

	/// Point light used in renderer 
	TYR_SHADER_STRUCT(DirectionalLight)
	{
		TYR_SHADER_FLOAT3 direction;
		float intensity;
		TYR_SHADER_FLOAT3 colour;
		float padding;
	};

	TYR_SHADER_STRUCT(PointLight)
	{
		TYR_SHADER_FLOAT3 position;
		float range;
		TYR_SHADER_FLOAT3 attenuation;
		float intensity;
		TYR_SHADER_FLOAT3 colour;
		float padding;
	};

	TYR_SHADER_STRUCT(SpotLight)
	{
		TYR_SHADER_FLOAT3 position;
		float range;
		TYR_SHADER_FLOAT3 direction;
		float cone;
		TYR_SHADER_FLOAT3 attenuation;
		float intensity;
		TYR_SHADER_FLOAT3 colour;
		float padding;
	};

	TYR_SHADER_STRUCT(Material)
	{
		// PBR:
		// Texture 0 - albedo  
		// Texture 1 - RGB for normal and A for height 
		// Texture 2 - R = ambient occlusion, G = roughness, B = metallic 
		uint texture0; 
		uint texture1; 
		uint texture2;
		uint texture3; // could be used with a non-PBR material type 
		uint flags;
	};

	TYR_SHADER_STRUCT(Vertex)
	{
		// 3D position (world/local space)
		TYR_SHADER_FLOAT3 position;           // 12 bytes

		// Oct-encoded normal and tangent (4 bytes each). They can be encoded as octahedral because they are unit-vectors
		uint normalOct;            // 4 bytes: octahedral encoded normal
		uint tangentOct;           // 4 bytes: octahedral encoded tangent + bitangent sign

		// Texture coordinates (half precision for bandwidth)
		TYR_SHADER_HALF2 uv;                  // 4 bytes
	};

	TYR_SHADER_STRUCT(AnimVertex)
	{
		// 3D position (world/local space)
		TYR_SHADER_FLOAT3 position;           // 12 bytes

		// Oct-encoded normal and tangent (4 bytes each)
		uint normalOct;            // 4 bytes: octahedral encoded normal
		uint tangentOct;           // 4 bytes: octahedral encoded tangent + bitangent sign

		// Texture coordinates (half precision for bandwidth)
		TYR_SHADER_HALF2 uv;                  // 4 bytes

		// Note: Call GeomtryUtil::PackUint8x4 to create the two following members
		// 4x uint8 packed into one uint32
		uint boneIndices;    

		// 4x UNORM8 packed into one uint32
		uint boneWeights;      
	};

	TYR_SHADER_STRUCT(Meshlet)
	{
		// Local offsets
		uint vertexOffset;
		uint vertexCount;
		uint indexOffset;
		uint indexCount;
		// Global index into the material buffer - a mesh's submeshes can each use a
		// different material, so this lives per-meshlet rather than per-instance.
		uint materialIndex;
	};

	TYR_SHADER_STRUCT(MeshLOD)
	{
		// Global offsets
		uint vertexOffset;
		uint indexOffset;
		uint meshletOffset;
		uint meshletCount;
	};

	TYR_SHADER_STRUCT(Mesh)
	{
		TYR_SHADER_FLOAT4 sphere;
		TYR_SHADER_FLOAT3 aabbMin;
		TYR_SHADER_FLOAT3 aabbMax;
		uint lodOffset;
		uint lodCount;
	};

	TYR_SHADER_STRUCT(MeshInstance)
	{
		TYR_SHADER_FLOAT4x4 transform;
		uint meshIndex;
		uint materialIndex;
	};

	TYR_SHADER_STRUCT(Skeleton)
	{
		uint parentOffset;
		uint inverseBindOffset;
		uint boneCount;
	};

	TYR_SHADER_STRUCT(SkeletalMesh)
	{
		uint lodOffset;
		uint lodCount;

		uint boneRemapOffset; // into boneRemapBuffer
		uint boneCount;
	};

	TYR_SHADER_STRUCT(SkeletalInstance)
	{
		TYR_SHADER_FLOAT4x4 transform;
		uint meshIndex;
		uint materialIndex;
		uint boneMatrixOffset;
	};

#ifdef __cplusplus
}
#endif

#endif

