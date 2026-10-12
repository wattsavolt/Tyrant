#pragma once

#include "EngineMacros.h"
#include "Core.h"
#include "TextureAsset.h"
#include "RenderResource/MeshDesc.h"
#include "Shaders/ShaderTypes.h"

namespace tyr
{
	// The mesh asset file has one mesh header and a lod header per lod. It can have any number of chunks.
	// Each chunk has a non-encrypted chunk header and Zstd encrypted blob containing the meshlets, vertices and indices. 
	// Meshlet, vertex and offsets are local to chunk indices and vertices. 
	// Each chunk will only have meshlets from the same LOD. No LOD mixing in a chunk!
	// Originally, all chunks will be loaded and one chunk per LOD but could be streamed in later based on distance/visibilty. 

	struct MeshChunkHeader
	{
		uint decompressedMeshletsSize;
		uint decompressedVerticesSize;
		uint decompressedIndicesSize;
		// Size of this chunk's compressed blob on disk - also how far to seek forward to
		// reach the next chunk, since chunks are written back-to-back with no gaps.
		uint compressedBlobSize;
		// Size of this chunk's data once decompressed - not just the sum of the three
		// sizes above, since meshlets/vertices/indices are each aligned within the blob
		// (see meshletsOffset/verticesOffset/indicesOffset).
		uint decompressedBlobSize;
		uint meshletsOffset;
		uint verticesOffset;
		uint indicesOffset;
	};

	// One generated meshlet's local ranges into a chunk's decompressed meshlet-local index
	// buffer / mesh vertex buffer, its material slot and its culling bounds. The GPU's own
	// layout, so a chunk's decompressed meshlet bytes are uploaded as they are.
	using MeshChunkMeshlet = ShaderMeshlet;

	struct MeshLODHeader
	{
		uint chunkOffset = 0;
		uint chunkCount = 0;
		// Submeshes for this LOD only - a submesh's meshlet range/count can differ per LOD.
		uint submeshOffset = 0;
		uint submeshCount = 0;
		// How far this LOD's surface can be from LOD0's, in mesh-space units.
		float error = 0.0f;
	};

	// Where a skinned mesh chunk's bone weights sit in its decompressed blob - one
	// ShaderSkinVertex per vertex, in the same order as the chunk's vertices.
	struct MeshChunkSkin
	{
		uint skinOffset = 0;
		uint decompressedSkinSize = 0;
	};

	// One material-homogeneous group of meshlets within a single LOD (matches a glTF
	// primitive, or Unreal's "section" - same idea under different names).
	struct MeshSubmesh
	{
		uint meshletOffset = 0;
		uint meshletCount = 0;
		// Index into MeshHeader::materials.
		uint materialSlot = 0;
	};

	// For rigid meshes
	// The default material will be added to the material component of the entity generated when importing the mesh (same for skeletal)
	struct MeshHeader
	{
		// Written first and bumped whenever the layout changes, so an older file is rejected
		// instead of misread. The high half is a tag no older file starts with.
		static constexpr uint c_FileVersion = 0x544D0002;
		uint fileVersion = c_FileVersion;

		LocalArray<MeshLODHeader, MeshConstants::c_MaxLods> lods;
		// Currently there will be only one chunk per LOD until streamed chunks are supported.
		// This could be handy later but could be optimized to a local array if chunk streaming not used and becomes a bottleneck
		Array<MeshChunkHeader> chunks;
		// Flat list of submeshes across all LODs - each MeshLODHeader indexes its own
		// range via submeshOffset/submeshCount.
		Array<MeshSubmesh> submeshes;
		// This mesh's material slots (referenced by MeshSubmesh::materialSlot), one
		// AssetID per slot - mirrors how a glTF mesh's primitives each reference an
		// index into the asset's shared materials list.
		Array<AssetID> materials;
		BoundingSphere sphere;
		Vector3 aabbMin;
		Vector3 aabbMax;

		// Skinned meshes only, which are saved with SerializeSkinnedMesh. One entry per chunk.
		Array<MeshChunkSkin> chunkSkins;
		AssetID skeleton;
		// The furthest any vertex sits from its most weighted bone, for bounding posed meshes.
		float boneBoundsMargin = 0.0f;

		bool IsSkinned() const { return !chunkSkins.IsEmpty(); }
		bool IsCurrentVersion() const { return fileVersion == c_FileVersion; }

		void Reset()
		{
			fileVersion = c_FileVersion;
			lods.Clear();
			chunks.Clear();
			submeshes.Clear();
			materials.Clear();
			sphere = {};
			aabbMin = {};
			aabbMax = {};
			chunkSkins.Clear();
			skeleton = {};
			boneBoundsMargin = 0.0f;
		}
	};

	template<> void Serialize(BufferedFileStream& stream, const MeshChunkHeader& header);
	template<> void Deserialize(BufferedFileStream& stream, MeshChunkHeader& header);

	template<> void Serialize(BufferedFileStream& stream, const MeshLODHeader& header);
	template<> void Deserialize(BufferedFileStream& stream, MeshLODHeader& header);

	template<> void Serialize(BufferedFileStream& stream, const MeshSubmesh& submesh);
	template<> void Deserialize(BufferedFileStream& stream, MeshSubmesh& submesh);

	template<> void Serialize(BufferedFileStream& stream, const MeshHeader& header);
	template<> void Deserialize(BufferedFileStream& stream, MeshHeader& header);

	// A .skmesh header is a .mesh header followed by its skinning data.
	void SerializeSkinnedMesh(BufferedFileStream& stream, const MeshHeader& header);
	void DeserializeSkinnedMesh(BufferedFileStream& stream, MeshHeader& header);
}