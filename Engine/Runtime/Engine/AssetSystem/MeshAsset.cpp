#include "MeshAsset.h"

namespace tyr
{
	template<>
	void Serialize(BufferedFileStream& stream, const MeshChunkHeader& header)
	{
		Serialize<uint>(stream, header.decompressedMeshletsSize);
		Serialize<uint>(stream, header.decompressedVerticesSize);
		Serialize<uint>(stream, header.decompressedIndicesSize);
		Serialize<uint>(stream, header.compressedBlobSize);
		Serialize<uint>(stream, header.decompressedBlobSize);
		Serialize<uint>(stream, header.blobFileOffset);
		Serialize<uint>(stream, header.meshletsOffset);
		Serialize<uint>(stream, header.verticesOffset);
		Serialize<uint>(stream, header.indicesOffset);
	}

	template<>
	void Deserialize(BufferedFileStream& stream, MeshChunkHeader& header)
	{
		Deserialize<uint>(stream, header.decompressedMeshletsSize);
		Deserialize<uint>(stream, header.decompressedVerticesSize);
		Deserialize<uint>(stream, header.decompressedIndicesSize);
		Deserialize<uint>(stream, header.compressedBlobSize);
		Deserialize<uint>(stream, header.decompressedBlobSize);
		Deserialize<uint>(stream, header.blobFileOffset);
		Deserialize<uint>(stream, header.meshletsOffset);
		Deserialize<uint>(stream, header.verticesOffset);
		Deserialize<uint>(stream, header.indicesOffset);
	}

	template<>
	void Serialize(BufferedFileStream& stream, const MeshLODHeader& header)
	{
		Serialize<uint>(stream, header.chunkOffset);
		Serialize<uint>(stream, header.chunkCount);
		Serialize<uint>(stream, header.submeshOffset);
		Serialize<uint>(stream, header.submeshCount);
	}

	template<>
	void Deserialize(BufferedFileStream& stream, MeshLODHeader& header)
	{
		Deserialize<uint>(stream, header.chunkOffset);
		Deserialize<uint>(stream, header.chunkCount);
		Deserialize<uint>(stream, header.submeshOffset);
		Deserialize<uint>(stream, header.submeshCount);
	}

	template<>
	void Serialize(BufferedFileStream& stream, const MeshSubmesh& submesh)
	{
		Serialize<uint>(stream, submesh.meshletOffset);
		Serialize<uint>(stream, submesh.meshletCount);
		Serialize<uint>(stream, submesh.materialSlot);
	}

	template<>
	void Deserialize(BufferedFileStream& stream, MeshSubmesh& submesh)
	{
		Deserialize<uint>(stream, submesh.meshletOffset);
		Deserialize<uint>(stream, submesh.meshletCount);
		Deserialize<uint>(stream, submesh.materialSlot);
	}

	template<>
	void Serialize(BufferedFileStream& stream, const MeshHeader& header)
	{
		Serialize(stream, header.lods);
		Serialize(stream, header.chunks);
		Serialize(stream, header.submeshes);
		Serialize(stream, header.materials);
		stream.Write(&header.sphere, sizeof(header.sphere));
		stream.Write(&header.aabbMin, sizeof(header.aabbMin));
		stream.Write(&header.aabbMax, sizeof(header.aabbMax));
	}

	template<>
	void Deserialize(BufferedFileStream& stream, MeshHeader& header)
	{
		Deserialize(stream, header.lods);
		Deserialize(stream, header.chunks);
		Deserialize(stream, header.submeshes);
		Deserialize(stream, header.materials);
		stream.Read(&header.sphere, sizeof(header.sphere));
		stream.Read(&header.aabbMin, sizeof(header.aabbMin));
		stream.Read(&header.aabbMax, sizeof(header.aabbMax));
	}
}
