#pragma once

#include "pass.h"
#include "gBufferPass.h"
#include "shaderIncludes.h"
#include "../aabbTreeBuilder.h"
#include "../shaders/include/gBufferDebugConstants.glsl"

class LightingPass : public Pass {
	friend Pass;
public:
	LightingPass() = default;

	void issueCommands(vk::CommandBuffer commandBuffer, vk::Framebuffer framebuffer) const override {
		std::array<vk::ClearValue, 1> clearValues;
		clearValues[0].color = std::array<float, 4>{ 0.0f, 0.0f, 0.0f, 1.0f };
		vk::RenderPassBeginInfo passBeginInfo;
		passBeginInfo
			.setRenderPass(getPass())
			.setFramebuffer(framebuffer)
			.setRenderArea(vk::Rect2D(vk::Offset2D(0, 0), imageExtent))
			.setClearValues(clearValues);
		commandBuffer.beginRenderPass(passBeginInfo, vk::SubpassContents::eInline);

		/*commandBuffer.pipelineBarrier(
			vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::PipelineStageFlagBits::eFragmentShader,
			vk::DependencyFlagBits::eByRegion, {}, {}, {}
		);*/

		commandBuffer.setViewport(0, { vk::Viewport(
			0.0f, 0.0f, static_cast<float>(imageExtent.width), static_cast<float>(imageExtent.height), 0.0f, 1.0f
		) });
		commandBuffer.setScissor(0, { vk::Rect2D(vk::Offset2D(0, 0), imageExtent) });

		commandBuffer.bindDescriptorSets(
			vk::PipelineBindPoint::eGraphics, _pipelineLayout.get(), 0, descriptorSet, {}
		);
		commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, getPipelines()[0].get());
		commandBuffer.draw(4, 1, 0, 0);

		commandBuffer.endRenderPass();
	}

	[[nodiscard]] vk::DescriptorSetLayout getDescriptorSetLayout() const {
		return _descriptorSetLayout.get();
	}

	void initializeDescriptorSetFor(
		const GBuffer &gBuffer, const SceneBuffers &sceneBuffers,
		vk::Buffer uniformBuffer, vk::Buffer reservoirBuffer, vk::DeviceSize reservoirBufferSize,
		vk::Device device, vk::DescriptorSet set
	) {
		std::vector<vk::WriteDescriptorSet> descriptorWrite;

		std::array<vk::DescriptorImageInfo, 4> imageInfo{
			vk::DescriptorImageInfo(_sampler.get(), gBuffer.getAlbedoView(), vk::ImageLayout::eShaderReadOnlyOptimal),
			vk::DescriptorImageInfo(_sampler.get(), gBuffer.getNormalView(), vk::ImageLayout::eShaderReadOnlyOptimal),
			vk::DescriptorImageInfo(_sampler.get(), gBuffer.getMaterialPropertiesView(), vk::ImageLayout::eShaderReadOnlyOptimal),
			vk::DescriptorImageInfo(_sampler.get(), gBuffer.getWorldPositionView(), vk::ImageLayout::eShaderReadOnlyOptimal),
		};
		for (std::size_t i = 0; i < imageInfo.size(); ++i) {
			descriptorWrite.emplace_back()
				.setDstSet(set)
				.setDstBinding(static_cast<uint32_t>(i))
				.setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
				.setPImageInfo(&imageInfo[i])
				.setDescriptorCount(1);
		}

		vk::DescriptorBufferInfo uniformInfo(uniformBuffer, 0, sizeof(shader::LightingPassUniforms));
		vk::DescriptorBufferInfo reservoirsInfo(reservoirBuffer, 0, reservoirBufferSize);
		vk::DescriptorBufferInfo pointLightsInfo(sceneBuffers.getPtLights(), 0, sceneBuffers.getPtLightsBufferSize());
		vk::DescriptorBufferInfo triLightsInfo(sceneBuffers.getTriLights(), 0, sceneBuffers.getTriLightsBufferSize());
		descriptorWrite.emplace_back()
			.setDstSet(set)
			.setDstBinding(4)
			.setDescriptorType(vk::DescriptorType::eUniformBuffer)
			.setBufferInfo(uniformInfo);
		descriptorWrite.emplace_back()
			.setDstSet(set)
			.setDstBinding(5)
			.setDescriptorType(vk::DescriptorType::eStorageBuffer)
			.setBufferInfo(reservoirsInfo);
		descriptorWrite.emplace_back()
			.setDstSet(set)
			.setDstBinding(6)
			.setDescriptorType(vk::DescriptorType::eStorageBuffer)
			.setBufferInfo(pointLightsInfo);
		descriptorWrite.emplace_back()
			.setDstSet(set)
			.setDstBinding(7)
			.setDescriptorType(vk::DescriptorType::eStorageBuffer)
			.setBufferInfo(triLightsInfo);

		device.updateDescriptorSets(descriptorWrite, {});
	}

	vk::Extent2D imageExtent;
	vk::DescriptorSet descriptorSet;
protected:
	explicit LightingPass(vk::Format format) : Pass(), _swapchainFormat(format) {
	}

	Shader _vert, _frag;
	vk::Format _swapchainFormat;
	vk::UniqueSampler _sampler;
	vk::UniquePipelineLayout _pipelineLayout;
	vk::UniqueDescriptorSetLayout _descriptorSetLayout;

	vk::UniqueRenderPass _createPass(vk::Device device) override {
		std::array<vk::AttachmentDescription, 1> colorAttachments;
		colorAttachments[0]
			.setFormat(_swapchainFormat)
			.setSamples(vk::SampleCountFlagBits::e1)
			.setLoadOp(vk::AttachmentLoadOp::eClear)
			.setStoreOp(vk::AttachmentStoreOp::eStore)
			.setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
			.setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
			.setInitialLayout(vk::ImageLayout::eColorAttachmentOptimal)
			.setFinalLayout(vk::ImageLayout::eColorAttachmentOptimal);

		std::array<vk::AttachmentReference, 1> colorAttachmentReferences{
			vk::AttachmentReference(0, vk::ImageLayout::eColorAttachmentOptimal)
		};

		std::array<vk::SubpassDescription, 1> subpasses;
		subpasses[0]
			.setPipelineBindPoint(vk::PipelineBindPoint::eGraphics)
			.setColorAttachments(colorAttachmentReferences);

		std::array<vk::SubpassDependency, 1> dependencies;
		dependencies[0]
			.setSrcSubpass(VK_SUBPASS_EXTERNAL)
			.setDstSubpass(0)
			.setSrcStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput | vk::PipelineStageFlagBits::eLateFragmentTests)
			.setSrcAccessMask(vk::AccessFlagBits::eColorAttachmentWrite | vk::AccessFlagBits::eDepthStencilAttachmentWrite)
			.setDstStageMask(vk::PipelineStageFlagBits::eFragmentShader)
			.setDstAccessMask(vk::AccessFlagBits::eShaderRead);

		vk::RenderPassCreateInfo renderPassInfo;
		renderPassInfo
			.setAttachments(colorAttachments)
			.setSubpasses(subpasses)
			.setDependencies(dependencies);

		return device.createRenderPassUnique(renderPassInfo);
	}
	std::vector<PipelineCreationInfo> _getPipelineCreationInfo() override {
		std::vector<PipelineCreationInfo> result;

		GraphicsPipelineCreationInfo info;
		info.inputAssemblyState
			.setTopology(vk::PrimitiveTopology::eTriangleStrip);
		info.viewportState
			.setViewportCount(1)
			.setScissorCount(1);
		info.rasterizationState = GraphicsPipelineCreationInfo::getDefaultRasterizationState();
		info.multisampleState = GraphicsPipelineCreationInfo::getNoMultisampleState();
		info.attachmentColorBlendStorage.emplace_back(GraphicsPipelineCreationInfo::getNoBlendAttachment());
		info.colorBlendState.setAttachments(info.attachmentColorBlendStorage);
		info.shaderStages.emplace_back(_frag.getStageInfo());
		info.shaderStages.emplace_back(_vert.getStageInfo());
		info.dynamicStates.emplace_back(vk::DynamicState::eViewport);
		info.dynamicStates.emplace_back(vk::DynamicState::eScissor);
		info.pipelineLayout = _pipelineLayout.get();
		result.emplace_back(std::move(info));

		return result;
	}
	void _initialize(vk::Device dev) override {
		_vert = Shader::load(dev, "shaders/quad.vert.spv", "main", vk::ShaderStageFlagBits::eVertex);
		_frag = Shader::load(dev, "shaders/lighting.frag.spv", "main", vk::ShaderStageFlagBits::eFragment);

		_sampler = createSampler(dev, vk::Filter::eNearest, vk::Filter::eNearest, vk::SamplerMipmapMode::eNearest);

		std::array<vk::DescriptorSetLayoutBinding, 8> descriptorBindings{
			vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment),
			vk::DescriptorSetLayoutBinding(1, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment),
			vk::DescriptorSetLayoutBinding(2, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment),
			vk::DescriptorSetLayoutBinding(3, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment),
			vk::DescriptorSetLayoutBinding(4, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eFragment),
			vk::DescriptorSetLayoutBinding(5, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eFragment),
			vk::DescriptorSetLayoutBinding(6, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eFragment),
			vk::DescriptorSetLayoutBinding(7, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eFragment)
		};
		vk::DescriptorSetLayoutCreateInfo descriptorInfo;
		descriptorInfo
			.setBindings(descriptorBindings);
		_descriptorSetLayout = dev.createDescriptorSetLayoutUnique(descriptorInfo);


		std::array<vk::DescriptorSetLayout, 1> descriptorLayouts{ _descriptorSetLayout.get() };
		vk::PipelineLayoutCreateInfo pipelineInfo;
		pipelineInfo.setSetLayouts(descriptorLayouts);
		_pipelineLayout = dev.createPipelineLayoutUnique(pipelineInfo);

		Pass::_initialize(dev);
	}
};
