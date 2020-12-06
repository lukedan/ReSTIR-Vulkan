#pragma once

#define RESERVOIR_SIZE 4

#include <vulkan/vulkan.hpp>
#include "vma.h"

//#define ORIGINAL
#undef MemoryBarrier

class UnbiasedRtPass {
public:
	UnbiasedRtPass() = default;
	UnbiasedRtPass(UnbiasedRtPass&&) = default;
	UnbiasedRtPass& operator=(UnbiasedRtPass&&) = default;
	struct cameraUniforms
	{
		nvmath::mat4 viewInverse;
		nvmath::mat4 projInverse;
		nvmath::vec4 cameraPos;
		nvmath::vec4 tempLightPoint;
		float cameraNear;
		float cameraFar;
		float tanHalfFovY;
		float aspectRatio;
	};

	[[nodiscard]] vk::DescriptorSetLayout getDescriptorSetLayout() const {
		return _descriptorSetLayout.get();
	}

	void freeDeviceMemory(vk::Device dev)
	{
		for (int i = 0; i < memories.size(); i++)
		{
			dev.freeMemory(memories.at(i));
		}
	}

	void issueCommands(vk::CommandBuffer commandBuffer, vk::Extent2D extent, vk::DispatchLoaderDynamic dld) {
		commandBuffer.pipelineBarrier(
			vk::PipelineStageFlagBits::eAllCommands,
			vk::PipelineStageFlagBits::eAllCommands,
			{}, {}, {}, {}
		);

		commandBuffer.bindPipeline(vk::PipelineBindPoint::eRayTracingKHR, _pipelines[0].get());
		commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eRayTracingKHR, _pipelineLayout.get(), 0, { descriptorSet }, {});
		commandBuffer.traceRaysKHR(rayGenSBT, rayMissSBT, rayHitSBT, rayCallSBT, extent.width, extent.height, 1, dld);
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

		[[nodiscard]] inline static vk::RayTracingShaderGroupCreateInfoKHR
			getRtShadowMissShaderGroupCreate()
		{
			vk::RayTracingShaderGroupCreateInfoKHR info;
			info.type = vk::RayTracingShaderGroupTypeKHR::eGeneral;
			info.generalShader = 3;
			info.closestHitShader = VK_SHADER_UNUSED_KHR;
			info.anyHitShader = VK_SHADER_UNUSED_KHR;
			info.intersectionShader = VK_SHADER_UNUSED_KHR;
			info.pShaderGroupCaptureReplayHandle = nullptr;

			return info;
		}

		std::vector<vk::PipelineShaderStageCreateInfo> shaderStages;
		std::vector<vk::RayTracingShaderGroupCreateInfoKHR> shaderGroups;
	};

	void createDescriptorSetForRayTracing(
		vk::Device dev,
		const GBuffer& gbuffer,
		vk::Buffer uniformBuffer,
		vk::Buffer reservoirBuffer, vk::DeviceSize reservoirBufferSize,
		vk::DescriptorSet set,
		vk::DispatchLoaderDynamic& dld
	) {
		std::array<vk::WriteDescriptorSet, 4> descriptorWrite;

		vk::WriteDescriptorSetAccelerationStructureKHR descriptorAccelerationStructureInfo;
		descriptorAccelerationStructureInfo.
			setAccelerationStructureCount(1)
			.setAccelerationStructures(_topLevelAS.get());

		vk::WriteDescriptorSet accelerationStructureWrite;
		accelerationStructureWrite
			.setPNext(&descriptorAccelerationStructureInfo)
			.setDstSet(set)
			.setDstBinding(0)
			.setDstArrayElement(0)
			.setDescriptorType(vk::DescriptorType::eAccelerationStructureKHR)
			.setDescriptorCount(1);
		descriptorWrite[0] = accelerationStructureWrite;


		// GBuffer Data
		std::array<vk::DescriptorImageInfo, 1> imageInfo{
			vk::DescriptorImageInfo(_sampler.get(), gbuffer.getWorldPositionView(), vk::ImageLayout::eShaderReadOnlyOptimal)
		};

		descriptorWrite[1]
			.setDstSet(set)
			.setDstBinding(1)
			.setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
			.setPImageInfo(&imageInfo[0])
			.setDescriptorCount(1);

		std::array<vk::DescriptorBufferInfo, 1> reservoirsBufferInfo
		{
			vk::DescriptorBufferInfo(reservoirBuffer, 0, reservoirBufferSize)
		};

		vk::WriteDescriptorSet reservoirsWrite;
		reservoirsWrite
			.setDstSet(set)
			.setDstBinding(2)
			.setDescriptorType(vk::DescriptorType::eStorageBuffer)
			.setBufferInfo(reservoirsBufferInfo);
		descriptorWrite[2] = reservoirsWrite;

		vk::DescriptorBufferInfo restirUniformInfo(uniformBuffer, 0, sizeof(shader::RestirUniforms));

		descriptorWrite[3]
			.setDstSet(set)
			.setDstBinding(3)
			.setDescriptorType(vk::DescriptorType::eUniformBuffer)
			.setBufferInfo(restirUniformInfo);

		dev.updateDescriptorSets(descriptorWrite, {}, dld);
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

		VmaAllocationCreateInfo allocationInfo{};
		allocationInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;

		_shaderBindingTable = allocator.createBuffer(bufferInfo, allocationInfo);

		VmaAllocationInfo sbtAllocationInfo;
		_shaderBindingTable.getAllocInfo(&sbtAllocationInfo);

		// Set shader info
		uint8_t* dstData = _shaderBindingTable.mapAs<uint8_t>();
		std::vector<uint8_t> shaderHandleStorage(shaderBindingTableSize);
		dev.getRayTracingShaderGroupHandlesKHR(_pipelines[0].get(), 0, shaderGroupSize, shaderBindingTableSize, shaderHandleStorage.data(), dld);

		for (uint32_t g = 0; g < shaderGroupSize; g++)
		{
			memcpy(dstData, shaderHandleStorage.data() + g * rtProperties.shaderGroupHandleSize,
				rtProperties.shaderGroupHandleSize);
			dstData += rtProperties.shaderGroupBaseAlignment;
		}

		_shaderBindingTable.unmap();

		// Set buffer region handle
		rayGenSBT
			.setBuffer(_shaderBindingTable.get())
			.setOffset(0)
			.setStride(rtProperties.shaderGroupHandleSize)
			.setSize(shaderBindingTableSize);

		rayMissSBT
			.setBuffer(_shaderBindingTable.get())
			.setOffset(2 * rtProperties.shaderGroupBaseAlignment)
			.setStride(rtProperties.shaderGroupBaseAlignment)
			.setSize(shaderBindingTableSize);

		rayHitSBT
			.setBuffer(_shaderBindingTable.get())
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
		vk::Queue& queue,
		SceneBuffers& sceneBuffer,
		nvh::GltfScene& gltfScene)
	{
		_allBlas.resize(gltfScene.m_primMeshes.size());
		int index = 0;
		std::vector<vk::DeviceAddress> allBlasHandle;
		for (auto& primMesh : gltfScene.m_primMeshes)
		{
			vk::AccelerationStructureCreateGeometryTypeInfoKHR blasCreateGeoInfo;
			blasCreateGeoInfo.setGeometryType(vk::GeometryTypeKHR::eTriangles);
			blasCreateGeoInfo.setMaxPrimitiveCount(primMesh.indexCount / 3.0f);
			blasCreateGeoInfo.setIndexType(vk::IndexType::eUint32);
			blasCreateGeoInfo.setMaxVertexCount(primMesh.vertexCount);
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
				_allBlas.at(index) = dev.createAccelerationStructureKHRUnique(blasCreateInfo, nullptr, dynamicDispatcher);
			}
			catch (std::system_error e)
			{
				std::cout << "Error in create bottom level AS" << std::endl;
				exit(-1);
			}

			vk::DeviceSize blasScratchSize;

			createAccelerationStructure(
				dev, pd, allocator, dynamicDispatcher,
				_allBlas.at(index).get(),
				vk::AccelerationStructureMemoryRequirementsTypeKHR::eObject,
				&blasScratchSize);

			vma::UniqueBuffer blasScratchBuffer = createScratchBuffer(blasScratchSize, allocator);

			vk::AccelerationStructureDeviceAddressInfoKHR devAddrInfo;
			devAddrInfo.setAccelerationStructure(_allBlas.at(index).get());
			allBlasHandle.push_back(dev.getAccelerationStructureAddressKHR(devAddrInfo, dynamicDispatcher));

			vk::AccelerationStructureGeometryKHR blasAccelerationGeometry;
			blasAccelerationGeometry.setFlags(vk::GeometryFlagBitsKHR::eOpaque);
			blasAccelerationGeometry.setGeometryType(vk::GeometryTypeKHR::eTriangles);
			blasAccelerationGeometry.geometry.triangles.setVertexFormat(vk::Format::eR32G32B32Sfloat);
			blasAccelerationGeometry.geometry.triangles.vertexData.setDeviceAddress(dev.getBufferAddress(sceneBuffer.getVertices()));
			blasAccelerationGeometry.geometry.triangles.setVertexStride(sizeof(Vertex));
			blasAccelerationGeometry.geometry.triangles.setIndexType(vk::IndexType::eUint32);
			blasAccelerationGeometry.geometry.triangles.indexData.setDeviceAddress(dev.getBufferAddress(sceneBuffer.getIndices()));


			std::vector<vk::AccelerationStructureGeometryKHR> blasAccelerationGeometries(
				{ blasAccelerationGeometry });

			const vk::AccelerationStructureGeometryKHR* blasPpGeometries = blasAccelerationGeometries.data();

			vk::AccelerationStructureBuildGeometryInfoKHR blasAccelerationBuildGeometryInfo;
			blasAccelerationBuildGeometryInfo.setType(vk::AccelerationStructureTypeKHR::eBottomLevel);
			blasAccelerationBuildGeometryInfo.setFlags(vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace);
			blasAccelerationBuildGeometryInfo.setUpdate(VK_FALSE);
			blasAccelerationBuildGeometryInfo.setDstAccelerationStructure(_allBlas.at(index).get());
			blasAccelerationBuildGeometryInfo.setGeometryArrayOfPointers(VK_FALSE);
			blasAccelerationBuildGeometryInfo.setGeometryCount(1);
			blasAccelerationBuildGeometryInfo.setPpGeometries(&blasPpGeometries);
			blasAccelerationBuildGeometryInfo.scratchData.setDeviceAddress(dev.getBufferAddress(blasScratchBuffer.get()));

			vk::AccelerationStructureBuildOffsetInfoKHR blasAccelerationBuildOffsetInfo;
			blasAccelerationBuildOffsetInfo.primitiveCount = primMesh.indexCount / 3;
			blasAccelerationBuildOffsetInfo.primitiveOffset = primMesh.firstIndex * sizeof(uint32_t);
			blasAccelerationBuildOffsetInfo.firstVertex = primMesh.vertexOffset;
			blasAccelerationBuildOffsetInfo.transformOffset = 0;

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

			index++;
		}

		// Top level acceleration structure
		std::vector<BlasInstanceForTlas> tlas;
		tlas.reserve(gltfScene.m_nodes.size());
		for (auto& node : gltfScene.m_nodes)
		{
			BlasInstanceForTlas inst;
			inst.transform = node.worldMatrix;
			inst.instanceId = node.primMesh;
			inst.blasId = node.primMesh;
			inst.flags = vk::GeometryInstanceFlagBitsKHR::eTriangleFacingCullDisable;
			inst.hitGroupId = 0;
			tlas.emplace_back(inst);
		}

		vk::AccelerationStructureCreateGeometryTypeInfoKHR tlasAccelerationCreateGeometryInfo;
		tlasAccelerationCreateGeometryInfo.geometryType = vk::GeometryTypeKHR::eInstances;
		tlasAccelerationCreateGeometryInfo.maxPrimitiveCount = (static_cast<uint32_t>(tlas.size()));
		tlasAccelerationCreateGeometryInfo.allowsTransforms = VK_FALSE;

		vk::AccelerationStructureCreateInfoKHR tlasAccelerationInfo;
		tlasAccelerationInfo.flags = vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace;
		tlasAccelerationInfo.type = vk::AccelerationStructureTypeKHR::eTopLevel;
		tlasAccelerationInfo.maxGeometryCount = 1;
		tlasAccelerationInfo.pGeometryInfos = &tlasAccelerationCreateGeometryInfo;

		_topLevelAS = dev.createAccelerationStructureKHRUnique(tlasAccelerationInfo, nullptr, dynamicDispatcher);

		vk::DeviceSize tlasScratchSize;

		createAccelerationStructure(
			dev, pd, allocator, dynamicDispatcher,
			_topLevelAS.get(),
			vk::AccelerationStructureMemoryRequirementsTypeKHR::eObject,
			&tlasScratchSize);

		vma::UniqueBuffer tlasScratchBuffer = createScratchBuffer(tlasScratchSize, allocator);

		std::vector<vk::AccelerationStructureInstanceKHR> geometryInstances;
		//geometryInstances.reserve(tlas.size());

		for (const auto& inst : tlas)
		{
			vk::DeviceAddress blasAddress = allBlasHandle[inst.blasId];

			vk::AccelerationStructureInstanceKHR acInstance;
			nvmath::mat4f transp = nvmath::transpose(inst.transform);
			memcpy(&acInstance.transform, &transp, sizeof(acInstance.transform));
			acInstance.setInstanceCustomIndex(inst.instanceId);
			acInstance.setMask(inst.mask);
			acInstance.setInstanceShaderBindingTableRecordOffset(inst.hitGroupId);
			acInstance.setFlags(inst.flags);
			acInstance.setAccelerationStructureReference(blasAddress);

			geometryInstances.push_back(acInstance);
		}


		instance = createMappedBuffer(
			dev, geometryInstances.data(),
			sizeof(vk::AccelerationStructureInstanceKHR) * geometryInstances.size(),
			allocator, dynamicDispatcher);

		vk::AccelerationStructureGeometryKHR tlasAccelerationGeometry;
		tlasAccelerationGeometry.flags = vk::GeometryFlagBitsKHR::eOpaque;
		tlasAccelerationGeometry.geometryType = vk::GeometryTypeKHR::eInstances;
		tlasAccelerationGeometry.geometry.instances.arrayOfPointers = VK_FALSE;
		tlasAccelerationGeometry.geometry.instances.data.deviceAddress = dev.getBufferAddress(instance.get());

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
		tlasAccelerationBuildGeometryInfo.scratchData.deviceAddress = dev.getBufferAddress(tlasScratchBuffer.get());

		vk::AccelerationStructureBuildOffsetInfoKHR tlasAccelerationBuildOffsetInfo;
		tlasAccelerationBuildOffsetInfo.primitiveCount = tlas.size();
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

		vk::MemoryBarrier barrier;
		barrier
			.setSrcAccessMask(vk::AccessFlagBits::eTransferWrite)
			.setDstAccessMask(vk::AccessFlagBits::eAccelerationStructureReadKHR);

		tlasCommandBuffers.at(0).pipelineBarrier(vk::PipelineStageFlagBits::eTransfer,
			vk::PipelineStageFlagBits::eAccelerationStructureBuildKHR, {}, barrier, {}, {}, dynamicDispatcher);

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

		// Create camera uniform buffer
		cameraUniformBuffer = allocator.createTypedBuffer<shader::LightingPassUniforms>(1, vk::BufferUsageFlagBits::eUniformBuffer, VMA_MEMORY_USAGE_CPU_TO_GPU);
	}

	void updateCameraUniform(Camera& _camera)
	{
		auto* cameraUniformBegin = cameraUniformBuffer.mapAs<shader::LightingPassUniforms>();
		cameraUniformBegin->cameraPos = _camera.position;
		cameraUniformBuffer.unmap();
		cameraUniformBuffer.flush();
	}

	inline static UnbiasedRtPass create(vk::Device dev, vk::DispatchLoaderDynamic& dld)
	{
		UnbiasedRtPass pass = UnbiasedRtPass();
		pass._initialize(dev, dld);
		return std::move(pass);
	}

	void setDispatchLoaderDynamic(vk::DispatchLoaderDynamic& dld)
	{
		_dld = dld;
	}

	void destroyAllocations(vma::Allocator& allocator)
	{
		for (int i = 0; i < asAllocation.size(); i++)
		{
			allocator.freeMemory(asAllocation.at(i));
		}
	}

	//vk::DescriptorSet descriptorSet;
	vma::UniqueBuffer _shaderBindingTable;
	vk::StridedBufferRegionKHR rayGenSBT;
	vk::StridedBufferRegionKHR rayMissSBT;
	vk::StridedBufferRegionKHR rayHitSBT;
	vk::StridedBufferRegionKHR rayCallSBT;
	vk::DescriptorSet descriptorSet;
protected:
	Shader _rayGen, _rayChit, _rayMiss, _rayShadowMiss;
	vk::Format _swapchainFormat;
	vk::UniqueSampler _sampler;
	vk::UniquePipelineLayout _pipelineLayout;
	vk::UniqueDescriptorSetLayout _descriptorSetLayout;

	[[nodiscard]] vk::UniqueRenderPass _createPass(vk::Device);

	[[nodiscard]] std::vector<PipelineCreationInfo> _getPipelineCreationInfo()
	{
		std::vector<PipelineCreationInfo> result;
		PipelineCreationInfo& info = result.emplace_back();
		info.shaderGroups.emplace_back(PipelineCreationInfo::getRtGenShaderGroupCreate());
		info.shaderGroups.emplace_back(PipelineCreationInfo::getRtHitShaderGroupCreate());
		info.shaderGroups.emplace_back(PipelineCreationInfo::getRtMissShaderGroupCreate());
		info.shaderGroups.emplace_back(PipelineCreationInfo::getRtShadowMissShaderGroupCreate());
		info.shaderStages.emplace_back(_rayGen.getStageInfo());
		info.shaderStages.emplace_back(_rayChit.getStageInfo());
		info.shaderStages.emplace_back(_rayMiss.getStageInfo());
		info.shaderStages.emplace_back(_rayShadowMiss.getStageInfo());

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
		_sampler = createSampler(dev, vk::Filter::eNearest, vk::Filter::eNearest, vk::SamplerMipmapMode::eNearest);

		_rayGen = Shader::load(dev, "shaders/unbiasedRaytrace.rgen.spv", "main", vk::ShaderStageFlagBits::eRaygenKHR);
		_rayChit = Shader::load(dev, "shaders/unbiasedRaytrace.rchit.spv", "main", vk::ShaderStageFlagBits::eClosestHitKHR);
		_rayMiss = Shader::load(dev, "shaders/unbiasedRaytrace.rmiss.spv", "main", vk::ShaderStageFlagBits::eMissKHR);
		_rayShadowMiss = Shader::load(dev, "shaders/unbiasedRaytraceShadow.rmiss.spv", "main", vk::ShaderStageFlagBits::eMissKHR);

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



		std::array<vk::DescriptorSetLayoutBinding, 4> bindings{
			accelerationStructureLayoutBinding,
			vk::DescriptorSetLayoutBinding(1, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eRaygenKHR),
			vk::DescriptorSetLayoutBinding(2, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eRaygenKHR),
			vk::DescriptorSetLayoutBinding(3, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eRaygenKHR)
		};

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

	vma::UniqueBuffer instance;
	vma::UniqueBuffer cameraUniformBuffer;
	std::vector<vk::DeviceMemory> memories;

	// Acceleration Structure
	vk::UniqueHandle<vk::AccelerationStructureKHR, vk::DispatchLoaderDynamic> _bottomLevelAS;
	std::vector<vk::UniqueHandle<vk::AccelerationStructureKHR, vk::DispatchLoaderDynamic>> _allBlas;
	uint64_t _bottomLevelASHandle;
	vk::UniqueHandle<vk::AccelerationStructureKHR, vk::DispatchLoaderDynamic> _topLevelAS;

	vk::DispatchLoaderDynamic _dld;

	std::vector<VmaAllocation> asAllocation;

	// Offscreen buffer
	vma::UniqueImage _offscreenBuffer;
	vk::UniqueImageView _offscreenBufferView;

	uint64_t GetBufferAddress(vk::Device dev, vk::Buffer buffer, vk::DispatchLoaderDynamic& dld)
	{
		vk::BufferDeviceAddressInfoKHR bufferAddressInfo;
		bufferAddressInfo.setBuffer(buffer);

		return dev.getBufferAddress(bufferAddressInfo, dld);
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

	void createAccelerationStructure(
		vk::Device dev,
		vk::PhysicalDevice pd,
		vma::Allocator& allocator,
		vk::DispatchLoaderDynamic dld,
		vk::AccelerationStructureKHR ac,
		vk::AccelerationStructureMemoryRequirementsTypeKHR memoryType,
		vk::DeviceSize* size
	) {
		vk::MemoryRequirements asRequirements = GetAccelerationStructureMemoryRequirements(
			dev, ac, memoryType, dld
		);

		*size = asRequirements.size;

		vk::BufferCreateInfo asBufferInfo;
		asBufferInfo
			.setSize(asRequirements.size)
			.setUsage(vk::BufferUsageFlagBits::eRayTracingKHR | vk::BufferUsageFlagBits::eShaderDeviceAddress)
			.setSharingMode(vk::SharingMode::eExclusive);

		vk::UniqueBuffer tmpbuffer = dev.createBufferUnique(asBufferInfo, nullptr);
		vk::MemoryRequirements bufRequirements = dev.getBufferMemoryRequirements(tmpbuffer.get());
		uint32_t allocationMemoryBits = bufRequirements.memoryTypeBits | asRequirements.memoryTypeBits;

		VmaAllocationCreateInfo allocationInfo{};
		allocationInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;

		VmaAllocation allocation = nullptr;
		VmaAllocationInfo allocationDetail;

		vkCheck(allocator.allocateMemory(asRequirements, allocation, allocationDetail, allocationInfo));

		vk::BindAccelerationStructureMemoryInfoKHR accelerationMemoryBindInfo;
		accelerationMemoryBindInfo
			.setAccelerationStructure(ac)
			.setMemory(static_cast<vk::DeviceMemory>(allocationDetail.deviceMemory))
			.setMemoryOffset(allocationDetail.offset);

		dev.bindAccelerationStructureMemoryKHR(accelerationMemoryBindInfo, dld);

		asAllocation.push_back(allocation);
	}


	vma::UniqueBuffer createScratchBuffer(vk::DeviceSize size, vma::Allocator& allocator)
	{
		VK_BUFFER_USAGE_RAY_TRACING_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
		vk::BufferCreateInfo bufferInfo;
		bufferInfo
			.setSize(size)
			.setUsage(vk::BufferUsageFlagBits::eRayTracingKHR |
				vk::BufferUsageFlagBits::eShaderDeviceAddress |
				vk::BufferUsageFlagBits::eTransferDst);
		VmaAllocationCreateInfo allocInfo{};
		allocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;

		vma::UniqueBuffer scratchBuffer = allocator.createBuffer(bufferInfo, allocInfo);

		return scratchBuffer;
	}

	vma::UniqueBuffer createMappedBuffer(
		vk::Device& dev,
		void* srcData,
		uint32_t byteLength,
		vma::Allocator& allocator,
		vk::DispatchLoaderDynamic dld
	)
	{
		vk::BufferCreateInfo bufferInfo;
		bufferInfo
			.setSize(byteLength)
			.setUsage(vk::BufferUsageFlagBits::eShaderDeviceAddress)
			.setSharingMode(vk::SharingMode::eExclusive);

		VmaAllocationCreateInfo allocationInfo{};
		allocationInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;

		vma::UniqueBuffer mappedBuffer = allocator.createBuffer(bufferInfo, allocationInfo);
		void* dstData = mappedBuffer.map();
		if (srcData != nullptr) {
			memcpy(dstData, srcData, byteLength);
		}
		mappedBuffer.unmap();

		return mappedBuffer;
	}
};