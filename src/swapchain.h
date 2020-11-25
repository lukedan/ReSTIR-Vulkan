#pragma once

#include <vector>

#include <vulkan/vulkan.hpp>

#include "glfwWindow.h"

class Swapchain {
public:
	struct BufferSet {
		vk::UniqueImageView imageView;
		vk::UniqueFramebuffer framebuffer;
		vk::UniqueCommandBuffer commandBuffer;
	};

	[[nodiscard]] std::vector<BufferSet> getBuffers(vk::Device, vk::RenderPass, vk::CommandPool) const;

	void reset() {
		_swapchainImages.clear();
		_swapchain.reset();
	}

	[[nodiscard]] const vk::UniqueSwapchainKHR &getSwapchain() const {
		return _swapchain;
	}
	[[nodiscard]] std::size_t getNumImages() const {
		return _swapchainImages.size();
	}
	[[nodiscard]] vk::Format getImageFormat() const {
		return _imageFormat;
	}
	[[nodiscard]] vk::Extent2D getImageExtent() const {
		return _imageExtent;
	}
	[[nodiscard]] const std::vector<vk::Image> &getImages() const {
		return _swapchainImages;
	}

	[[nodiscard]] vk::Image& getImageAtIndexs(uint32_t i)
	{
		return _swapchainImages[i];
	}

	[[nodiscard]] static Swapchain create(vk::Device, const vk::SwapchainCreateInfoKHR&);
private:
	std::vector<vk::Image> _swapchainImages;
	vk::UniqueSwapchainKHR _swapchain;
	vk::Format _imageFormat = vk::Format::eUndefined;
	vk::Extent2D _imageExtent;
};
