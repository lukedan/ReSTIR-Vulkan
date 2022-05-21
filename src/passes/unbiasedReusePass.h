#pragma once

#include <vulkan/vulkan.hpp>

#include "vma.h"

class UnbiasedReusePass {
public:
	UnbiasedReusePass() = default;
	UnbiasedReusePass(UnbiasedReusePass&&) = default;
	UnbiasedReusePass& operator=(UnbiasedReusePass&&) = default;

	[[nodiscard]] vk::DescriptorSetLayout getFrameDescriptorSetLayout() const {
		return _frameDescriptorSetLayout.get();
	}
	[[nodiscard]] vk::DescriptorSetLayout getHardwareRaytraceDescriptorSetLayout() const {
		return _hwRaytraceDescriptorLayout.get();
	}
	[[nodiscard]] vk::DescriptorSetLayout getSoftwareRaytraceDescriptorSetLayout() const {
		return _swRaytraceDescriptorLayout.get();
	}

	void issueCommands(vk::CommandBuffer commandBuffer, vk::DispatchLoaderDynamic dld) {
		commandBuffer.pipelineBarrier(
			vk::PipelineStageFlagBits::eRayTracingShaderKHR | vk::PipelineStageFlagBits::eComputeShader,
			vk::PipelineStageFlagBits::eRayTracingShaderKHR | vk::PipelineStageFlagBits::eComputeShader,
			{}, {}, {}, {}
		);

		if (useSoftwareRayTracing) {
			commandBuffer.bindPipeline(vk::PipelineBindPoint::eCompute, _softwarePipeline.get());
			commandBuffer.bindDescriptorSets(
				vk::PipelineBindPoint::eCompute, _swPipelineLayout.get(), 0,
				{ frameDescriptorSet, raytraceDescriptorSet }, {}
			);
			commandBuffer.dispatch(
				ceilDiv<uint32_t>(bufferExtent.width, UNBIASED_REUSE_GROUP_SIZE_X),
				ceilDiv<uint32_t>(bufferExtent.height, UNBIASED_REUSE_GROUP_SIZE_Y),
				1
			);
		} else {
			commandBuffer.bindPipeline(vk::PipelineBindPoint::eRayTracingKHR, _hwRaytracePipeline.get());
			commandBuffer.bindDescriptorSets(
				vk::PipelineBindPoint::eRayTracingKHR, _hwPipelineLayout.get(), 0,
				{ frameDescriptorSet, raytraceDescriptorSet }, {}
			);
			commandBuffer.traceRaysKHR(rayGenSBT, rayMissSBT, rayHitSBT, rayCallSBT, bufferExtent.width, bufferExtent.height, 1, dld);
		}
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

	void initializeFrameDescriptorSet(
		vk::Device dev,
		const GBuffer& gbuffer, vk::Buffer uniformBuffer,
		vk::Buffer reservoirBuffer, vk::Buffer resultReservoirBuffer, vk::DeviceSize reservoirBufferSize,
		vk::DescriptorSet set
	) {
		std::array<vk::WriteDescriptorSet, 8> descriptorWrite;

		// GBuffer Data
		std::array<vk::DescriptorImageInfo, 5> imageInfo{
			vk::DescriptorImageInfo(_sampler.get(), gbuffer.getWorldPositionView(), vk::ImageLayout::eShaderReadOnlyOptimal),
			vk::DescriptorImageInfo(_sampler.get(), gbuffer.getAlbedoView(), vk::ImageLayout::eShaderReadOnlyOptimal),
			vk::DescriptorImageInfo(_sampler.get(), gbuffer.getNormalView(), vk::ImageLayout::eShaderReadOnlyOptimal),
			vk::DescriptorImageInfo(_sampler.get(), gbuffer.getMaterialPropertiesView(), vk::ImageLayout::eShaderReadOnlyOptimal),
			vk::DescriptorImageInfo(_sampler.get(), gbuffer.getDepthView(), vk::ImageLayout::eShaderReadOnlyOptimal)
		};
		for (std::size_t i = 0; i < imageInfo.size(); ++i) {
			descriptorWrite[i]
				.setDstSet(set)
				.setDstBinding(static_cast<uint32_t>(i))
				.setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
				.setImageInfo(imageInfo[i]);
		}

		std::array<vk::DescriptorBufferInfo, 3> reservoirsBufferInfo{
			vk::DescriptorBufferInfo(reservoirBuffer, 0, reservoirBufferSize),
			vk::DescriptorBufferInfo(resultReservoirBuffer, 0, reservoirBufferSize),
			vk::DescriptorBufferInfo(uniformBuffer, 0, sizeof(shader::RestirUniforms))
		};

		descriptorWrite[5]
			.setDstSet(set)
			.setDstBinding(5)
			.setDescriptorType(vk::DescriptorType::eStorageBuffer)
			.setBufferInfo(reservoirsBufferInfo[0]);
		descriptorWrite[6]
			.setDstSet(set)
			.setDstBinding(6)
			.setDescriptorType(vk::DescriptorType::eStorageBuffer)
			.setBufferInfo(reservoirsBufferInfo[1]);
		descriptorWrite[7]
			.setDstSet(set)
			.setDstBinding(7)
			.setDescriptorType(vk::DescriptorType::eUniformBuffer)
			.setBufferInfo(reservoirsBufferInfo[2]);

		dev.updateDescriptorSets(descriptorWrite, {});
	}

	void initializeHardwareRaytraceDescriptorSet(vk::Device dev, const SceneRaytraceBuffers &rtBuffer, vk::DescriptorSet set) {
		vk::AccelerationStructureKHR tlas = rtBuffer.getTopLevelAccelerationStructure();
		vk::WriteDescriptorSetAccelerationStructureKHR descriptorAccelerationStructureInfo;
		descriptorAccelerationStructureInfo
			.setAccelerationStructureCount(1)
			.setAccelerationStructures(tlas);

		vk::WriteDescriptorSet accelerationStructureWrite;
		accelerationStructureWrite
			.setPNext(&descriptorAccelerationStructureInfo)
			.setDstSet(set)
			.setDstBinding(0)
			.setDstArrayElement(0)
			.setDescriptorType(vk::DescriptorType::eAccelerationStructureKHR)
			.setDescriptorCount(1);

		dev.updateDescriptorSets(accelerationStructureWrite, {}, *_dld);
	}

	void initializeSoftwareRaytraceDescriptorSet(vk::Device dev, const AabbTreeBuffers &aabbTree, vk::DescriptorSet set) {
		std::array<vk::WriteDescriptorSet, 2> writes;
		vk::DescriptorBufferInfo nodeInfo(aabbTree.nodeBuffer.get(), 0, aabbTree.nodeBufferSize);
		vk::DescriptorBufferInfo triangleInfo(aabbTree.triangleBuffer.get(), 0, aabbTree.triangleBufferSize);

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

	void createShaderBindingTable(vk::Device& dev, vma::Allocator& allocator, vk::PhysicalDevice& physicalDev, vk::DispatchLoaderDynamic dld)
	{
		vk::PhysicalDeviceRayTracingPipelinePropertiesKHR rtProperties;
		vk::PhysicalDeviceProperties2 devProperties2;
		devProperties2.pNext = &rtProperties;

		uint32_t shaderGroupSize = 3;

		physicalDev.getProperties2(&devProperties2);
		uint32_t shaderBindingTableSize = shaderGroupSize * rtProperties.shaderGroupBaseAlignment;

		vk::BufferCreateInfo bufferInfo;
		bufferInfo
			.setSize(shaderBindingTableSize)
			.setUsage(vk::BufferUsageFlagBits::eTransferSrc | vk::BufferUsageFlagBits::eShaderDeviceAddress)
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
			_hwRaytracePipeline.get(), 0, shaderGroupSize, shaderBindingTableSize, shaderHandleStorage.data(), dld
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
		auto sbtAddr = dev.getBufferAddress(_shaderBindingTable.get());

		rayGenSBT
			.setDeviceAddress(sbtAddr)
			.setStride(rtProperties.shaderGroupBaseAlignment)
			.setSize(rtProperties.shaderGroupBaseAlignment);

		rayMissSBT
			.setDeviceAddress(sbtAddr + 2 * rtProperties.shaderGroupBaseAlignment)
			.setStride(rtProperties.shaderGroupBaseAlignment)
			.setSize(rtProperties.shaderGroupBaseAlignment);

		rayHitSBT
			.setDeviceAddress(sbtAddr + rtProperties.shaderGroupBaseAlignment)
			.setStride(rtProperties.shaderGroupBaseAlignment)
			.setSize(rtProperties.shaderGroupBaseAlignment);

		rayCallSBT
			.setDeviceAddress(sbtAddr)
			.setStride(0)
			.setSize(0);
	}

	inline static UnbiasedReusePass create(vk::Device dev, vk::DispatchLoaderDynamic& dld)
	{
		UnbiasedReusePass pass = UnbiasedReusePass();
		pass._initialize(dev, dld);
		return std::move(pass);
	}

	void setDispatchLoaderDynamic(vk::DispatchLoaderDynamic &dld)
	{
		_dld = &dld;
	}

	//vk::DescriptorSet descriptorSet;
	vma::UniqueBuffer _shaderBindingTable;
	vk::StridedDeviceAddressRegionKHR rayGenSBT;
	vk::StridedDeviceAddressRegionKHR rayMissSBT;
	vk::StridedDeviceAddressRegionKHR rayHitSBT;
	vk::StridedDeviceAddressRegionKHR rayCallSBT;
	vk::DescriptorSet frameDescriptorSet;
	vk::DescriptorSet raytraceDescriptorSet;
	vk::Extent2D bufferExtent;
	bool useSoftwareRayTracing = false;
protected:
	Shader _rayGen, _rayChit, _rayMiss, _rayShadowMiss, _software;
	vk::Format _swapchainFormat;
	vk::UniqueSampler _sampler;
	vk::UniquePipelineLayout _hwPipelineLayout;
	vk::UniquePipelineLayout _swPipelineLayout;
	vk::UniqueDescriptorSetLayout _frameDescriptorSetLayout;
	vk::UniqueDescriptorSetLayout _hwRaytraceDescriptorLayout;
	vk::UniqueDescriptorSetLayout _swRaytraceDescriptorLayout;

	[[nodiscard]] vk::UniqueHandle<vk::Pipeline, vk::DispatchLoaderDynamic> _createHardwareRaytracePipeline(vk::Device dev, vk::DispatchLoaderDynamic& dld) {
		// Set ray tracing pipeline
		PipelineCreationInfo info;
		info.shaderGroups.emplace_back(PipelineCreationInfo::getRtGenShaderGroupCreate());
		info.shaderGroups.emplace_back(PipelineCreationInfo::getRtHitShaderGroupCreate());
		info.shaderGroups.emplace_back(PipelineCreationInfo::getRtMissShaderGroupCreate());
		info.shaderGroups.emplace_back(PipelineCreationInfo::getRtShadowMissShaderGroupCreate());
		info.shaderStages.emplace_back(_rayGen.getStageInfo());
		info.shaderStages.emplace_back(_rayChit.getStageInfo());
		info.shaderStages.emplace_back(_rayMiss.getStageInfo());
		info.shaderStages.emplace_back(_rayShadowMiss.getStageInfo());

		vk::RayTracingPipelineCreateInfoKHR rtPipelineInfo;
		rtPipelineInfo
			.setStages(info.shaderStages)
			.setGroups(info.shaderGroups)
			.setMaxPipelineRayRecursionDepth(1)
			.setLayout(_hwPipelineLayout.get());

		auto [res, pipeline] = dev.createRayTracingPipelineKHRUnique(nullptr, nullptr, rtPipelineInfo, nullptr, dld).asTuple();
		vkCheck(res);
		return std::move(pipeline);
	}

	void _initialize(vk::Device dev, vk::DispatchLoaderDynamic& dld) {
		_sampler = createSampler(dev, vk::Filter::eNearest, vk::Filter::eNearest, vk::SamplerMipmapMode::eNearest);

		_rayGen = Shader::load(dev, "shaders/unbiasedReuseHardware.rgen.spv", "main", vk::ShaderStageFlagBits::eRaygenKHR);
		_rayChit = Shader::load(dev, "shaders/hwVisibilityTest.rchit.spv", "main", vk::ShaderStageFlagBits::eClosestHitKHR);
		_rayMiss = Shader::load(dev, "shaders/hwVisibilityTest.rmiss.spv", "main", vk::ShaderStageFlagBits::eMissKHR);
		_rayShadowMiss = Shader::load(dev, "shaders/hwVisibilityTestShadow.rmiss.spv", "main", vk::ShaderStageFlagBits::eMissKHR);

		_software = Shader::load(dev, "shaders/unbiasedReuseSoftware.comp.spv", "main", vk::ShaderStageFlagBits::eCompute);

		vk::ShaderStageFlags stageFlags = vk::ShaderStageFlagBits::eRaygenKHR | vk::ShaderStageFlagBits::eCompute;

		std::array<vk::DescriptorSetLayoutBinding, 8> frameBindings{
			vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eCombinedImageSampler, 1, stageFlags),
			vk::DescriptorSetLayoutBinding(1, vk::DescriptorType::eCombinedImageSampler, 1, stageFlags),
			vk::DescriptorSetLayoutBinding(2, vk::DescriptorType::eCombinedImageSampler, 1, stageFlags),
			vk::DescriptorSetLayoutBinding(3, vk::DescriptorType::eCombinedImageSampler, 1, stageFlags),
			vk::DescriptorSetLayoutBinding(4, vk::DescriptorType::eCombinedImageSampler, 1, stageFlags),
			vk::DescriptorSetLayoutBinding(5, vk::DescriptorType::eStorageBuffer, 1, stageFlags),
			vk::DescriptorSetLayoutBinding(6, vk::DescriptorType::eStorageBuffer, 1, stageFlags),
			vk::DescriptorSetLayoutBinding(7, vk::DescriptorType::eUniformBuffer, 1, stageFlags)
		};

		vk::DescriptorSetLayoutCreateInfo layoutInfo;
		layoutInfo.setBindings(frameBindings);
		_frameDescriptorSetLayout = dev.createDescriptorSetLayoutUnique(layoutInfo);


		// Acceleration structure descriptor binding
		vk::DescriptorSetLayoutBinding accelerationStructureLayoutBinding;
		accelerationStructureLayoutBinding
			.setBinding(0)
			.setDescriptorType(vk::DescriptorType::eAccelerationStructureKHR)
			.setDescriptorCount(1)
			.setStageFlags(vk::ShaderStageFlagBits::eRaygenKHR);

		vk::DescriptorSetLayoutCreateInfo raytraceLayoutInfo;
		raytraceLayoutInfo.setBindings(accelerationStructureLayoutBinding);
		_hwRaytraceDescriptorLayout = dev.createDescriptorSetLayoutUnique(raytraceLayoutInfo);


		std::array<vk::DescriptorSetLayoutBinding, 2> swRaytraceBindings{
			vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eCompute),
			vk::DescriptorSetLayoutBinding(1, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eCompute)
		};

		vk::DescriptorSetLayoutCreateInfo swRaytraceLayoutInfo;
		swRaytraceLayoutInfo.setBindings(swRaytraceBindings);
		_swRaytraceDescriptorLayout = dev.createDescriptorSetLayoutUnique(swRaytraceLayoutInfo);


		std::array<vk::DescriptorSetLayout, 2> hwDescriptorLayouts{ _frameDescriptorSetLayout.get(), _hwRaytraceDescriptorLayout.get() };

		vk::PipelineLayoutCreateInfo hwPipelineLayoutInfo;
		hwPipelineLayoutInfo.setSetLayouts(hwDescriptorLayouts);
		_hwPipelineLayout = dev.createPipelineLayoutUnique(hwPipelineLayoutInfo);


		std::array<vk::DescriptorSetLayout, 2> swDescriptorLayouts{ _frameDescriptorSetLayout.get(), _swRaytraceDescriptorLayout.get() };

		vk::PipelineLayoutCreateInfo swPipelineLayoutInfo;
		swPipelineLayoutInfo.setSetLayouts(swDescriptorLayouts);
		_swPipelineLayout = dev.createPipelineLayoutUnique(swPipelineLayoutInfo);


		//Pipeline
#ifndef RENDERDOC_CAPTURE
		_hwRaytracePipeline = _createHardwareRaytracePipeline(dev, dld);
#endif

		vk::ComputePipelineCreateInfo swPipelineInfo;
		swPipelineInfo
			.setLayout(_swPipelineLayout.get())
			.setStage(_software.getStageInfo());
		auto [res, pipeline] = dev.createComputePipelineUnique(nullptr, swPipelineInfo);
		vkCheck(res);
		_softwarePipeline = std::move(pipeline);
	}
private:
	vk::UniqueRenderPass _pass;
	vk::UniqueHandle<vk::Pipeline, vk::DispatchLoaderDynamic> _hwRaytracePipeline;
	vk::UniquePipeline _softwarePipeline;

	vk::DispatchLoaderDynamic *_dld = nullptr;
};