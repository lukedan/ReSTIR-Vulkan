#pragma once

#include <vulkan/vulkan.hpp>

#include "pass.h"
#include "vma.h"

class RestirPass : public Pass {
	friend Pass;
public:
	RestirPass() = default;
	RestirPass(RestirPass&&) = default;
	RestirPass& operator=(RestirPass&&) = default;

	[[nodiscard]] vk::DescriptorSetLayout getStaticDescriptorSetLayout() const {
		return _staticDescriptorSetLayout.get();
	}
	[[nodiscard]] vk::DescriptorSetLayout getFrameDescriptorSetLayout() const {
		return _frameDescriptorSetLayout.get();
	}
	[[nodiscard]] vk::DescriptorSetLayout getHardwareRayTraceDescriptorSetLayout() const {
		return _hwRayTraceDescriptorSetLayout.get();
	}
	[[nodiscard]] vk::DescriptorSetLayout getSoftwareRayTraceDescriptorSetLayout() const {
		return _swRayTraceDescriptorSetLayout.get();
	}

	void freeDeviceMemory(vk::Device dev)
	{
		for (int i = 0; i < memories.size(); i++)
		{
			dev.freeMemory(memories.at(i));
		}
	}

	void issueCommands(vk::CommandBuffer commandBuffer, vk::Framebuffer) const override {
		commandBuffer.pipelineBarrier(
			vk::PipelineStageFlagBits::eAllCommands,
			vk::PipelineStageFlagBits::eAllCommands,
			{}, {}, {}, {}
		);

		if (useSoftwareRayTracing) {
			commandBuffer.bindPipeline(vk::PipelineBindPoint::eCompute, getPipelines()[0].get());
			commandBuffer.bindDescriptorSets(
				vk::PipelineBindPoint::eCompute, _swPipelineLayout.get(), 0,
				{ staticDescriptorSet, frameDescriptorSet, raytraceDescriptorSet }, {}
			);
			commandBuffer.dispatch(
				ceilDiv<uint32_t>(bufferExtent.width, OMNI_GROUP_SIZE_X),
				ceilDiv<uint32_t>(bufferExtent.height, OMNI_GROUP_SIZE_Y),
				1
			);
		} else {
			commandBuffer.bindPipeline(vk::PipelineBindPoint::eRayTracingKHR, _hwRayTracePipeline.get());
			commandBuffer.bindDescriptorSets(
				vk::PipelineBindPoint::eRayTracingKHR, _hwPipelineLayout.get(), 0,
				{ staticDescriptorSet, frameDescriptorSet, raytraceDescriptorSet }, {}
			);
			commandBuffer.traceRaysKHR(rayGenSBT, rayMissSBT, rayHitSBT, rayCallSBT, bufferExtent.width, bufferExtent.height, 1, *dynamicLoader);
		}
	}


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


	void initializeStaticDescriptorSetFor(
		const SceneBuffers& scene, vk::Buffer uniformBuffer, vk::Device device, vk::DescriptorSet set
	) {
		std::array<vk::WriteDescriptorSet, 4> writes;

		vk::DescriptorBufferInfo pointLightBuffer(scene.getPtLights(), 0, scene.getPtLightsBufferSize());
		vk::DescriptorBufferInfo triangleLightBuffer(scene.getTriLights(), 0, scene.getTriLightsBufferSize());
		vk::DescriptorBufferInfo aliasTableBufferInfo(scene.getAliasTable(), 0, scene.getAliasTableBufferSize());
		vk::DescriptorBufferInfo uniformBufferInfo(uniformBuffer, 0, sizeof(shader::RestirUniforms));

		writes[0]
			.setDstSet(set)
			.setDstBinding(0)
			.setDescriptorType(vk::DescriptorType::eStorageBuffer)
			.setBufferInfo(pointLightBuffer);
		writes[1]
			.setDstSet(set)
			.setDstBinding(1)
			.setDescriptorType(vk::DescriptorType::eStorageBuffer)
			.setBufferInfo(triangleLightBuffer);
		writes[2]
			.setDstSet(set)
			.setDstBinding(2)
			.setDescriptorType(vk::DescriptorType::eStorageBuffer)
			.setBufferInfo(aliasTableBufferInfo);

		writes[3]
			.setDstSet(set)
			.setDstBinding(3)
			.setDescriptorType(vk::DescriptorType::eUniformBuffer)
			.setBufferInfo(uniformBufferInfo);

		device.updateDescriptorSets(writes, {});
	}

	void initializeFrameDescriptorSetFor(
		const GBuffer& gbuffer, const GBuffer& prevFrameGBuffer,
		vk::Buffer reservoirBuffer, vk::Buffer prevFrameReservoirBuffer, vk::DeviceSize reservoirBufferSize,
		vk::Device device, vk::DescriptorSet set
	) {
		std::vector<vk::WriteDescriptorSet> writes;

		std::vector<vk::DescriptorImageInfo> imageInfo{
			vk::DescriptorImageInfo(_sampler.get(), gbuffer.getWorldPositionView(), vk::ImageLayout::eShaderReadOnlyOptimal),
			vk::DescriptorImageInfo(_sampler.get(), gbuffer.getAlbedoView(), vk::ImageLayout::eShaderReadOnlyOptimal),
			vk::DescriptorImageInfo(_sampler.get(), gbuffer.getNormalView(), vk::ImageLayout::eShaderReadOnlyOptimal),
			vk::DescriptorImageInfo(_sampler.get(), gbuffer.getMaterialPropertiesView(), vk::ImageLayout::eShaderReadOnlyOptimal),

			vk::DescriptorImageInfo(_sampler.get(), prevFrameGBuffer.getWorldPositionView(), vk::ImageLayout::eShaderReadOnlyOptimal),
			vk::DescriptorImageInfo(_sampler.get(), prevFrameGBuffer.getAlbedoView(), vk::ImageLayout::eShaderReadOnlyOptimal),
			vk::DescriptorImageInfo(_sampler.get(), prevFrameGBuffer.getNormalView(), vk::ImageLayout::eShaderReadOnlyOptimal),
			vk::DescriptorImageInfo(_sampler.get(), prevFrameGBuffer.getDepthView(), vk::ImageLayout::eShaderReadOnlyOptimal)
		};

		vk::DescriptorBufferInfo prevReservoirInfo(prevFrameReservoirBuffer, 0, reservoirBufferSize);
		vk::DescriptorBufferInfo reservoirInfo(reservoirBuffer, 0, reservoirBufferSize);


		for (vk::DescriptorImageInfo &info : imageInfo) {
			writes.emplace_back()
				.setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
				.setImageInfo(info);
		}

		writes.emplace_back()
			.setDescriptorType(vk::DescriptorType::eStorageBuffer)
			.setBufferInfo(reservoirInfo);
		writes.emplace_back()
			.setDescriptorType(vk::DescriptorType::eStorageBuffer)
			.setBufferInfo(prevReservoirInfo);


		for (std::size_t i = 0; i < writes.size(); ++i) {
			writes[i]
				.setDstSet(set)
				.setDstBinding(static_cast<uint32_t>(i));
		}
		device.updateDescriptorSets(writes, {});
	}

	void initializeHardwareRayTracingDescriptorSet(const SceneRaytraceBuffers &buffers, vk::Device dev, vk::DescriptorSet set) {
		std::array<vk::WriteDescriptorSet, 1> descriptorWrite;

		vk::AccelerationStructureKHR tlas = buffers.getTopLevelAccelerationStructure();
		vk::WriteDescriptorSetAccelerationStructureKHR descriptorAccelerationStructureInfo;
		descriptorAccelerationStructureInfo
			.setAccelerationStructureCount(1)
			.setAccelerationStructures(tlas);

		descriptorWrite[0]
			.setPNext(&descriptorAccelerationStructureInfo)
			.setDstSet(set)
			.setDstBinding(0)
			.setDstArrayElement(0)
			.setDescriptorType(vk::DescriptorType::eAccelerationStructureKHR)
			.setDescriptorCount(1);

		dev.updateDescriptorSets(descriptorWrite, {}, *dynamicLoader);
	}

	void initializeSoftwareRayTracingDescriptorSet(const AabbTreeBuffers &treeBuffers, vk::Device dev, vk::DescriptorSet set) {
		std::array<vk::WriteDescriptorSet, 2> writes;
		vk::DescriptorBufferInfo nodeInfo(treeBuffers.nodeBuffer.get(), 0, treeBuffers.nodeBufferSize);
		vk::DescriptorBufferInfo triangleInfo(treeBuffers.triangleBuffer.get(), 0, treeBuffers.triangleBufferSize);

		writes[0]
			.setDstSet(set)
			.setDstBinding(0)
			.setDescriptorType(vk::DescriptorType::eStorageBuffer)
			.setBufferInfo(nodeInfo);
		writes[1]
			.setDstSet(set)
			.setDstBinding(1)
			.setDescriptorType(vk::DescriptorType::eStorageBuffer)
			.setBufferInfo(triangleInfo);

		dev.updateDescriptorSets(writes, {});
	}


	void createShaderBindingTable(vk::Device& dev, vma::Allocator& allocator, vk::PhysicalDevice& physicalDev)
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
		vk::Result res = dev.getRayTracingShaderGroupHandlesKHR(
			_hwRayTracePipeline.get(), 0, shaderGroupSize, shaderBindingTableSize, shaderHandleStorage.data(), *dynamicLoader
		);
		vkCheck(res);

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


	//vk::DescriptorSet descriptorSet;
	vma::UniqueBuffer _shaderBindingTable;
	vk::StridedBufferRegionKHR rayGenSBT;
	vk::StridedBufferRegionKHR rayMissSBT;
	vk::StridedBufferRegionKHR rayHitSBT;
	vk::StridedBufferRegionKHR rayCallSBT;

	vk::DescriptorSet staticDescriptorSet;
	vk::DescriptorSet frameDescriptorSet;
	vk::DescriptorSet raytraceDescriptorSet;

	vk::Extent2D bufferExtent;
	const vk::DispatchLoaderDynamic *dynamicLoader = nullptr;
	bool useSoftwareRayTracing = false;
protected:
	explicit RestirPass(const vk::DispatchLoaderDynamic &loader) : Pass(), dynamicLoader(&loader) {
	}

	Shader _rayGen, _rayChit, _rayMiss, _rayShadowMiss, _software;

	vk::UniqueSampler _sampler;
	vk::UniquePipelineLayout _hwPipelineLayout;
	vk::UniquePipelineLayout _swPipelineLayout;
	vk::UniqueDescriptorSetLayout _staticDescriptorSetLayout;
	vk::UniqueDescriptorSetLayout _frameDescriptorSetLayout;
	vk::UniqueDescriptorSetLayout _hwRayTraceDescriptorSetLayout;
	vk::UniqueDescriptorSetLayout _swRayTraceDescriptorSetLayout;
	vk::UniqueHandle<vk::Pipeline, vk::DispatchLoaderDynamic> _hwRayTracePipeline;

	[[nodiscard]] vk::UniqueRenderPass _createPass(vk::Device) override {
		return {};
	}

	[[nodiscard]] std::vector<PipelineCreationInfo> _getPipelineCreationInfo() override {
		return {};
	}

	[[nodiscard]] std::vector<vk::UniquePipeline> _createPipelines(vk::Device dev) override {
		std::vector<vk::UniquePipeline> pipelines;

		{ // compute pipeline
			vk::ComputePipelineCreateInfo pipelineInfo;
			pipelineInfo
				.setStage(_software.getStageInfo())
				.setLayout(_swPipelineLayout.get());
			auto [res, pipeline] = dev.createComputePipelineUnique(nullptr, pipelineInfo);
			vkCheck(res);
			pipelines.emplace_back(std::move(pipeline));
		}

		return pipelines;
	}

	void _initialize(vk::Device dev) override {
		constexpr vk::ShaderStageFlags stageFlags = vk::ShaderStageFlagBits::eRaygenKHR | vk::ShaderStageFlagBits::eCompute;

		_sampler = createSampler(dev, vk::Filter::eNearest, vk::Filter::eNearest, vk::SamplerMipmapMode::eNearest);

		_rayGen = Shader::load(dev, "shaders/restirOmniHardware.rgen.spv", "main", vk::ShaderStageFlagBits::eRaygenKHR);
		_rayChit = Shader::load(dev, "shaders/hwVisibilityTest.rchit.spv", "main", vk::ShaderStageFlagBits::eClosestHitKHR);
		_rayMiss = Shader::load(dev, "shaders/hwVisibilityTest.rmiss.spv", "main", vk::ShaderStageFlagBits::eMissKHR);
		_rayShadowMiss = Shader::load(dev, "shaders/hwVisibilityTestShadow.rmiss.spv", "main", vk::ShaderStageFlagBits::eMissKHR);
		_software = Shader::load(dev, "shaders/restirOmniSoftware.comp.spv", "main", vk::ShaderStageFlagBits::eCompute);


		std::array<vk::DescriptorSetLayoutBinding, 4> staticBindings{
			vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eStorageBuffer, 1, stageFlags),
			vk::DescriptorSetLayoutBinding(1, vk::DescriptorType::eStorageBuffer, 1, stageFlags),
			vk::DescriptorSetLayoutBinding(2, vk::DescriptorType::eStorageBuffer, 1, stageFlags),
			vk::DescriptorSetLayoutBinding(3, vk::DescriptorType::eUniformBuffer, 1, stageFlags)
		};

		vk::DescriptorSetLayoutCreateInfo staticLayoutInfo;
		staticLayoutInfo.setBindings(staticBindings);
		_staticDescriptorSetLayout = dev.createDescriptorSetLayoutUnique(staticLayoutInfo);


		std::array<vk::DescriptorSetLayoutBinding, 10> frameBindings{
			vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eCombinedImageSampler, 1, stageFlags),
			vk::DescriptorSetLayoutBinding(1, vk::DescriptorType::eCombinedImageSampler, 1, stageFlags),
			vk::DescriptorSetLayoutBinding(2, vk::DescriptorType::eCombinedImageSampler, 1, stageFlags),
			vk::DescriptorSetLayoutBinding(3, vk::DescriptorType::eCombinedImageSampler, 1, stageFlags),
			vk::DescriptorSetLayoutBinding(4, vk::DescriptorType::eCombinedImageSampler, 1, stageFlags),
			vk::DescriptorSetLayoutBinding(5, vk::DescriptorType::eCombinedImageSampler, 1, stageFlags),
			vk::DescriptorSetLayoutBinding(6, vk::DescriptorType::eCombinedImageSampler, 1, stageFlags),
			vk::DescriptorSetLayoutBinding(7, vk::DescriptorType::eCombinedImageSampler, 1, stageFlags),
			vk::DescriptorSetLayoutBinding(8, vk::DescriptorType::eStorageBuffer, 1, stageFlags),
			vk::DescriptorSetLayoutBinding(9, vk::DescriptorType::eStorageBuffer, 1, stageFlags)
		};

		vk::DescriptorSetLayoutCreateInfo frameDescriptorInfo;
		frameDescriptorInfo.setBindings(frameBindings);
		_frameDescriptorSetLayout = dev.createDescriptorSetLayoutUnique(frameDescriptorInfo);


		// Acceleration structure descriptor binding
		vk::DescriptorSetLayoutBinding accelerationStructureLayoutBinding;
		accelerationStructureLayoutBinding.binding = 0;
		accelerationStructureLayoutBinding.descriptorType = vk::DescriptorType::eAccelerationStructureKHR;
		accelerationStructureLayoutBinding.descriptorCount = 1;
		accelerationStructureLayoutBinding.stageFlags = vk::ShaderStageFlagBits::eRaygenKHR;

		std::array<vk::DescriptorSetLayoutBinding, 1> hwRayTraceBindings{
			accelerationStructureLayoutBinding
		};

		vk::DescriptorSetLayoutCreateInfo hwRayTraceLayoutInfo;
		hwRayTraceLayoutInfo.setBindings(hwRayTraceBindings);
		_hwRayTraceDescriptorSetLayout = dev.createDescriptorSetLayoutUnique(hwRayTraceLayoutInfo);


		std::array<vk::DescriptorSetLayoutBinding, 2> swRayTraceBindings{
			vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eCompute),
			vk::DescriptorSetLayoutBinding(1, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eCompute)
		};
		vk::DescriptorSetLayoutCreateInfo swRayTraceLayoutInfo;
		swRayTraceLayoutInfo.setBindings(swRayTraceBindings);
		_swRayTraceDescriptorSetLayout = dev.createDescriptorSetLayoutUnique(swRayTraceLayoutInfo);


		std::array<vk::DescriptorSetLayout, 3> hwDescriptorLayouts{
			_staticDescriptorSetLayout.get(), _frameDescriptorSetLayout.get(), _hwRayTraceDescriptorSetLayout.get()
		};

		vk::PipelineLayoutCreateInfo hwPipelineLayoutInfo;
		hwPipelineLayoutInfo.setSetLayouts(hwDescriptorLayouts);
		_hwPipelineLayout = dev.createPipelineLayoutUnique(hwPipelineLayoutInfo);

		std::array<vk::DescriptorSetLayout, 3> swDescriptorLayouts{
			_staticDescriptorSetLayout.get(), _frameDescriptorSetLayout.get(), _swRayTraceDescriptorSetLayout.get()
		};

		vk::PipelineLayoutCreateInfo swPipelineLayoutInfo;
		swPipelineLayoutInfo.setSetLayouts(swDescriptorLayouts);
		_swPipelineLayout = dev.createPipelineLayoutUnique(swPipelineLayoutInfo);


#ifndef RENDERDOC_CAPTURE
		{ // create ray tracing pipeline
			std::vector<vk::RayTracingShaderGroupCreateInfoKHR> shaderGroups;
			shaderGroups.emplace_back(getRtGenShaderGroupCreate());
			shaderGroups.emplace_back(getRtHitShaderGroupCreate());
			shaderGroups.emplace_back(getRtMissShaderGroupCreate());
			shaderGroups.emplace_back(getRtShadowMissShaderGroupCreate());

			std::vector<vk::PipelineShaderStageCreateInfo> shaderStages;
			shaderStages.emplace_back(_rayGen.getStageInfo());
			shaderStages.emplace_back(_rayChit.getStageInfo());
			shaderStages.emplace_back(_rayMiss.getStageInfo());
			shaderStages.emplace_back(_rayShadowMiss.getStageInfo());

			vk::RayTracingPipelineCreateInfoKHR rtPipelineInfo;
			rtPipelineInfo
				.setStages(shaderStages)
				.setGroups(shaderGroups)
				.setMaxRecursionDepth(1)
				.setLibraries({})
				.setLayout(_hwPipelineLayout.get());
			auto [res, pipeline] = dev.createRayTracingPipelineKHRUnique(nullptr, rtPipelineInfo, nullptr, *dynamicLoader);
			vkCheck(res);
			_hwRayTracePipeline = std::move(pipeline);
		}
#endif


		Pass::_initialize(dev);
	}
private:
	std::vector<vk::DeviceMemory> memories;

	uint64_t GetBufferAddress(vk::Device dev, vk::Buffer buffer)
	{
		vk::BufferDeviceAddressInfoKHR bufferAddressInfo;
		bufferAddressInfo.setBuffer(buffer);

		return dev.getBufferAddress(bufferAddressInfo, *dynamicLoader);
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

	void BindAccelerationMemory(vk::Device dev, vk::AccelerationStructureKHR acceleration, vk::DeviceMemory memory)
	{
		vk::BindAccelerationStructureMemoryInfoKHR accelerationMemoryBindInfo;
		accelerationMemoryBindInfo
			.setAccelerationStructure(acceleration)
			.setMemory(memory)
			.setMemoryOffset(0)
			.setDeviceIndexCount(0)
			.setPDeviceIndices(nullptr);
		dev.bindAccelerationStructureMemoryKHR(accelerationMemoryBindInfo, *dynamicLoader);
	}
};
