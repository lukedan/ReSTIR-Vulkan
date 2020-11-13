#include <iostream>
#include <unordered_set>

#include <vulkan/vulkan.hpp>

#include "glfwWindow.h"
#include "misc.h"
#include "swapchain.h"

vk::SurfaceFormatKHR chooseSurfaceFormat(const vk::PhysicalDevice &dev, const vk::SurfaceKHR &surface) {
	std::vector<vk::SurfaceFormatKHR> available = dev.getSurfaceFormatsKHR(surface);
	vk::SurfaceFormatKHR desired(vk::Format::eB8G8R8A8Srgb, vk::ColorSpaceKHR::eSrgbNonlinear);
	if (std::find(available.begin(), available.end(), desired) != available.end()) {
		return desired;
	}
	return available[0];
}

vk::PresentModeKHR choosePresentMode(const vk::PhysicalDevice &dev, vk::SurfaceKHR surface) {
	std::vector<vk::PresentModeKHR> available = dev.getSurfacePresentModesKHR(surface);
	if (std::find(available.begin(), available.end(), vk::PresentModeKHR::eMailbox) != available.end()) {
		return vk::PresentModeKHR::eMailbox;
	}
	return vk::PresentModeKHR::eFifo;
}

vk::Extent2D chooseSwapExtent(
	const vk::SurfaceCapabilitiesKHR &capabilities, const GlfwWindow &wnd
) {
	if (capabilities.currentExtent.width != std::numeric_limits<std::uint32_t>::max()) {
		return capabilities.currentExtent;
	}
	vk::Extent2D result = wnd.getFramebufferSize();
	result.width = std::clamp(result.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
	result.height = std::clamp(result.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
	return result;
}


std::vector<Swapchain::BufferSet> createAndRecordSwapchainBuffers(
	const Swapchain &swapchain, vk::Device device, vk::RenderPass renderPass, vk::CommandPool commandPool, vk::Pipeline graphicsPipeline
) {
	std::vector<Swapchain::BufferSet> swapchainBuffers = swapchain.getBuffers(device, renderPass, commandPool);

	// record command buffers
	for (const Swapchain::BufferSet &bufferSet : swapchainBuffers) {
		vk::CommandBufferBeginInfo beginInfo;
		bufferSet.commandBuffer->begin(beginInfo);

		std::vector<vk::ClearValue> clearValues{
			vk::ClearValue(std::array<float, 4>{ 0.0f, 0.0f, 0.0f, 1.0f })
		};
		vk::RenderPassBeginInfo passBeginInfo;
		passBeginInfo
			.setRenderPass(renderPass)
			.setFramebuffer(bufferSet.framebuffer.get())
			.setRenderArea(vk::Rect2D(vk::Offset2D(0, 0), swapchain.getImageExtent()))
			.setClearValues(clearValues);
		bufferSet.commandBuffer->beginRenderPass(passBeginInfo, vk::SubpassContents::eInline);

		bufferSet.commandBuffer->setViewport(0, { vk::Viewport(
			0.0f, 0.0f, swapchain.getImageExtent().width, swapchain.getImageExtent().height, 0.0f, 1.0f
		) });
		bufferSet.commandBuffer->setScissor(0, { vk::Rect2D(vk::Offset2D(0, 0), swapchain.getImageExtent()) });

		bufferSet.commandBuffer->bindPipeline(vk::PipelineBindPoint::eGraphics, graphicsPipeline);
		bufferSet.commandBuffer->draw(3, 1, 0, 0);

		bufferSet.commandBuffer->endRenderPass();
		bufferSet.commandBuffer->end();
	}

	return swapchainBuffers;
}


uint32_t chooseImageCount(const vk::SurfaceCapabilitiesKHR &capabilities) {
	uint32_t imageCount = capabilities.minImageCount + 1;
	if (capabilities.maxImageCount != 0) {
		imageCount = std::min(imageCount, capabilities.maxImageCount);
	}
	return imageCount;
}

int main() {
	std::cout << "Hello World" << std::endl;
}
