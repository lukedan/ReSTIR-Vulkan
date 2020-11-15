#pragma once

#include "pass.h"
#include "../misc.h"
#include "../shader.h"

class DemoPass : public Pass {
public:
	DemoPass(vk::Format format) : Pass(), _swapchainFormat(format) {
	}

	void issueCommands(vk::CommandBuffer buf) const override {
		buf.setViewport(0, { vk::Viewport(
			0.0f, 0.0f, imageExtent.width, imageExtent.height, 0.0f, 1.0f
		) });
		buf.setScissor(0, { vk::Rect2D(vk::Offset2D(0, 0), imageExtent) });

		buf.bindPipeline(vk::PipelineBindPoint::eGraphics, getPipelines()[0].get());
		buf.draw(3, 1, 0, 0);
	}

	vk::Extent2D imageExtent;
protected:
	Shader _vert, _frag;
	vk::Format _swapchainFormat;
	vk::UniquePipelineLayout _pipelineLayout;

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
			.setSrcStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput)
			.setSrcAccessMask(vk::AccessFlags())
			.setDstStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput)
			.setDstAccessMask(vk::AccessFlagBits::eColorAttachmentWrite);

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
		info.inputAssemblyState = PipelineCreationInfo::getTriangleListWithoutPrimitiveRestartInputAssembly();
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
		_vert = Shader::load(dev, "shaders/simple.vert.spv", "main", vk::ShaderStageFlagBits::eVertex);
		_frag = Shader::load(dev, "shaders/simple.frag.spv", "main", vk::ShaderStageFlagBits::eFragment);

		vk::PipelineLayoutCreateInfo pipelineInfo;
		_pipelineLayout = dev.createPipelineLayoutUnique(pipelineInfo);

		Pass::_initialize(dev);
	}
};
