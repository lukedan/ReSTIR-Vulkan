#pragma once

#include "pass.h"
#include "gBufferPass.h"

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
			0.0f, 0.0f, imageExtent.width, imageExtent.height, 0.0f, 1.0f
		) });
		commandBuffer.setScissor(0, { vk::Rect2D(vk::Offset2D(0, 0), imageExtent) });

		commandBuffer.bindDescriptorSets(
			vk::PipelineBindPoint::eGraphics, _pipelineLayout.get(), 0, { descriptorSet }, {}
		);
		commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, getPipelines()[0].get());
		commandBuffer.draw(4, 1, 0, 0);

		commandBuffer.endRenderPass();
	}

	[[nodiscard]] vk::DescriptorSetLayout getDescriptorSetLayout() const {
		return _descriptorSetLayout.get();
	}

	void initializeDescriptorSetFor(GBuffer &buf, vk::Device device, vk::DescriptorSet set) {
		std::array<vk::DescriptorImageInfo, 3> imageInfo{
			vk::DescriptorImageInfo(_sampler.get(), buf.getAlbedoView(), vk::ImageLayout::eShaderReadOnlyOptimal),
			vk::DescriptorImageInfo(_sampler.get(), buf.getNormalView(), vk::ImageLayout::eShaderReadOnlyOptimal),
			vk::DescriptorImageInfo(_sampler.get(), buf.getDepthView(), vk::ImageLayout::eShaderReadOnlyOptimal)
		};

		std::array<vk::WriteDescriptorSet, 3> descriptorWrite;
		for (std::size_t i = 0; i < 3; ++i) {
			descriptorWrite[i]
				.setDstSet(set)
				.setDstBinding(i)
				.setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
				.setPImageInfo(&imageInfo[i])
				.setDescriptorCount(1);
		}
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
		std::vector<vk::AttachmentDescription> colorAttachments;
		colorAttachments.emplace_back()
			.setFormat(_swapchainFormat)
			.setSamples(vk::SampleCountFlagBits::e1)
			.setLoadOp(vk::AttachmentLoadOp::eClear)
			.setStoreOp(vk::AttachmentStoreOp::eStore)
			.setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
			.setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
			.setInitialLayout(vk::ImageLayout::eUndefined)
			.setFinalLayout(vk::ImageLayout::ePresentSrcKHR);

		std::vector<vk::AttachmentReference> colorAttachmentReferences;
		colorAttachmentReferences.emplace_back()
			.setAttachment(0)
			.setLayout(vk::ImageLayout::eColorAttachmentOptimal);

		std::vector<vk::SubpassDescription> subpasses;
		subpasses.emplace_back()
			.setPipelineBindPoint(vk::PipelineBindPoint::eGraphics)
			.setColorAttachments(colorAttachmentReferences);

		std::vector<vk::SubpassDependency> dependencies;
		dependencies.emplace_back()
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
	std::vector<PipelineCreationInfo> _getPipelineCreationInfo() {
		std::vector<PipelineCreationInfo> result;
		PipelineCreationInfo &info = result.emplace_back();
		info.inputAssemblyState
			.setTopology(vk::PrimitiveTopology::eTriangleStrip);
		info.viewportState
			.setViewportCount(1)
			.setScissorCount(1);
		info.rasterizationState = PipelineCreationInfo::getDefaultRasterizationState();
		info.multisampleState = PipelineCreationInfo::getNoMultisampleState();
		info.attachmentColorBlendStorage.emplace_back(PipelineCreationInfo::getNoBlendAttachment());
		info.colorBlendState.setAttachments(info.attachmentColorBlendStorage);
		info.shaderStages.emplace_back(_frag.getStageInfo());
		info.shaderStages.emplace_back(_vert.getStageInfo());
		info.dynamicStates.emplace_back(vk::DynamicState::eViewport);
		info.dynamicStates.emplace_back(vk::DynamicState::eScissor);
		info.pipelineLayout = _pipelineLayout.get();

		return result;
	}
	void _initialize(vk::Device dev) override {
		_vert = Shader::load(dev, "shaders/quad.vert.spv", "main", vk::ShaderStageFlagBits::eVertex);
		_frag = Shader::load(dev, "shaders/lighting.frag.spv", "main", vk::ShaderStageFlagBits::eFragment);

		_sampler = createSampler(dev, vk::Filter::eNearest, vk::Filter::eNearest, vk::SamplerMipmapMode::eNearest);

		std::array<vk::DescriptorSetLayoutBinding, 3> descriptorBindings{
			vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment),
			vk::DescriptorSetLayoutBinding(1, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment),
			vk::DescriptorSetLayoutBinding(2, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment)
		};
		vk::DescriptorSetLayoutCreateInfo descriptorInfo;
		descriptorInfo
			.setBindings(descriptorBindings);
		_descriptorSetLayout = dev.createDescriptorSetLayoutUnique(descriptorInfo);

		std::array<vk::DescriptorSetLayout, 1> descriptorLayouts{ _descriptorSetLayout.get() };
		vk::PipelineLayoutCreateInfo pipelineInfo;
		pipelineInfo
			.setSetLayouts(descriptorLayouts);
		_pipelineLayout = dev.createPipelineLayoutUnique(pipelineInfo);

		Pass::_initialize(dev);
	}
};
