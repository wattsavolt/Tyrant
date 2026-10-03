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
// Compute-only output storage image for the deferred lighting pass, not part of the
// bindless sampled textures[] array.
#define TYR_BINDING_LIGHTING_OUTPUT 14
// GPU-driven instance culling data, written by a compute pre-pass and read by later
// draw-time shaders.
#define TYR_BINDING_ACTIVE_INSTANCE_INDICES 15
#define TYR_BINDING_VISIBLE_INSTANCE_INDICES 16
#define TYR_BINDING_INDIRECT_DRAW_COMMANDS 17
#define TYR_BINDING_DRAW_COUNT 18
// One top-level acceleration structure per buffered RenderFrame slot, indexed the same way
// TYR_BINDING_LIGHTING_OUTPUT is - bound once at creation (the handle is rebuilt in place every
// frame, never recreated), not re-bound per frame.
#define TYR_BINDING_TLAS 19
// One Texture2DArray per buffered RenderFrame slot (one array layer per shadow-casting light
// slot that tick) - RAW is the shadow compute pass's own output, SHADOW_MASKS is the denoiser's
// output/temporal history. Both read and written only via Load()/indexed storage access, never
// sampled/filtered, so a single storage-image binding covers every pass that touches them.
#define TYR_BINDING_SHADOW_MASKS_RAW 20
#define TYR_BINDING_SHADOW_MASKS 21
// DeferredLightingCS's light-index -> shadow-slot lookup for point/spot lights, rebuilt and
// re-uploaded every tick. Point entries first, then spot - see
// RenderConstants::c_ShadowLightSlotMapEntryCount.
#define TYR_BINDING_SHADOW_LIGHT_SLOT_MAP 22

// How many copies of the per-frame render targets TYR_BINDING_LIGHTING_OUTPUT holds, one
// per buffered frame slot - kept in sync with the matching C++ constant.
#define TYR_BUFFERED_FRAME_COUNT 3

// Where spot-light entries start in TYR_BINDING_SHADOW_LIGHT_SLOT_MAP (point entries come
// first) - kept in sync with RenderConstants::c_MaxPointLights.
#define TYR_MAX_POINT_LIGHTS 16

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
	// time, so this always describes that scene's sole view.
	TYR_SHADER_STRUCT(SceneInfo)
	{
		TYR_SHADER_FLOAT4x4 viewProj;
		// Used by the deferred lighting pass to reconstruct world position from depth.
		TYR_SHADER_FLOAT4x4 invViewProj;
		TYR_SHADER_FLOAT3 camPos;
		float ambient;
		// How many of the light buffers' entries are actually populated - each is sized for a
		// fixed max capacity, and looping past these counts would read uninitialized GPU
		// memory, corrupting the summed lighting result.
		uint dirLightCount;
		uint pointLightCount;
		uint spotLightCount;
		// camPos+ambient above already exactly fill a 16-byte cbuffer slot, so dirLightCount
		// starts a fresh one on the HLSL side regardless of what follows it - pad the C++ side
		// to match that same 16-byte size exactly, rather than leaving it implicitly smaller.
		float _pad0;
		// World-space frustum planes extracted from viewProj (Gribb-Hartmann), normalized so
		// xyz is a unit normal - used by GPU instance culling. Order: left, right, bottom,
		// top, near, far.
		TYR_SHADER_FLOAT4 frustumPlanes[6];
		// Last frame's viewProj, used to compute screen-space motion vectors (current NDC
		// minus reprojected previous-frame NDC). Camera motion only for now - per-instance
		// motion needs each instance's own previous transform, not tracked yet.
		TYR_SHADER_FLOAT4x4 prevViewProj;
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
		// PBR textures: 0=albedo, 1=RGB normal/A height, 2=R ambient occlusion/G roughness/B metallic.
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
		// Which of the mesh's submeshes this meshlet belongs to, not a resolved material
		// buffer index - the drawn instance's own material list resolves this slot to an
		// actual material.
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
		// One resolved material index per submesh slot, defaulted from the mesh asset's own
		// materials and overridden per-slot where set. Array size must match the engine's
		// max submesh count; slots beyond the mesh's actual submesh count are unused.
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
		// One resolved material index per submesh slot. Array size must match the engine's
		// max skeletal submesh count exactly.
		uint materialIndices[24];
	};

#ifdef __cplusplus
}
#endif

#endif

