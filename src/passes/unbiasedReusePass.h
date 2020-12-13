#pragma once

#include <vulkan/vulkan.hpp>

#include "vma.h"

class UnbiasedReusePass {
public:
	UnbiasedReusePass() = default;
	UnbiasedReusePass(UnbiasedReusePass&&) = default;
	UnbiasedReusePass& operator=(UnbiasedReusePass&&) = default;

	[[nodiscard]] vk::DescriptorSetLayout getFrameDescriptorSetLayout() const {
		return _descriptorSetLayout.get();
	}
	[[nodiscard]] vk::DescriptorSetLayout getRaytraceDescriptorSetLayout() const {
		return _rayTraceDescriptorLayout.get();
	}

	void issueCommands(vk::CommandBuffer commandBuffer, vk::Extent2D extent, vk::DispatchLoaderDynamic dld) {
		commandBuffer.bindPipeline(vk::PipelineBindPoint::eRayTracingKHR, _pipelines[0].get());
		commandBuffer.bindDescriptorSets(
			vk::PipelineBindPoint::eRayTracingKHR, _pipelineLayout.get(), 0,
			{ frameDescriptorSet, raytraceDescriptorSet }, {}
		);

		commandBuffer.pipelineBarrier(
			vk::PipelineStageFlagBits::eRayTracingShaderKHR | vk::PipelineStageFlagBits::eComputeShader,
			vk::PipelineStageFlagBits::eRayTracingShaderKHR | vk::PipelineStageFlagBits::eComputeShader,
			{}, {}, {}, {}
		);
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
				.setDstBinding(i)
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
	vk::StridedBufferRegionKHR rayGenSBT;
	vk::StridedBufferRegionKHR rayMissSBT;
	vk::StridedBufferRegionKHR rayHitSBT;
	vk::StridedBufferRegionKHR rayCallSBT;
	vk::DescriptorSet frameDescriptorSet;
	vk::DescriptorSet raytraceDescriptorSet;
protected:
	Shader _rayGen, _rayChit, _rayMiss, _rayShadowMiss;
	vk::Format _swapchainFormat;
	vk::UniqueSampler _sampler;
	vk::UniquePipelineLayout _pipelineLayout;
	vk::UniqueDescriptorSetLayout _descriptorSetLayout;
	vk::UniqueDescriptorSetLayout _rayTraceDescriptorLayout;

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

		_rayGen = Shader::load(dev, "shaders/unbiasedReuseHardware.rgen.spv", "main", vk::ShaderStageFlagBits::eRaygenKHR);
		_rayChit = Shader::load(dev, "shaders/hwVisibilityTest.rchit.spv", "main", vk::ShaderStageFlagBits::eClosestHitKHR);
		_rayMiss = Shader::load(dev, "shaders/hwVisibilityTest.rmiss.spv", "main", vk::ShaderStageFlagBits::eMissKHR);
		_rayShadowMiss = Shader::load(dev, "shaders/hwVisibilityTestShadow.rmiss.spv", "main", vk::ShaderStageFlagBits::eMissKHR);

		std::array<vk::DescriptorSetLayoutBinding, 8> bindings{
			vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eRaygenKHR),
			vk::DescriptorSetLayoutBinding(1, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eRaygenKHR),
			vk::DescriptorSetLayoutBinding(2, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eRaygenKHR),
			vk::DescriptorSetLayoutBinding(3, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eRaygenKHR),
			vk::DescriptorSetLayoutBinding(4, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eRaygenKHR),
			vk::DescriptorSetLayoutBinding(5, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eRaygenKHR),
			vk::DescriptorSetLayoutBinding(6, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eRaygenKHR),
			vk::DescriptorSetLayoutBinding(7, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eRaygenKHR)
		};

		vk::DescriptorSetLayoutCreateInfo layoutInfo;
		layoutInfo.setBindings(bindings);
		_descriptorSetLayout = dev.createDescriptorSetLayoutUnique(layoutInfo);

		// Acceleration structure descriptor binding
		vk::DescriptorSetLayoutBinding accelerationStructureLayoutBinding;
		accelerationStructureLayoutBinding
			.setBinding(0)
			.setDescriptorType(vk::DescriptorType::eAccelerationStructureKHR)
			.setDescriptorCount(1)
			.setStageFlags(vk::ShaderStageFlagBits::eRaygenKHR);

		vk::DescriptorSetLayoutCreateInfo raytraceLayoutInfo;
		raytraceLayoutInfo.setBindings(accelerationStructureLayoutBinding);
		_rayTraceDescriptorLayout = dev.createDescriptorSetLayoutUnique(raytraceLayoutInfo);

		std::array<vk::DescriptorSetLayout, 2> descriptorLayouts{ _descriptorSetLayout.get(), _rayTraceDescriptorLayout.get() };

		vk::PipelineLayoutCreateInfo pipelineLayoutInfo;
		pipelineLayoutInfo.setSetLayouts(descriptorLayouts);
		_pipelineLayout = dev.createPipelineLayoutUnique(pipelineLayoutInfo);

		//Pipeline
		_pipelines = _createPipelines(dev, dld);
	}
private:
	vk::UniqueRenderPass _pass;
	std::vector<vk::UniqueHandle<vk::Pipeline, vk::DispatchLoaderDynamic>> _pipelines;

	vk::DispatchLoaderDynamic *_dld = nullptr;
};