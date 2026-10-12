#ifndef MESH_TASK_PAYLOAD_HLSli
#define MESH_TASK_PAYLOAD_HLSli

#include "ShaderTypes.h"

// What a task shader group hands its mesh shader groups - one group per surviving meshlet.
struct MeshTaskPayload
{
	uint meshInstanceIndex;
	uint globalVertexBase;
	// The same as globalVertexBase unless the instance is skinned.
	uint prevGlobalVertexBase;
	uint globalIndexBase;
	uint meshletIndices[TYR_MESHLETS_PER_TASK_GROUP];
};

#endif
