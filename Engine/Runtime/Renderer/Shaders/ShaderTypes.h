#ifndef SHADER_TYPES_H
#define SHADER_TYPES_H

// Descriptor set 0 bindings - shared between the C++ pipeline/descriptor set setup and the
// HLSL shaders so both sides always agree on the numbers.
#define TYR_BINDING_SCENE_INFO 0
#define TYR_BINDING_MESH 1
#define TYR_BINDING_MESH_LOD 2
#define TYR_BINDING_MESHLET 3
#define TYR_BINDING_VERTEX 4
#define TYR_BINDING_INDEX 5
#define TYR_BINDING_MESH_INSTANCE 6
#define TYR_BINDING_MATERIAL 7
#define TYR_BINDING_DIR_LIGHT 8
#define TYR_BINDING_POINT_LIGHT 9
#define TYR_BINDING_SPOT_LIGHT 10
#define TYR_BINDING_TEXTURES 11
#define TYR_BINDING_SAMPLERS 12
#define TYR_BINDING_GUI_VERTEX 13

// Must match ModelImporter's meshlet build limits.
#define TYR_MAX_MESHLET_VERTICES 64
#define TYR_MAX_MESHLET_TRIANGLES 124

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

	// Single-view scene data for the currently rendered scene - only one scene renders at a
	// time (see Scene's own comment), so this always describes that scene's sole view.
	TYR_SHADER_STRUCT(SceneInfo)
	{
		TYR_SHADER_FLOAT4x4 viewProj;
		TYR_SHADER_FLOAT3 camPos;
		float ambient;
		// How many of directionalLightBuffer/pointLightBuffer/spotLightBuffer's entries are
		// actually populated - each buffer is sized for its own RenderConstants::c_Max*Lights,
		// so a shader looping the full fixed capacity instead of these counts would read
		// whichever slots nothing has ever written to, which is not guaranteed to be zeroed
		// (uninitialized GPU memory, e.g. validation-layer poison fill) - one such light's
		// garbage intensity/colour/direction corrupts the entire summed result. See
		// MeshPS.hlsl's light loops.
		uint dirLightCount;
		uint pointLightCount;
		uint spotLightCount;
		// camPos+ambient above already exactly fill a 16-byte cbuffer slot, so dirLightCount
		// starts a fresh one on the HLSL side regardless of what follows it - pad the C++ side
		// to match that same 16-byte size exactly, rather than leaving it implicitly smaller.
		float _pad0;
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
		// Which of the mesh's submeshes this meshlet belongs to - NOT a resolved material
		// buffer index. The instance being drawn is what actually decides materials (see
		// MeshInstance::materialIndices below); this is just which slot of that array to
		// read, since a mesh's submeshes can each use a different material.
		uint materialSlot;
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
		// One resolved material buffer index per submesh slot (see Meshlet::materialSlot
		// above) - this instance's own materials, defaulted from the mesh asset's own
		// authored materials and overridden per-slot where a MeshComponent's overrides say
		// so. Array size must match MeshConstants::c_MaxSubmeshes exactly. Slots
		// beyond the mesh's actual submesh count are unused/ignored.
		uint materialIndices[16];
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
		uint boneMatrixOffset;
		// One resolved material buffer index per submesh slot - see
		// ShaderMeshInstance::materialIndices' comment above. Array size must match
		// SkeletalMeshConstants::c_MaxSubmeshes exactly.
		uint materialIndices[24];
	};

#ifdef __cplusplus
}
#endif

#endif

