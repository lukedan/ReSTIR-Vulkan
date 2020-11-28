#pragma once

#include "pass.h"

#include "../shaderIncludes.h"

class SoftwareVisibilityTestPass : public Pass {
public:
	void issueCommands(vk::CommandBuffer buffer, vk::Framebuffer) const override {
		buffer.bindPipeline(vk::PipelineBindPoint::eCompute, getPipelines()[0].get());
		buffer.pipelineBarrier(
			vk::PipelineStageFlagBits::eComputeShader,
			vk::PipelineStageFlagBits::eComputeShader,
			vk::DependencyFlagBits::eByRegion,
			{}, {}, {}
		);
		buffer.dispatch(
			ceilDiv(_screenSize.width, LIGHT_SAMPLE_GROUP_SIZE_X),
			ceilDiv(_screenSize.height, LIGHT_SAMPLE_GROUP_SIZE_Y),
			1
		);
	}
protected:
	Shader _shader;
	vk::UniqueDescriptorSetLayout _descriptorLayout;
	vk::UniquePipelineLayout _layout;
	vk::Extent2D _screenSize;

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
		_shader = Shader::load(dev, "shaders/softwareVisibilityTest.comp.spv", "main", vk::ShaderStageFlagBits::eCompute);

		std::array<vk::DescriptorSetLayoutBinding, 5> bindings{
			vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eCompute),
			vk::DescriptorSetLayoutBinding(1, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eCompute),
			vk::DescriptorSetLayoutBinding(2, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eCompute),
			vk::DescriptorSetLayoutBinding(3, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eCompute),
			vk::DescriptorSetLayoutBinding(4, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eCompute)
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
