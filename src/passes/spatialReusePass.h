#pragma once

#include "pass.h"

class SpatialReusePass : public Pass {
public:
	[[nodiscard]] vk::DescriptorSetLayout getDescriptorSetLayout() const {
		return _descriptorLayout.get();
	}

	void issueCommands(vk::CommandBuffer buffer, vk::Framebuffer) const override {
		buffer.pipelineBarrier(
			vk::PipelineStageFlagBits::eComputeShader,
			vk::PipelineStageFlagBits::eComputeShader,
			{}, {}, {}, {}
		);
		buffer.bindPipeline(vk::PipelineBindPoint::eCompute, getPipelines()[0].get());
		buffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute, _layout.get(), 0, { descriptorSet }, {});
		buffer.dispatch(
			ceilDiv<uint32_t>(screenSize.width, LIGHT_SAMPLE_GROUP_SIZE_X),
			ceilDiv<uint32_t>(screenSize.height, LIGHT_SAMPLE_GROUP_SIZE_Y),
			1
		);
	}

	void initializeDescriptorSetFor(
		const GBuffer& gbuffer,  vk::Buffer uniformBuffer,
		vk::Buffer reservoirBuffer, vk::DeviceSize reservoirBufferSize,
		vk::Buffer resultReservoirBuffer,
		vk::Buffer posThresholdBuffer, vk::Buffer normalAngleThresholdBuffer,
		vk::Device device, vk::DescriptorSet set
	) {
		std::array<vk::WriteDescriptorSet, 9> writes;

		// currently no world position buffer, so use depth as a placeholder
		vk::DescriptorBufferInfo uniformInfo(uniformBuffer, 0, sizeof(shader::RestirUniforms));
		vk::DescriptorImageInfo worldPosImageInfo(_sampler.get(), gbuffer.getWorldPositionView(), vk::ImageLayout::eShaderReadOnlyOptimal);
		vk::DescriptorImageInfo albedoImageInfo(_sampler.get(), gbuffer.getAlbedoView(), vk::ImageLayout::eShaderReadOnlyOptimal);
		vk::DescriptorImageInfo normalImageInfo(_sampler.get(), gbuffer.getNormalView(), vk::ImageLayout::eShaderReadOnlyOptimal);
		vk::DescriptorImageInfo materialImageInfo(_sampler.get(), gbuffer.getMaterialPropertiesView(), vk::ImageLayout::eShaderReadOnlyOptimal);
		vk::DescriptorBufferInfo reservoirInfo(reservoirBuffer, 0, reservoirBufferSize);
		vk::DescriptorBufferInfo resultReservoirInfo(resultReservoirBuffer, 0, reservoirBufferSize);
		vk::DescriptorBufferInfo posThresholdInfo(posThresholdBuffer, 0, sizeof(float));
		vk::DescriptorBufferInfo normalAngleThresholdInfo(normalAngleThresholdBuffer, 0, sizeof(float));

		writes[0]
			.setDstSet(set)
			.setDstBinding(0)
			.setDescriptorType(vk::DescriptorType::eUniformBuffer)
			.setBufferInfo(uniformInfo);
		writes[1]
			.setDstSet(set)
			.setDstBinding(1)
			.setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
			.setImageInfo(worldPosImageInfo);
		writes[2]
			.setDstSet(set)
			.setDstBinding(2)
			.setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
			.setImageInfo(albedoImageInfo);
		writes[3]
			.setDstSet(set)
			.setDstBinding(3)
			.setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
			.setImageInfo(normalImageInfo);
		writes[4]
			.setDstSet(set)
			.setDstBinding(4)
			.setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
			.setImageInfo(materialImageInfo);
		writes[5]
			.setDstSet(set)
			.setDstBinding(5)
			.setDescriptorType(vk::DescriptorType::eStorageBuffer)
			.setBufferInfo(reservoirInfo);
		writes[6]
			.setDstSet(set)
			.setDstBinding(6)
			.setDescriptorType(vk::DescriptorType::eStorageBuffer)
			.setBufferInfo(resultReservoirInfo);
		writes[7]
			.setDstSet(set)
			.setDstBinding(7)
			.setDescriptorType(vk::DescriptorType::eUniformBuffer)
			.setBufferInfo(posThresholdInfo);
		writes[8]
			.setDstSet(set)
			.setDstBinding(8)
			.setDescriptorType(vk::DescriptorType::eUniformBuffer)
			.setBufferInfo(normalAngleThresholdInfo);


		device.updateDescriptorSets(writes, {});
	}

	vk::DescriptorSet descriptorSet;
	vk::Extent2D screenSize;
protected:
	Shader _shader;
	vk::UniqueDescriptorSetLayout _descriptorLayout;
	vk::UniquePipelineLayout _layout;
	vk::UniqueSampler _sampler;

	vk::UniqueRenderPass _createPass(vk::Device device) override {
		return {};
	}

	std::vector<PipelineCreationInfo> _getPipelineCreationInfo() override {
		std::vector<PipelineCreationInfo> result;

		vk::ComputePipelineCreateInfo pipelineInfo;
		pipelineInfo
			.setStage(_shader.getStageInfo())
			.setLayout(_layout.get());
		result.emplace_back(pipelineInfo);

		return result;
	}

	void _initialize(vk::Device dev) override {
		_shader = Shader::load(dev, "shaders/spatialReuse.comp.spv", "main", vk::ShaderStageFlagBits::eCompute);

		_sampler = createSampler(dev, vk::Filter::eNearest, vk::Filter::eNearest, vk::SamplerMipmapMode::eNearest);

		std::array<vk::DescriptorSetLayoutBinding, 9> bindings{
			vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eCompute),
			vk::DescriptorSetLayoutBinding(1, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eCompute),
			vk::DescriptorSetLayoutBinding(2, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eCompute),
			vk::DescriptorSetLayoutBinding(3, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eCompute),
			vk::DescriptorSetLayoutBinding(4, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eCompute),
			vk::DescriptorSetLayoutBinding(5, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eCompute),
			vk::DescriptorSetLayoutBinding(6, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eCompute),
			vk::DescriptorSetLayoutBinding(7, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eCompute),
			vk::DescriptorSetLayoutBinding(8, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eCompute)
		};

		vk::DescriptorSetLayoutCreateInfo descriptorInfo;
		descriptorInfo.setBindings(bindings);
		_descriptorLayout = dev.createDescriptorSetLayoutUnique(descriptorInfo);

		std::array<vk::DescriptorSetLayout, 1> descriptorLayouts{ _descriptorLayout.get() };

		vk::PipelineLayoutCreateInfo layoutInfo;
		layoutInfo.setSetLayouts(descriptorLayouts);
		_layout = dev.createPipelineLayoutUnique(layoutInfo);

		Pass::_initialize(dev);
	}
};
