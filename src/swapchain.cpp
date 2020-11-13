#include "swapchain.h"

std::vector<Swapchain::BufferSet> Swapchain::getBuffers(
	vk::Device device, vk::RenderPass renderPass, vk::CommandPool commandPool
) const {
	std::vector<BufferSet> result(_swapchainImages.size());

	vk::ImageSubresourceRange range; // for all image views
	range
		.setAspectMask(vk::ImageAspectFlagBits::eColor)
		.setBaseMipLevel(0)
		.setLevelCount(1)
		.setBaseArrayLayer(0)
		.setLayerCount(1);
	for (std::size_t i = 0; i < result.size(); ++i) {
		// create image view
		vk::ImageViewCreateInfo imageViewInfo;
		imageViewInfo
			.setImage(_swapchainImages[i])
			.setViewType(vk::ImageViewType::e2D)
			.setFormat(_imageFormat)
			.setSubresourceRange(range);

		result[i].imageView = device.createImageViewUnique(imageViewInfo);

		// create framebuffer
		std::vector<vk::ImageView> attachments{ result[i].imageView.get() };
		vk::FramebufferCreateInfo framebufferInfo;
		framebufferInfo
			.setRenderPass(renderPass)
			.setAttachments(attachments)
			.setWidth(_imageExtent.width)
			.setHeight(_imageExtent.height)
			.setLayers(1);
		result[i].framebuffer = device.createFramebufferUnique(framebufferInfo);
	}

	// create command buffers
	vk::CommandBufferAllocateInfo allocInfo;
	allocInfo
		.setCommandPool(commandPool)
		.setLevel(vk::CommandBufferLevel::ePrimary)
		.setCommandBufferCount(static_cast<uint32_t>(result.size()));
	std::vector<vk::UniqueCommandBuffer> commandBuffers = device.allocateCommandBuffersUnique(allocInfo);
	for (std::size_t i = 0; i < result.size(); ++i) {
		result[i].commandBuffer = std::move(commandBuffers[i]);
	}

	return result;
}

Swapchain Swapchain::create(vk::Device device, const vk::SwapchainCreateInfoKHR &createInfo) {
	Swapchain result;
	result._swapchain = device.createSwapchainKHRUnique(createInfo);
	result._swapchainImages = device.getSwapchainImagesKHR(result._swapchain.get());
	result._imageFormat = createInfo.imageFormat;
	result._imageExtent = createInfo.imageExtent;
	return result;
}