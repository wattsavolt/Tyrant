#include "VulkanAccelerationStructure.h"
#include "VulkanDevice.h"
#include "VulkanExtensions.h"

namespace tyr
{
	namespace
	{
		VkIndexType ToVulkanIndexType(IndexType type)
		{
			return type == IndexType::UInt16 ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32;
		}
	}

	AccelerationStructureHandle Device::CreateAccelerationStructure(const AccelerationStructureDesc& desc)
	{
		DeviceInternal& device = static_cast<DeviceInternal&>(*this);
		const AccelerationStructureHandle handle(device.m_AccelerationStructurePool.Create());
		AccelerationStructure& as = device.m_AccelerationStructurePool[handle.h];
		as.desc = desc;

		const bool isTopLevel = desc.type == AccelerationStructureType::TopLevel;
		const VkAccelerationStructureTypeKHR vkType = isTopLevel
			? VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR
			: VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;

		// Only counts/formats are needed to size the structure - real buffer addresses aren't
		// known/needed until an actual build (see CommandList::BuildAccelerationStructures).
		LocalArray<VkAccelerationStructureGeometryKHR, 1> vkGeometries;
		LocalArray<uint32_t, 1> maxPrimitiveCounts;

		if (isTopLevel)
		{
			VkAccelerationStructureGeometryKHR& geom = vkGeometries.ExpandOne();
			geom = {};
			geom.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
			geom.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
			geom.geometry.instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
			geom.geometry.instances.arrayOfPointers = VK_FALSE;
			maxPrimitiveCounts.Add(desc.maxInstanceCount);
		}
		else
		{
			for (const AccelerationStructureGeometryDesc& geomDesc : desc.geometries)
			{
				VkAccelerationStructureGeometryKHR& geom = vkGeometries.ExpandOne();
				geom = {};
				geom.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
				geom.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
				VkAccelerationStructureGeometryTrianglesDataKHR& tri = geom.geometry.triangles;
				tri.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
				tri.vertexFormat = VulkanUtility::ToVulkanPixelFormat(geomDesc.vertexFormat);
				tri.vertexStride = geomDesc.vertexStride;
				tri.maxVertex = geomDesc.maxVertexCount > 0 ? geomDesc.maxVertexCount - 1 : 0;
				tri.indexType = ToVulkanIndexType(geomDesc.indexType);
				geom.flags = geomDesc.isOpaque ? VK_GEOMETRY_OPAQUE_BIT_KHR : 0;
				maxPrimitiveCounts.Add(geomDesc.maxPrimitiveCount);
			}
		}

		VkAccelerationStructureBuildGeometryInfoKHR buildInfo{};
		buildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
		buildInfo.type = vkType;
		buildInfo.flags = static_cast<VkBuildAccelerationStructureFlagsKHR>(desc.buildFlags);
		buildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
		buildInfo.geometryCount = vkGeometries.Size();
		buildInfo.pGeometries = vkGeometries.Data();

		VkAccelerationStructureBuildSizesInfoKHR buildSizes{};
		buildSizes.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
		vkGetAccelerationStructureBuildSizesKHR(device.m_LogicalDevice, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
			&buildInfo, maxPrimitiveCounts.Data(), &buildSizes);

		BufferDesc bufferDesc;
		bufferDesc.debugName = desc.debugName;
		bufferDesc.usage = static_cast<BufferUsage>(BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT | BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
		bufferDesc.memoryProperty = MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
		bufferDesc.size = static_cast<size_t>(buildSizes.accelerationStructureSize);
		as.backingBuffer = CreateBuffer(bufferDesc);
		const Buffer& backingBuffer = device.GetBuffer(as.backingBuffer);

		VkAccelerationStructureCreateInfoKHR createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
		createInfo.buffer = backingBuffer.buffer;
		createInfo.offset = 0;
		createInfo.size = buildSizes.accelerationStructureSize;
		createInfo.type = vkType;
		TYR_GASSERT(vkCreateAccelerationStructureKHR(device.m_LogicalDevice, &createInfo, g_VulkanAllocationCallbacks, &as.accelerationStructure));

		TYR_SET_GFX_DEBUG_NAME(device.m_LogicalDevice, desc.debugName, VK_OBJECT_TYPE_ACCELERATION_STRUCTURE_KHR, reinterpret_cast<uint64>(as.accelerationStructure));

		VkAccelerationStructureDeviceAddressInfoKHR addressInfo{};
		addressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
		addressInfo.accelerationStructure = as.accelerationStructure;
		as.deviceAddress = vkGetAccelerationStructureDeviceAddressKHR(device.m_LogicalDevice, &addressInfo);

		as.buildScratchSize = buildSizes.buildScratchSize;
		as.updateScratchSize = buildSizes.updateScratchSize;

		return handle;
	}

	void Device::DeleteAccelerationStructure(AccelerationStructureHandle handle)
	{
		DeviceInternal& device = static_cast<DeviceInternal&>(*this);
		AccelerationStructure& as = device.GetAccelerationStructure(handle);
		vkDestroyAccelerationStructureKHR(device.m_LogicalDevice, as.accelerationStructure, g_VulkanAllocationCallbacks);
		DeleteBuffer(as.backingBuffer);
		device.m_AccelerationStructurePool.Delete(handle.h);
	}

	uint64 Device::GetAccelerationStructureDeviceAddress(AccelerationStructureHandle handle) const
	{
		const DeviceInternal& device = static_cast<const DeviceInternal&>(*this);
		return static_cast<uint64>(device.GetAccelerationStructure(handle).deviceAddress);
	}

	size_t Device::GetAccelerationStructureBuildScratchSize(AccelerationStructureHandle handle) const
	{
		const DeviceInternal& device = static_cast<const DeviceInternal&>(*this);
		return static_cast<size_t>(device.GetAccelerationStructure(handle).buildScratchSize);
	}

	size_t Device::GetAccelerationStructureUpdateScratchSize(AccelerationStructureHandle handle) const
	{
		const DeviceInternal& device = static_cast<const DeviceInternal&>(*this);
		return static_cast<size_t>(device.GetAccelerationStructure(handle).updateScratchSize);
	}
}
