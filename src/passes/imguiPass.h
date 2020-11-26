#pragma once

#include "pass.h"

#include <imgui.h>
#include <imgui_impl_vulkan.h>

class ImGuiPass : public Pass {
	friend Pass;
public:
	ImGuiPass() = default;

	void issueCommands(vk::CommandBuffer commandBuffer, vk::Framebuffer framebuffer) const override {
		vk::RenderPassBeginInfo passBeginInfo;
		passBeginInfo
			.setRenderPass(getPass())
			.setFramebuffer(framebuffer)
			.setRenderArea(vk::Rect2D(vk::Offset2D(0, 0), imageExtent));
		commandBuffer.beginRenderPass(passBeginInfo, vk::SubpassContents::eInline);

		ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer);

		commandBuffer.endRenderPass();
	}

	vk::Extent2D imageExtent;
protected:
	explicit ImGuiPass(vk::Format fmt, vk::ImageLayout finalLayout = vk::ImageLayout::ePresentSrcKHR) :
		_swapchainFormat(fmt), _finalLayout(finalLayout) {
	}

	vk::Format _swapchainFormat;
	vk::ImageLayout _finalLayout;

	vk::UniqueRenderPass _createPass(vk::Device device) override {
		std::array<vk::AttachmentDescription, 1> colorAttachments;
		colorAttachments[0]
			.setFormat(_swapchainFormat)
			.setSamples(vk::SampleCountFlagBits::e1)
			.setLoadOp(vk::AttachmentLoadOp::eLoad)
			.setStoreOp(vk::AttachmentStoreOp::eStore)
			.setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
			.setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
			.setInitialLayout(vk::ImageLayout::eColorAttachmentOptimal)
			.setFinalLayout(_finalLayout);

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
			.setSrcStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput)
			.setSrcAccessMask(vk::AccessFlagBits::eColorAttachmentWrite)
			.setDstStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput)
			.setDstAccessMask(vk::AccessFlagBits::eColorAttachmentRead);

		vk::RenderPassCreateInfo renderPassInfo;
		renderPassInfo
			.setAttachments(colorAttachments)
			.setSubpasses(subpasses)
			.setDependencies(dependencies);

		return device.createRenderPassUnique(renderPassInfo);
	}
	std::vector<PipelineCreationInfo> _getPipelineCreationInfo() override {
		return {};
	}
	void _initialize(vk::Device dev) override {
		Pass::_initialize(dev);
	}
};
