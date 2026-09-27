
#pragma once

#include "GraphicsBase.h"
#include "RenderAPITypes.h"
#include "Buffer.h"

namespace tyr
{
	TYR_CREATE_HANDLE_TYPE(AccelerationStructureHandle);

	// One triangle geometry going into a bottom-level acceleration structure (BLAS) - a mesh's
	// vertex/index buffers, the same ones already used for rasterization. A BLAS can have more
	// than one of these (eg. per-submesh), but only one is needed for now.
	struct AccelerationStructureGeometryDesc
	{
		BufferHandle vertexBuffer;
		size_t vertexOffset = 0;
		uint vertexStride = 0;
		uint maxVertexCount = 0;
		PixelFormat vertexFormat = PixelFormat::PF_R32G32B32_SFLOAT;
		BufferHandle indexBuffer;
		size_t indexOffset = 0;
		IndexType indexType = IndexType::UInt32;
		uint maxPrimitiveCount = 0;
		bool isOpaque = true;
	};

	struct AccelerationStructureDesc
	{
		TYR_DECLARE_GDEBUGNAME(debugName);
		AccelerationStructureType type = AccelerationStructureType::BottomLevel;
		AccelerationStructureBuildFlags buildFlags = AccelerationStructureBuildFlags::ACCELERATION_STRUCTURE_BUILD_PREFER_FAST_TRACE_BIT;
		// BottomLevel only - ignored for TopLevel.
		LocalArray<AccelerationStructureGeometryDesc, 1> geometries;
		// TopLevel only - the most instances this TLAS will ever be built/updated with.
		uint maxInstanceCount = 0;
	};

	// Actual per-build parameters for CommandList::BuildAccelerationStructures - as opposed to
	// AccelerationStructureDesc, which only describes what's needed to size the structure once
	// at creation. A bottom-level build reuses its own AccelerationStructureGeometryDesc (a
	// mesh's vertex/index buffers don't change), so needs nothing extra here; a top-level build
	// needs the instance buffer, since its contents are rewritten every frame.
	struct AccelerationStructureBuildInfo
	{
		AccelerationStructureHandle accelerationStructure;
		// TopLevel only - ignored for BottomLevel.
		BufferHandle instanceBuffer;
		size_t instanceBufferOffset = 0;
		uint instanceCount = 0;
		BufferHandle scratchBuffer;
		size_t scratchOffset = 0;
		// Fast refit instead of a full rebuild - only valid if this acceleration structure was
		// created with ACCELERATION_STRUCTURE_BUILD_ALLOW_UPDATE_BIT.
		bool update = false;
	};

	// One entry in a top-level acceleration structure's instance buffer. Matches
	// VkAccelerationStructureInstanceKHR's layout (also identical in shape to D3D12's
	// D3D12_RAYTRACING_INSTANCE_DESC) so it can be written straight into a GPU buffer and read
	// by the build with no conversion step.
	struct AccelerationStructureInstance
	{
		// Row-major 3x4 affine transform.
		float transform[3][4];
		uint instanceCustomIndex : 24;
		uint mask : 8;
		uint shaderBindingTableRecordOffset : 24;
		uint flags : 8;
		// A bottom-level acceleration structure's own device address - see
		// Device::GetAccelerationStructureDeviceAddress.
		uint64 accelerationStructureReference;
	};
}
