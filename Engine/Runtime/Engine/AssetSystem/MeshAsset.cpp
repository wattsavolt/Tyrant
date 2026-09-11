#include "MeshAsset.h"

namespace tyr
{
	template<>
	void Serialize(BufferedFileStream& stream, const MeshChunkHeader& header)
	{
		// TODO: Implement
	}

	template<>
	void Deserialize(BufferedFileStream& stream, MeshChunkHeader& header)
	{
		// TODO: Implement
	}

	template<>
	void Serialize(BufferedFileStream& stream, const MeshLODHeader& header)
	{
		// TODO: Implement
	}

	template<>
	void Deserialize(BufferedFileStream& stream, MeshLODHeader& header)
	{
		// TODO: Implement
	}

	template<>
	void Serialize(BufferedFileStream& stream, const MeshHeader& header)
	{
		// TODO: Implement
	}

	template<>
	void Deserialize(BufferedFileStream& stream, MeshHeader& header)
	{
		header.chunks.Reserve(MeshConstants::c_MaxLods);
		// TODO: Implement rest
	}
}
