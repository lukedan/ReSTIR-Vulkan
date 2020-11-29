#pragma once

#include "pass.h"

#include "../shaderIncludes.h"
#include "../shader.h"
#include "../aabbTreeBuilder.h"
#include "gBufferPass.h"

class SoftwareVisibilityTestPass : public Pass {
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
			ceilDiv(screenSize.width, SW_VISIBILITY_TEST_GROUP_SIZE_X),
			ceilDiv(screenSize.height, SW_VISIBILITY_TEST_GROUP_SIZE_Y),
			1
		);
	}

	void initializeDescriptorSetFor(
		const GBuffer &gbuffer, const AabbTreeBuffers &aabbTree, vk::Buffer uniformBuffer,
		vk::Buffer reservoirBuffer, vk::DeviceSize reservoirBufferSize,
		vk::Device device, vk::DescriptorSet set
	) {
		std::array<vk::WriteDescriptorSet, 5> writes;

		// currently no world position buffer, so use depth as a placeholder
		vk::DescriptorImageInfo worldPosImageInfo(_sampler.get(), gbuffer.getWorldPositionView(), vk::ImageLayout::eShaderReadOnlyOptimal);
		vk::DescriptorBufferInfo uniformInfo(uniformBuffer, 0, sizeof(shader::RestirUniforms));
		vk::DescriptorBufferInfo reservoirInfo(reservoirBuffer, 0, reservoirBufferSize);
		vk::DescriptorBufferInfo aabbTreeNodesBuffer(aabbTree.nodeBuffer.get(), 0, aabbTree.nodeBufferSize);
		vk::DescriptorBufferInfo triangleBuffer(aabbTree.triangleBuffer.get(), 0, aabbTree.triangleBufferSize);
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
			.setDescriptorType(vk::DescriptorType::eStorageBuffer)
			.setBufferInfo(reservoirInfo);
		writes[3]
			.setDstSet(set)
			.setDstBinding(3)
			.setDescriptorType(vk::DescriptorType::eStorageBuffer)
			.setBufferInfo(aabbTreeNodesBuffer);
		writes[4]
			.setDstSet(set)
			.setDstBinding(4)
			.setDescriptorType(vk::DescriptorType::eStorageBuffer)
			.setBufferInfo(triangleBuffer);

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
		_shader = Shader::load(dev, "shaders/softwareVisibilityTest.comp.spv", "main", vk::ShaderStageFlagBits::eCompute);

		_sampler = createSampler(dev, vk::Filter::eNearest, vk::Filter::eNearest, vk::SamplerMipmapMode::eNearest);

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
