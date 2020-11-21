#pragma once
#include <vulkan/vulkan.hpp>
#include "vma.h"

struct AccelerationMemory {
	vk::UniqueBuffer buffer;
	vk::DeviceMemory memory;
	uint64_t memoryAddress = 0;
	void* mappedPointer = nullptr;
};

class RtPass {
public:
	RtPass() = default;
	RtPass(RtPass&&) = default;
	RtPass& operator=(RtPass&&) = default;

	void issueCommands(vk::CommandBuffer commandBuffer, vk::Framebuffer framebuffer, vk::Extent2D extent, vk::Image swapchainImage, vk::DispatchLoaderDynamic dld) {
		vk::ImageSubresourceRange subresourceRange;
		subresourceRange
			.setAspectMask(vk::ImageAspectFlagBits::eColor)
			.setBaseMipLevel(0)
			.setLevelCount(1)
			.setBaseArrayLayer(0)
			.setLayerCount(1);

		vk::ImageCopy copyRegion;
		copyRegion
			.setSrcOffset({ 0, 0, 0 })
			.setDstOffset({ 0, 0, 0 });

		copyRegion.srcSubresource
			.setAspectMask(vk::ImageAspectFlagBits::eColor)
			.setMipLevel(0)
			.setBaseArrayLayer(0)
			.setLayerCount(1);

		copyRegion.dstSubresource
			.setAspectMask(vk::ImageAspectFlagBits::eColor)
			.setMipLevel(0)
			.setBaseArrayLayer(0)
			.setLayerCount(1);

		copyRegion.extent
			.setDepth(1)
			.setWidth(extent.width)
			.setHeight(extent.height);

		// Offscreen buffer -> Shader writeable state
		InsertCommandImageBarrier(commandBuffer, _offscreenBuffer.get(), {},
			vk::AccessFlagBits::eShaderWrite,
			vk::ImageLayout::eUndefined, vk::ImageLayout::eGeneral,
			subresourceRange);

		commandBuffer.bindPipeline(vk::PipelineBindPoint::eRayTracingKHR, _pipelines[0].get());
		commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eRayTracingKHR, _pipelineLayout.get(), 0, { _descriptorSet.get() }, {});
		commandBuffer.traceRaysKHR(rayGenSBT, rayMissSBT, rayHitSBT, rayCallSBT, extent.width, extent.height, 1, dld);

		// Swapchain image -> Copy destination state
		InsertCommandImageBarrier(commandBuffer, swapchainImage, {},
			vk::AccessFlagBits::eTransferWrite,
			vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal,
			subresourceRange);

		// Offscreen buffer -> Copy source state
		InsertCommandImageBarrier(commandBuffer, _offscreenBuffer.get(), vk::AccessFlagBits::eShaderWrite,
			vk::AccessFlagBits::eTransferRead, vk::ImageLayout::eGeneral, vk::ImageLayout::eTransferSrcOptimal,
			subresourceRange);

		// Offscreen buffer -> Swapchain image
		commandBuffer.copyImage(_offscreenBuffer.get(), vk::ImageLayout::eTransferSrcOptimal,
			swapchainImage, vk::ImageLayout::eTransferDstOptimal, copyRegion);

		// Swapchain image -> Presentable state
		InsertCommandImageBarrier(commandBuffer, swapchainImage, {},
			vk::AccessFlagBits::eTransferWrite,
			vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::ePresentSrcKHR,
			subresourceRange);
	}

	struct PipelineCreationInfo
	{
		[[nodiscard]] inline static vk::RayTracingShaderGroupCreateInfoKHR
			getRtGenShaderGroupCreate()
		{
			vk::RayTracingShaderGroupCreateInfoKHR info;
			info.type = vk::RayTracingShaderGroupTypeKHR::eGeneral;
			info.generalShader = 0;
			info.closestHitShader = VK_SHADER_UNUSED_KHR;
			info.anyHitShader = VK_SHADER_UNUSED_KHR;
			info.intersectionShader = VK_SHADER_UNUSED_KHR;
			info.pShaderGroupCaptureReplayHandle = nullptr;

			return info;
		}
		[[nodiscard]] inline static vk::RayTracingShaderGroupCreateInfoKHR
			getRtHitShaderGroupCreate()
		{
			vk::RayTracingShaderGroupCreateInfoKHR info;
			info.type = vk::RayTracingShaderGroupTypeKHR::eTrianglesHitGroup;
			info.generalShader = VK_SHADER_UNUSED_KHR;
			info.closestHitShader = 1;
			info.anyHitShader = VK_SHADER_UNUSED_KHR;
			info.intersectionShader = VK_SHADER_UNUSED_KHR;
			info.pShaderGroupCaptureReplayHandle = nullptr;

			return info;
		}
		[[nodiscard]] inline static vk::RayTracingShaderGroupCreateInfoKHR
			getRtMissShaderGroupCreate()
		{
			vk::RayTracingShaderGroupCreateInfoKHR info;
			info.type = vk::RayTracingShaderGroupTypeKHR::eGeneral;
			info.generalShader = 2;
			info.closestHitShader = VK_SHADER_UNUSED_KHR;
			info.anyHitShader = VK_SHADER_UNUSED_KHR;
			info.intersectionShader = VK_SHADER_UNUSED_KHR;
			info.pShaderGroupCaptureReplayHandle = nullptr;

			return info;
		}

		std::vector<vk::PipelineShaderStageCreateInfo> shaderStages;
		std::vector<vk::RayTracingShaderGroupCreateInfoKHR> shaderGroups;
	};

	void createDescriptorSetForRayTracing(vk::Device& dev,
		vk::DescriptorPool& pool,
		vk::DispatchLoaderDynamic& dld)
	{
		vk::DescriptorSetAllocateInfo setInfo;
		setInfo
			.setDescriptorPool(pool)
			.setDescriptorSetCount(1)
			.setSetLayouts(_descriptorSetLayout.get());

		_descriptorSet = std::move(dev.allocateDescriptorSetsUnique(setInfo)[0]);

		vk::WriteDescriptorSetAccelerationStructureKHR descriptorAccelerationStructureInfo;
		descriptorAccelerationStructureInfo.
			setAccelerationStructureCount(1)
			.setAccelerationStructures(_topLevelAS.get());

		vk::WriteDescriptorSet accelerationStructureWrite;
		accelerationStructureWrite
			.setPNext(&descriptorAccelerationStructureInfo)
			.setDstSet(_descriptorSet.get())
			.setDstBinding(0)
			.setDstArrayElement(0)
			.setDescriptorType(vk::DescriptorType::eAccelerationStructureKHR)
			.setDescriptorCount(1);

		vk::DescriptorImageInfo storageImageInfo;
		storageImageInfo.imageView = _offscreenBufferView.get();
		storageImageInfo.imageLayout = vk::ImageLayout::eGeneral;

		vk::WriteDescriptorSet outputImageWrite;
		outputImageWrite
			.setDstSet(_descriptorSet.get())
			.setDstBinding(1)
			.setDescriptorType(vk::DescriptorType::eStorageImage)
			.setDescriptorCount(1)
			.setDstArrayElement(0)
			.setImageInfo(storageImageInfo);

		dev.updateDescriptorSets({ accelerationStructureWrite, outputImageWrite }, {}, dld);
	}

	void createShaderBindingTable(vk::Device& dev, vma::Allocator& allocator, vk::PhysicalDevice& physicalDev, vk::DispatchLoaderDynamic dld)
	{
		vk::PhysicalDeviceRayTracingPropertiesKHR rtProperties;
		vk::PhysicalDeviceProperties2 devProperties2;
		devProperties2.pNext = &rtProperties;

		uint32_t shaderGroupSize = 3;

		physicalDev.getProperties2(&devProperties2);
		uint32_t shaderBindingTableSize = shaderGroupSize * rtProperties.shaderGroupBaseAlignment;

		vk::BufferCreateInfo bufferInfo;
		bufferInfo
			.setSize(shaderBindingTableSize)
			.setUsage(vk::BufferUsageFlagBits::eTransferSrc)
			.setSharingMode(vk::SharingMode::eExclusive);

		_shaderBindingTable.buffer = dev.createBufferUnique(bufferInfo);

		vk::MemoryRequirements memoryRequirements = dev.getBufferMemoryRequirements(_shaderBindingTable.buffer.get());

		vk::MemoryAllocateInfo memAllocInfo;
		memAllocInfo
			.setAllocationSize(memoryRequirements.size)
			.setMemoryTypeIndex(FindMemoryType(physicalDev, memoryRequirements.memoryTypeBits, vk::MemoryPropertyFlagBits::eHostVisible));

		_shaderBindingTable.memory = dev.allocateMemory(memAllocInfo);

		dev.bindBufferMemory(_shaderBindingTable.buffer.get(), _shaderBindingTable.memory, {});

		// Set shader info
		uint8_t* dstData = reinterpret_cast<uint8_t*>(dev.mapMemory(_shaderBindingTable.memory, 0, shaderBindingTableSize, {}));
		std::vector<uint8_t> shaderHandleStorage(shaderBindingTableSize);
		dev.getRayTracingShaderGroupHandlesKHR(_pipelines[0].get(), 0, shaderGroupSize, shaderBindingTableSize, shaderHandleStorage.data(), dld);

		for (uint32_t g = 0; g < shaderGroupSize; g++)
		{
			memcpy(dstData, shaderHandleStorage.data() + g * rtProperties.shaderGroupHandleSize,
				rtProperties.shaderGroupHandleSize);
			dstData += rtProperties.shaderGroupBaseAlignment;
		}

		dev.unmapMemory(_shaderBindingTable.memory);

		// Set buffer region handle
		rayGenSBT
			.setBuffer(_shaderBindingTable.buffer.get())
			.setOffset(0)
			.setStride(rtProperties.shaderGroupHandleSize)
			.setSize(shaderBindingTableSize);

		rayMissSBT
			.setBuffer(_shaderBindingTable.buffer.get())
			.setOffset(2 * rtProperties.shaderGroupBaseAlignment)
			.setStride(rtProperties.shaderGroupBaseAlignment)
			.setSize(shaderBindingTableSize);

		rayHitSBT
			.setBuffer(_shaderBindingTable.buffer.get())
			.setOffset(rtProperties.shaderGroupBaseAlignment)
			.setStride(rtProperties.shaderGroupBaseAlignment)
			.setSize(shaderBindingTableSize);

		rayCallSBT
			.setBuffer({})
			.setOffset(0)
			.setStride(0)
			.setSize(0);
	}

	void createAccelerationStructure(
		vk::Device& dev,
		vk::PhysicalDevice& pd,
		vma::Allocator& allocator,
		vk::DispatchLoaderDynamic& dynamicDispatcher,
		vk::CommandPool& commandPool,
		vk::Queue& queue)
	{
		// Bottom level acceleration structure
		std::vector<float> verticesData = {
		+1.0f, +1.0f, +0.0f,
		-1.0f, +1.0f, +0.0f,
		+0.0f, -1.0f, +0.0f
		};


		std::vector<uint32_t> indicesData = { 0, 1, 2 };

		vk::AccelerationStructureCreateGeometryTypeInfoKHR blasCreateGeoInfo;
		blasCreateGeoInfo.setPNext(nullptr);
		blasCreateGeoInfo.setGeometryType(vk::GeometryTypeKHR::eTriangles);
		blasCreateGeoInfo.setMaxPrimitiveCount(indicesData.size() / 3.0f);
		blasCreateGeoInfo.setIndexType(vk::IndexType::eUint32);
		blasCreateGeoInfo.setMaxVertexCount(verticesData.size() / 3);
		blasCreateGeoInfo.setVertexFormat(vk::Format::eR32G32B32Sfloat);
		blasCreateGeoInfo.setAllowsTransforms(VK_FALSE);

		vk::AccelerationStructureCreateInfoKHR blasCreateInfo;
		blasCreateInfo.setPNext(nullptr);
		blasCreateInfo.setCompactedSize(0);
		blasCreateInfo.setType(vk::AccelerationStructureTypeKHR::eBottomLevel);
		blasCreateInfo.setFlags(vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace);
		blasCreateInfo.setMaxGeometryCount(1);
		blasCreateInfo.setPGeometryInfos(&blasCreateGeoInfo);
		blasCreateInfo.setDeviceAddress(VK_NULL_HANDLE);

		try
		{
			_bottomLevelAS = dev.createAccelerationStructureKHRUnique(blasCreateInfo, nullptr, dynamicDispatcher);
		}
		catch (std::system_error e)
		{
			std::cout << "Error in create bottom level AS" << std::endl;
			exit(-1);
		}

		AccelerationMemory blObjectMemory = CreateAccelerationScratchBuffer(
			dev, pd, allocator, _bottomLevelAS.get(), vk::AccelerationStructureMemoryRequirementsTypeKHR::eObject, dynamicDispatcher);

		BindAccelerationMemory(dev, _bottomLevelAS.get(), blObjectMemory.memory, dynamicDispatcher);

		AccelerationMemory blBuildScratchMemory = CreateAccelerationScratchBuffer(dev, pd, allocator, _bottomLevelAS.get(),
			vk::AccelerationStructureMemoryRequirementsTypeKHR::eBuildScratch, dynamicDispatcher);

		vk::AccelerationStructureDeviceAddressInfoKHR devAddrInfo;
		devAddrInfo.setAccelerationStructure(_bottomLevelAS.get());
		_bottomLevelASHandle = dev.getAccelerationStructureAddressKHR(devAddrInfo, dynamicDispatcher);

		// make sure bottom AS handle is valid
		if (_bottomLevelASHandle == 0) {
			std::cout << "Invalid Handle to BLAS" << std::endl;
			exit(-1);
		}


		vertex = CreateMappedBuffer(dev, pd, verticesData.data(), sizeof(float) * verticesData.size(), dynamicDispatcher);
		index = CreateMappedBuffer(dev, pd, indicesData.data(), sizeof(uint32_t) * indicesData.size(), dynamicDispatcher);

		vk::AccelerationStructureGeometryKHR blasAccelerationGeometry;
		blasAccelerationGeometry.setFlags(vk::GeometryFlagBitsKHR::eOpaque);
		blasAccelerationGeometry.setGeometryType(vk::GeometryTypeKHR::eTriangles);
		blasAccelerationGeometry.geometry.triangles.setVertexFormat(vk::Format::eR32G32B32Sfloat);
		blasAccelerationGeometry.geometry.triangles.vertexData.setDeviceAddress(vertex.memoryAddress);
		blasAccelerationGeometry.geometry.triangles.setVertexStride(3 * sizeof(float));
		blasAccelerationGeometry.geometry.triangles.setIndexType(vk::IndexType::eUint32);
		blasAccelerationGeometry.geometry.triangles.indexData.setDeviceAddress(index.memoryAddress);


		std::vector<vk::AccelerationStructureGeometryKHR> blasAccelerationGeometries(
			{ blasAccelerationGeometry });

		const vk::AccelerationStructureGeometryKHR* blasPpGeometries = blasAccelerationGeometries.data();

		vk::AccelerationStructureBuildGeometryInfoKHR blasAccelerationBuildGeometryInfo;
		blasAccelerationBuildGeometryInfo.setType(vk::AccelerationStructureTypeKHR::eBottomLevel);
		blasAccelerationBuildGeometryInfo.setFlags(vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace);
		blasAccelerationBuildGeometryInfo.setUpdate(VK_FALSE);
		blasAccelerationBuildGeometryInfo.setDstAccelerationStructure(_bottomLevelAS.get());
		blasAccelerationBuildGeometryInfo.setGeometryArrayOfPointers(VK_FALSE);
		blasAccelerationBuildGeometryInfo.setGeometryCount(1);
		blasAccelerationBuildGeometryInfo.setPpGeometries(&blasPpGeometries);
		blasAccelerationBuildGeometryInfo.scratchData.setDeviceAddress(blBuildScratchMemory.memoryAddress);

		vk::AccelerationStructureBuildOffsetInfoKHR blasAccelerationBuildOffsetInfo;
		blasAccelerationBuildOffsetInfo.primitiveCount = 1;
		blasAccelerationBuildOffsetInfo.primitiveOffset = 0x0;
		blasAccelerationBuildOffsetInfo.firstVertex = 0;
		blasAccelerationBuildOffsetInfo.transformOffset = 0x0;

		std::array<vk::AccelerationStructureBuildOffsetInfoKHR*, 1> blasAccelerationBuildOffsets = {
				&blasAccelerationBuildOffsetInfo };

		// Add acceleration command buffer
		std::vector<vk::CommandBuffer> blasCommandBuffers;

		vk::CommandBufferAllocateInfo blasCommandBufferAllocateInfo;
		blasCommandBufferAllocateInfo.commandPool = commandPool;
		blasCommandBufferAllocateInfo.level = vk::CommandBufferLevel::ePrimary;
		blasCommandBufferAllocateInfo.commandBufferCount = 1;

		blasCommandBuffers = dev.allocateCommandBuffers(blasCommandBufferAllocateInfo);

		vk::CommandBufferBeginInfo blasCommandBufferBeginInfo;
		blasCommandBufferBeginInfo.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
		blasCommandBufferBeginInfo.pInheritanceInfo = nullptr;
		blasCommandBuffers.at(0).begin(blasCommandBufferBeginInfo);

		blasCommandBuffers.at(0).buildAccelerationStructureKHR(1, &blasAccelerationBuildGeometryInfo, blasAccelerationBuildOffsets.data(), dynamicDispatcher);

		blasCommandBuffers.at(0).end();

		vk::SubmitInfo blasSubmitInfo;
		blasSubmitInfo.waitSemaphoreCount = 0;
		blasSubmitInfo.pWaitSemaphores = nullptr;
		blasSubmitInfo.pWaitDstStageMask = 0;
		blasSubmitInfo.commandBufferCount = 1;
		blasSubmitInfo.setPCommandBuffers(&(blasCommandBuffers.at(0)));
		blasSubmitInfo.signalSemaphoreCount = 0;
		blasSubmitInfo.pSignalSemaphores = nullptr;

		vk::Fence blasFence;
		vk::FenceCreateInfo blasFenceInfo;

		blasFence = dev.createFence(blasFenceInfo);
		queue.submit(blasSubmitInfo, blasFence);
		dev.waitForFences(blasFence, true, UINT64_MAX);

		dev.destroyFence(blasFence);
		dev.freeCommandBuffers(commandPool, blasCommandBuffers);

		// Top level acceleration structure
		vk::AccelerationStructureCreateGeometryTypeInfoKHR tlasAccelerationCreateGeometryInfo;
		tlasAccelerationCreateGeometryInfo.geometryType = vk::GeometryTypeKHR::eInstances;
		tlasAccelerationCreateGeometryInfo.maxPrimitiveCount = 1;
		tlasAccelerationCreateGeometryInfo.indexType = vk::IndexType::eNoneKHR;
		tlasAccelerationCreateGeometryInfo.maxVertexCount = 0;
		tlasAccelerationCreateGeometryInfo.vertexFormat = vk::Format::eUndefined;
		tlasAccelerationCreateGeometryInfo.allowsTransforms = VK_FALSE;

		vk::AccelerationStructureCreateInfoKHR tlasAccelerationInfo;
		tlasAccelerationInfo.flags = vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace;
		tlasAccelerationInfo.compactedSize = 0;
		tlasAccelerationInfo.type = vk::AccelerationStructureTypeKHR::eTopLevel;
		tlasAccelerationInfo.maxGeometryCount = 1;
		tlasAccelerationInfo.pGeometryInfos = &tlasAccelerationCreateGeometryInfo;
		tlasAccelerationInfo.deviceAddress = VK_NULL_HANDLE;

		try
		{
			_topLevelAS = dev.createAccelerationStructureKHRUnique(tlasAccelerationInfo, nullptr, dynamicDispatcher);
		}
		catch (std::system_error e)
		{
			std::cout << "Error in create top level AS" << std::endl;
			exit(-1);
		}


		AccelerationMemory objectMemory = CreateAccelerationScratchBuffer(dev, pd, allocator,
			_topLevelAS.get(), vk::AccelerationStructureMemoryRequirementsTypeKHR::eObject, dynamicDispatcher);

		BindAccelerationMemory(dev, _topLevelAS.get(), objectMemory.memory, dynamicDispatcher);

		AccelerationMemory buildScratchMemory = CreateAccelerationScratchBuffer(dev, pd, allocator,
			_topLevelAS.get(), vk::AccelerationStructureMemoryRequirementsTypeKHR::eBuildScratch, dynamicDispatcher);

		vk::TransformMatrixKHR transformation = std::array<std::array<float, 4>, 3> {
			1.0f, 0.0f, 0.0f, 0.0f,
				0.0f, 1.0f, 0.0f, 0.0f,
				0.0f, 0.0f, 1.0f, 0.0f
		};

		vk::AccelerationStructureInstanceKHR acInstance;
		acInstance.setTransform(transformation);
		acInstance.setInstanceCustomIndex(0);
		acInstance.setMask(0xFF);
		acInstance.setInstanceShaderBindingTableRecordOffset(0x0);
		acInstance.setFlags(vk::GeometryInstanceFlagBitsKHR::eTriangleFacingCullDisable);
		acInstance.setAccelerationStructureReference(_bottomLevelASHandle);
		std::vector<vk::AccelerationStructureInstanceKHR> instances = { acInstance };

		vk::BufferCreateInfo tlasBufferInfo;
		tlasBufferInfo
			.setSize(static_cast<uint32_t>(sizeof(vk::AccelerationStructureInstanceKHR) * instances.size()))
			.setUsage(vk::BufferUsageFlagBits::eShaderDeviceAddress);

		VmaAllocationCreateInfo allocationInfo{};
		allocationInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;

		instance = CreateMappedBuffer(dev, pd, instances.data(), sizeof(VkAccelerationStructureInstanceKHR) * instances.size(), dynamicDispatcher);

		vk::AccelerationStructureGeometryKHR tlasAccelerationGeometry;
		tlasAccelerationGeometry.flags = vk::GeometryFlagBitsKHR::eOpaque;
		tlasAccelerationGeometry.geometryType = vk::GeometryTypeKHR::eTriangles;
		tlasAccelerationGeometry.geometry = {};
		tlasAccelerationGeometry.geometry.instances.arrayOfPointers = VK_FALSE;
		tlasAccelerationGeometry.geometry.instances.data.deviceAddress = instance.memoryAddress;

		std::vector<vk::AccelerationStructureGeometryKHR> tlasAccelerationGeometries(
			{ tlasAccelerationGeometry });
		const vk::AccelerationStructureGeometryKHR* tlasPpGeometries = tlasAccelerationGeometries.data();

		vk::AccelerationStructureBuildGeometryInfoKHR tlasAccelerationBuildGeometryInfo;
		tlasAccelerationBuildGeometryInfo.type = vk::AccelerationStructureTypeKHR::eTopLevel;
		tlasAccelerationBuildGeometryInfo.flags = vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace;
		tlasAccelerationBuildGeometryInfo.update = VK_FALSE;
		tlasAccelerationBuildGeometryInfo.dstAccelerationStructure = _topLevelAS.get();
		tlasAccelerationBuildGeometryInfo.geometryArrayOfPointers = VK_FALSE;
		tlasAccelerationBuildGeometryInfo.geometryCount = 1;
		tlasAccelerationBuildGeometryInfo.ppGeometries = &tlasPpGeometries;
		tlasAccelerationBuildGeometryInfo.scratchData.deviceAddress = buildScratchMemory.memoryAddress;

		vk::AccelerationStructureBuildOffsetInfoKHR tlasAccelerationBuildOffsetInfo;
		tlasAccelerationBuildOffsetInfo.primitiveCount = 1;
		tlasAccelerationBuildOffsetInfo.primitiveOffset = 0x0;
		tlasAccelerationBuildOffsetInfo.firstVertex = 0;
		tlasAccelerationBuildOffsetInfo.transformOffset = 0x0;

		std::vector<vk::AccelerationStructureBuildOffsetInfoKHR*> accelerationBuildOffsets = {
			&tlasAccelerationBuildOffsetInfo };

		// Add acceleration command buffer
		std::vector<vk::CommandBuffer> tlasCommandBuffers;

		vk::CommandBufferAllocateInfo tlasCommandBufferAllocateInfo;
		tlasCommandBufferAllocateInfo.commandPool = commandPool;
		tlasCommandBufferAllocateInfo.level = vk::CommandBufferLevel::ePrimary;
		tlasCommandBufferAllocateInfo.commandBufferCount = 1;

		tlasCommandBuffers = dev.allocateCommandBuffers(tlasCommandBufferAllocateInfo);

		vk::CommandBufferBeginInfo tlasCommandBufferBeginInfo;
		tlasCommandBufferBeginInfo.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
		tlasCommandBufferBeginInfo.pInheritanceInfo = nullptr;
		tlasCommandBuffers.at(0).begin(tlasCommandBufferBeginInfo);

		tlasCommandBuffers.at(0).buildAccelerationStructureKHR(tlasAccelerationBuildGeometryInfo, accelerationBuildOffsets.at(0), dynamicDispatcher);

		tlasCommandBuffers.at(0).end();

		vk::SubmitInfo tlasSubmitInfo;
		tlasSubmitInfo.waitSemaphoreCount = 0;
		tlasSubmitInfo.pWaitSemaphores = nullptr;
		tlasSubmitInfo.pWaitDstStageMask = 0;
		tlasSubmitInfo.commandBufferCount = 1;
		tlasSubmitInfo.setPCommandBuffers(&(tlasCommandBuffers.at(0)));
		tlasSubmitInfo.signalSemaphoreCount = 0;
		tlasSubmitInfo.pSignalSemaphores = nullptr;

		vk::Fence fence;
		vk::FenceCreateInfo fenceInfo;

		fence = dev.createFence(fenceInfo);
		queue.submit(tlasSubmitInfo, fence);
		dev.waitForFences(fence, true, UINT64_MAX);

		dev.destroyFence(fence);
		dev.freeCommandBuffers(commandPool, tlasCommandBuffers);
	}

	void createOffscreenBuffer(vk::Device& dev, vma::Allocator& allocator, vk::Extent2D extent)
	{
		// Offscreen buffer
		vk::ImageCreateInfo imageInfo;
		imageInfo.imageType = vk::ImageType::e2D;
		imageInfo.format = vk::Format::eB8G8R8A8Unorm;
		imageInfo.extent = vk::Extent3D(extent, 1);
		imageInfo.mipLevels = 1;
		imageInfo.arrayLayers = 1;
		imageInfo.samples = vk::SampleCountFlagBits::e1;
		imageInfo.tiling = vk::ImageTiling::eOptimal;
		imageInfo.usage = vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eTransferSrc;
		imageInfo.sharingMode = vk::SharingMode::eExclusive;
		imageInfo.queueFamilyIndexCount = 0;
		imageInfo.pQueueFamilyIndices = nullptr;
		imageInfo.initialLayout = vk::ImageLayout::eUndefined;

		VmaAllocationCreateInfo allocationInfo{};
		_offscreenBuffer = allocator.createImage(imageInfo, allocationInfo);

		vk::ImageViewCreateInfo imageViewInfo;
		imageViewInfo.viewType = vk::ImageViewType::e2D;
		imageViewInfo.format = vk::Format::eB8G8R8A8Unorm;
		imageViewInfo.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
		imageViewInfo.subresourceRange.baseMipLevel = 0;
		imageViewInfo.subresourceRange.levelCount = 1;
		imageViewInfo.subresourceRange.baseArrayLayer = 0;
		imageViewInfo.subresourceRange.layerCount = 1;
		imageViewInfo.image = _offscreenBuffer.get();
		imageViewInfo.components.r = vk::ComponentSwizzle::eR;
		imageViewInfo.components.g = vk::ComponentSwizzle::eG;
		imageViewInfo.components.b = vk::ComponentSwizzle::eB;
		imageViewInfo.components.a = vk::ComponentSwizzle::eA;

		_offscreenBufferView = dev.createImageViewUnique(imageViewInfo);
	}

	inline static RtPass create(vk::Device dev, vk::DispatchLoaderDynamic& dld)
	{
		RtPass pass = RtPass();
		pass._initialize(dev, dld);
		return std::move(pass);
	}

	void setDispatchLoaderDynamic(vk::DispatchLoaderDynamic& dld)
	{
		_dld = dld;
	}

	vk::DescriptorSet descriptorSet;
	AccelerationMemory _shaderBindingTable;
	vk::StridedBufferRegionKHR rayGenSBT;
	vk::StridedBufferRegionKHR rayMissSBT;
	vk::StridedBufferRegionKHR rayHitSBT;
	vk::StridedBufferRegionKHR rayCallSBT;

protected:

	Shader _rayGen, _rayChit, _rayMiss;
	vk::Format _swapchainFormat;
	vk::UniqueSampler _sampler;
	vk::UniquePipelineLayout _pipelineLayout;
	vk::UniqueDescriptorSetLayout _descriptorSetLayout;
	vk::UniqueDescriptorSet _descriptorSet;

	[[nodiscard]] vk::UniqueRenderPass _createPass(vk::Device);

	[[nodiscard]] std::vector<PipelineCreationInfo> _getPipelineCreationInfo()
	{
		std::vector<PipelineCreationInfo> result;
		PipelineCreationInfo& info = result.emplace_back();
		info.shaderGroups.emplace_back(PipelineCreationInfo::getRtGenShaderGroupCreate());
		info.shaderGroups.emplace_back(PipelineCreationInfo::getRtHitShaderGroupCreate());
		info.shaderGroups.emplace_back(PipelineCreationInfo::getRtMissShaderGroupCreate());
		info.shaderStages.emplace_back(_rayGen.getStageInfo());
		info.shaderStages.emplace_back(_rayChit.getStageInfo());
		info.shaderStages.emplace_back(_rayMiss.getStageInfo());

		return result;
	}

	[[nodiscard]] std::vector<vk::UniqueHandle<vk::Pipeline, vk::DispatchLoaderDynamic>>  _createPipelines(vk::Device dev, vk::DispatchLoaderDynamic& dld) {
		std::vector<PipelineCreationInfo> pipelineInfo = _getPipelineCreationInfo();
		std::vector<vk::UniqueHandle<vk::Pipeline, vk::DispatchLoaderDynamic>> pipelines(pipelineInfo.size());
		for (std::size_t i = 0; i < pipelineInfo.size(); i++)
		{
			const PipelineCreationInfo& info = pipelineInfo[i];

			// Set ray tracing pipeline
			vk::RayTracingPipelineCreateInfoKHR rtPipelineInfo;
			rtPipelineInfo.setStages(info.shaderStages);
			rtPipelineInfo.setGroups(info.shaderGroups);
			rtPipelineInfo.setMaxRecursionDepth(1);
			rtPipelineInfo.setLibraries({});
			rtPipelineInfo.libraries.setLibraryCount(0);
			rtPipelineInfo.setLayout(_pipelineLayout.get());
			pipelines[i] = dev.createRayTracingPipelineKHRUnique(nullptr, rtPipelineInfo, nullptr, dld);
		}
		return pipelines;
	}

	void _initialize(vk::Device dev, vk::DispatchLoaderDynamic& dld) {

		_rayGen = Shader::load(dev, "shaders/raytrace.rgen.spv", "main", vk::ShaderStageFlagBits::eRaygenKHR);
		_rayChit = Shader::load(dev, "shaders/raytrace.rchit.spv", "main", vk::ShaderStageFlagBits::eClosestHitKHR);
		_rayMiss = Shader::load(dev, "shaders/raytrace.rmiss.spv", "main", vk::ShaderStageFlagBits::eMissKHR);

		// Create acceleration structure

		// Acceleration structure descriptor binding
		vk::DescriptorSetLayoutBinding accelerationStructureLayoutBinding;
		accelerationStructureLayoutBinding.binding = 0;
		accelerationStructureLayoutBinding.descriptorType = vk::DescriptorType::eAccelerationStructureKHR;
		accelerationStructureLayoutBinding.descriptorCount = 1;
		accelerationStructureLayoutBinding.stageFlags = vk::ShaderStageFlagBits::eRaygenKHR;

		// Out image binding
		vk::DescriptorSetLayoutBinding storageImageLayoutBinding;
		storageImageLayoutBinding.binding = 1;
		storageImageLayoutBinding.descriptorType = vk::DescriptorType::eStorageImage;
		storageImageLayoutBinding.descriptorCount = 1;
		storageImageLayoutBinding.stageFlags = vk::ShaderStageFlagBits::eRaygenKHR;

		std::array<vk::DescriptorSetLayoutBinding, 2> bindings{ accelerationStructureLayoutBinding, storageImageLayoutBinding };

		vk::DescriptorSetLayoutCreateInfo layoutInfo;
		layoutInfo.setBindings(bindings);
		_descriptorSetLayout = dev.createDescriptorSetLayoutUnique(layoutInfo);

		std::array<vk::DescriptorSetLayout, 1> descriptorLayouts{ _descriptorSetLayout.get() };

		vk::PipelineLayoutCreateInfo pipelineLayoutInfo;
		pipelineLayoutInfo.setSetLayouts(descriptorLayouts);
		_pipelineLayout = dev.createPipelineLayoutUnique(pipelineLayoutInfo);

		//_pass
		//_pass = createPass

		//Pipeline
		_pipelines = _createPipelines(dev, dld);
	}
private:
	vk::UniqueRenderPass _pass;
	std::vector<vk::UniqueHandle<vk::Pipeline, vk::DispatchLoaderDynamic>> _pipelines;

	AccelerationMemory vertex;
	AccelerationMemory index;
	AccelerationMemory instance;

	// Acceleration Structure
	vk::UniqueHandle<vk::AccelerationStructureKHR, vk::DispatchLoaderDynamic> _bottomLevelAS;
	uint64_t _bottomLevelASHandle;
	vk::UniqueHandle<vk::AccelerationStructureKHR, vk::DispatchLoaderDynamic> _topLevelAS;

	vk::DispatchLoaderDynamic _dld;

	// Offscreen buffer
	vma::UniqueImage _offscreenBuffer;
	vk::UniqueImageView _offscreenBufferView;

	uint64_t GetBufferAddress(vk::Device dev, vk::Buffer buffer, vk::DispatchLoaderDynamic& dld)
	{
		vk::BufferDeviceAddressInfoKHR bufferAddressInfo;
		bufferAddressInfo.setBuffer(buffer);

		return dev.getBufferAddressKHR(bufferAddressInfo, dld);
	}

	vk::MemoryRequirements GetAccelerationStructureMemoryRequirements(
		vk::Device dev,
		vk::AccelerationStructureKHR acceleration,
		vk::AccelerationStructureMemoryRequirementsTypeKHR type,
		vk::DispatchLoaderDynamic dynamicDispatcher)
	{
		vk::MemoryRequirements2 memoryRequirements2;

		vk::AccelerationStructureMemoryRequirementsInfoKHR accelerationMemoryRequirements;
		accelerationMemoryRequirements.setPNext(nullptr);
		accelerationMemoryRequirements.setType(type);
		accelerationMemoryRequirements.setBuildType(vk::AccelerationStructureBuildTypeKHR::eDevice);
		accelerationMemoryRequirements.setAccelerationStructure(acceleration);
		memoryRequirements2 = dev.getAccelerationStructureMemoryRequirementsKHR(accelerationMemoryRequirements, dynamicDispatcher);

		return memoryRequirements2.memoryRequirements;
	}


	AccelerationMemory CreateAccelerationScratchBuffer(
		vk::Device dev,
		vk::PhysicalDevice pd,
		vma::Allocator& allocator,
		vk::AccelerationStructureKHR acceleration,
		vk::AccelerationStructureMemoryRequirementsTypeKHR type,
		vk::DispatchLoaderDynamic& dynamicDispatcher)
	{
		AccelerationMemory out = {};

		vk::MemoryRequirements asRequirements =
			GetAccelerationStructureMemoryRequirements(dev, acceleration, type, dynamicDispatcher);

		vk::BufferCreateInfo bufferInfo;
		bufferInfo.setPNext(nullptr);
		bufferInfo.setSize(asRequirements.size);
		bufferInfo.setUsage(vk::BufferUsageFlagBits::eRayTracingKHR | vk::BufferUsageFlagBits::eShaderDeviceAddress);
		bufferInfo.setSharingMode(vk::SharingMode::eExclusive);
		bufferInfo.setQueueFamilyIndexCount(0);
		bufferInfo.setPQueueFamilyIndices(nullptr);

		out.buffer = dev.createBufferUnique(bufferInfo, nullptr);
		vk::MemoryRequirements bufRequirements = dev.getBufferMemoryRequirements(out.buffer.get());

		uint64_t allocationSize =
			asRequirements.size > bufRequirements.size ? asRequirements.size : bufRequirements.size;

		uint32_t allocationMemoryBits = bufRequirements.memoryTypeBits | asRequirements.memoryTypeBits;

		vk::MemoryAllocateFlagsInfo memAllocFlagsInfo = {};
		memAllocFlagsInfo
			.setFlags(vk::MemoryAllocateFlagBits::eDeviceAddress)
			.setDeviceMask(0);

		vk::MemoryAllocateInfo memAllocInfo = {};
		memAllocInfo
			.setAllocationSize(allocationSize)
			.setPNext(&memAllocFlagsInfo)
			.setMemoryTypeIndex(FindMemoryType(pd, allocationMemoryBits, vk::MemoryPropertyFlagBits::eDeviceLocal));

		dev.allocateMemory(&memAllocInfo, nullptr, &(out.memory));
		dev.bindBufferMemory(out.buffer.get(), out.memory, {});

		out.memoryAddress = GetBufferAddress(dev, out.buffer.get(), dynamicDispatcher);

		return out;
	}

	void InsertCommandImageBarrier(vk::CommandBuffer commandBuffer,
		vk::Image image,
		vk::AccessFlags srcAccessMask,
		vk::AccessFlags dstAccessMask,
		vk::ImageLayout oldLayout,
		vk::ImageLayout newLayout,
		const VkImageSubresourceRange& subresourceRange)
	{
		vk::ImageMemoryBarrier imageMmemoryBarrier;
		imageMmemoryBarrier
			.setSrcAccessMask(srcAccessMask)
			.setDstAccessMask(dstAccessMask)
			.setOldLayout(oldLayout)
			.setNewLayout(newLayout)
			.setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
			.setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
			.setImage(image)
			.setSubresourceRange(subresourceRange);
		commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eAllCommands,
			vk::PipelineStageFlagBits::eAllCommands,
			{}, {}, {}, { imageMmemoryBarrier });
	}

	uint32_t FindMemoryType(vk::PhysicalDevice physicalDevice, uint32_t typeFilter, vk::MemoryPropertyFlags properties) {
		vk::PhysicalDeviceMemoryProperties memProperties = physicalDevice.getMemoryProperties();

		for (uint32_t ii = 0; ii < memProperties.memoryTypeCount; ++ii) {
			if ((typeFilter & (1 << ii)) &&
				(memProperties.memoryTypes[ii].propertyFlags & properties) == properties) {
				return ii;
			}
		};
		throw std::runtime_error("failed to find suitable memory type!");
	}

	void BindAccelerationMemory(vk::Device dev, vk::AccelerationStructureKHR acceleration, vk::DeviceMemory memory, vk::DispatchLoaderDynamic dld)
	{
		vk::BindAccelerationStructureMemoryInfoKHR accelerationMemoryBindInfo;
		accelerationMemoryBindInfo
			.setAccelerationStructure(acceleration)
			.setMemory(memory)
			.setMemoryOffset(0)
			.setDeviceIndexCount(0)
			.setPDeviceIndices(nullptr);
		dev.bindAccelerationStructureMemoryKHR(accelerationMemoryBindInfo, dld);
	}

	AccelerationMemory CreateMappedBuffer(vk::Device& dev, vk::PhysicalDevice& pd, void* srcData, uint32_t byteLength, vk::DispatchLoaderDynamic dld) {
		AccelerationMemory out = {};

		vk::BufferCreateInfo bufferInfo;
		bufferInfo.size = byteLength;
		bufferInfo.usage = vk::BufferUsageFlagBits::eShaderDeviceAddress;
		bufferInfo.sharingMode = vk::SharingMode::eExclusive;
		bufferInfo.queueFamilyIndexCount = 0;
		bufferInfo.pQueueFamilyIndices = nullptr;
		out.buffer = dev.createBufferUnique(bufferInfo);

		vk::MemoryRequirements memoryRequirements;
		memoryRequirements = dev.getBufferMemoryRequirements(out.buffer.get());

		vk::MemoryAllocateFlagsInfo memAllocFlagsInfo;

		memAllocFlagsInfo.flags = vk::MemoryAllocateFlagBits::eDeviceAddress;
		memAllocFlagsInfo.deviceMask = 0;

		vk::MemoryAllocateInfo memAllocInfo;
		memAllocInfo.setPNext(&memAllocFlagsInfo);
		memAllocInfo.allocationSize = memoryRequirements.size;
		memAllocInfo.memoryTypeIndex =
			FindMemoryType(pd, memoryRequirements.memoryTypeBits, (vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent));
		out.memory = dev.allocateMemory(memAllocInfo);

		dev.bindBufferMemory(out.buffer.get(), out.memory, {});

		out.memoryAddress = GetBufferAddress(dev, out.buffer.get(), dld);

		void* dstData;
		dstData = dev.mapMemory(out.memory, 0, byteLength, {});
		if (srcData != nullptr) {
			memcpy(dstData, srcData, byteLength);
		}
		dev.unmapMemory(out.memory);
		out.mappedPointer = dstData;

		return out;
	}
};