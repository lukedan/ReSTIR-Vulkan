#pragma once

#include "glfwWindow.h"
#include "misc.h"
#include "swapchain.h"
#include "vma.h"
#include "sceneBuffers.h"
#include "passes/demoPass.h"
#include "passes/gBufferPass.h"
#include "passes/lightingPass.h"
#include "camera.h"

class App {
public:
	constexpr static uint32_t vulkanApiVersion = VK_MAKE_VERSION(1, 2, 0);
	constexpr static std::size_t maxFramesInFlight = 2;

	App();
	~App() {
		_device->waitIdle();
	}

	void mainLoop();

	[[nodiscard]] inline static vk::SurfaceFormatKHR chooseSurfaceFormat(
		const vk::PhysicalDevice &dev, const vk::SurfaceKHR &surface
	) {
		std::vector<vk::SurfaceFormatKHR> available = dev.getSurfaceFormatsKHR(surface);
		vk::SurfaceFormatKHR desired(vk::Format::eB8G8R8A8Srgb, vk::ColorSpaceKHR::eSrgbNonlinear);
		if (std::find(available.begin(), available.end(), desired) != available.end()) {
			return desired;
		}
		return available[0];
	}
	[[nodiscard]] inline static vk::PresentModeKHR choosePresentMode(
		const vk::PhysicalDevice &dev, vk::SurfaceKHR surface
	) {
		std::vector<vk::PresentModeKHR> available = dev.getSurfacePresentModesKHR(surface);
		if (std::find(available.begin(), available.end(), vk::PresentModeKHR::eMailbox) != available.end()) {
			return vk::PresentModeKHR::eMailbox;
		}
		return vk::PresentModeKHR::eFifo;
	}
	[[nodiscard]] inline static vk::Extent2D chooseSwapExtent(
		const vk::SurfaceCapabilitiesKHR &capabilities, const glfw::Window &wnd
	) {
		if (capabilities.currentExtent.width != std::numeric_limits<std::uint32_t>::max()) {
			return capabilities.currentExtent;
		}
		vk::Extent2D result = wnd.getFramebufferSize();
		result.width = std::clamp(
			result.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width
		);
		result.height = std::clamp(
			result.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height
		);
		return result;
	}
	[[nodiscard]] inline static uint32_t chooseImageCount(const vk::SurfaceCapabilitiesKHR &capabilities) {
		uint32_t imageCount = capabilities.minImageCount + 1;
		if (capabilities.maxImageCount != 0) {
			imageCount = std::min(imageCount, capabilities.maxImageCount);
		}
		return imageCount;
	}
protected:
	glfw::Window _window;

	Camera _camera;

	uint32_t _graphicsQueueIndex = 0;
	uint32_t _presentQueueIndex = 0;

	vk::Queue _graphicsQueue;
	vk::Queue _presentQueue;

	vk::UniqueInstance _instance;
	vk::DispatchLoaderDynamic _dynamicDispatcher;
	vk::UniqueHandle<vk::DebugUtilsMessengerEXT, vk::DispatchLoaderDynamic> _messanger;
	vk::PhysicalDevice _physicalDevice;
	vk::UniqueSurfaceKHR _surface;
	vk::UniqueDevice _device;

	vma::Allocator _allocator;
	vk::UniqueCommandPool _commandPool;
	vk::UniqueCommandPool _transientCommandPool;
	vk::UniqueDescriptorPool _staticDescriptorPool;
	vk::UniqueDescriptorPool _textureDescriptorPool;

	std::vector<uint32_t> _swapchainSharedQueues;
	vk::SwapchainCreateInfoKHR _swapchainInfo;
	Swapchain _swapchain;
	std::vector<Swapchain::BufferSet> _swapchainBuffers;

	// passes & resources
	GBuffer _gBuffer;
	GBufferPass _gBufferPass;
	vk::UniqueCommandBuffer _gBufferCommandBuffer;
	GBufferPass::Resources _gBufferResources;

	LightingPass _lightingPass;
	LightingPass::Resources _lightingPassResources;

	DemoPass _demoPass;

	nvh::GltfScene _gltfScene;
	SceneBuffers _sceneBuffers;

	AabbTree _aabbTree;
	AabbTreeBuffers _aabbTreeBuffers;

	// synchronization
	std::vector<vk::UniqueSemaphore> _imageAvailableSemaphore;
	std::vector<vk::UniqueSemaphore> _renderFinishedSemaphore;
	std::vector<vk::UniqueFence> _inFlightFences;
	std::vector<vk::UniqueFence> _inFlightImageFences;

	vk::UniqueFence _gBufferFence;


	nvmath::vec2f _lastMouse;
	int _pressedMouseButton = -1;
	bool _cameraUpdated = true;

	void _onMouseMoveEvent(double x, double y);
	void _onMouseButtonEvent(int button, int action, int mods);
	void _onScrollEvent(double x, double y);


	void _createAndRecordSwapchainBuffers(Pass &pass) {
		_swapchainBuffers = _swapchain.getBuffers(_device.get(), pass.getPass(), _commandPool.get());

		// record command buffers
		for (const Swapchain::BufferSet &bufferSet : _swapchainBuffers) {
			vk::CommandBufferBeginInfo beginInfo;
			bufferSet.commandBuffer->begin(beginInfo);
			pass.issueCommands(bufferSet.commandBuffer.get(), bufferSet.framebuffer.get());
			bufferSet.commandBuffer->end();
		}
	}

	void _createAndRecordGBufferCommandBuffer() {
		vk::CommandBufferAllocateInfo bufferInfo;
		bufferInfo
			.setCommandPool(_commandPool.get())
			.setCommandBufferCount(1)
			.setLevel(vk::CommandBufferLevel::ePrimary);
		_gBufferCommandBuffer = std::move(_device->allocateCommandBuffersUnique(bufferInfo)[0]);

		vk::CommandBufferBeginInfo beginInfo;
		_gBufferCommandBuffer->begin(beginInfo);
		_gBufferPass.issueCommands(_gBufferCommandBuffer.get(), _gBuffer.getFramebuffer());
		_gBufferCommandBuffer->end();
	}


	void _initializeLightingPassResources() {
		_lightingPassResources.gBuffer = &_gBuffer;
		_lightingPassResources.aabbTreeBuffers = &_aabbTreeBuffers;

		_lightingPassResources.uniformBuffer = _allocator.createTypedBuffer<LightingPass::Uniforms>(
			1, vk::BufferUsageFlagBits::eUniformBuffer, VMA_MEMORY_USAGE_CPU_TO_GPU
			);

		std::array<vk::DescriptorSetLayout, 1> lightingPassDescLayout{ _lightingPass.getDescriptorSetLayout() };
		vk::DescriptorSetAllocateInfo lightingPassDescAlloc;
		lightingPassDescAlloc
			.setDescriptorPool(_staticDescriptorPool.get())
			.setDescriptorSetCount(1)
			.setSetLayouts(lightingPassDescLayout);
		_lightingPassResources.descriptorSet = std::move(_device->allocateDescriptorSetsUnique(lightingPassDescAlloc)[0]);

		_lightingPass.initializeDescriptorSetFor(_lightingPassResources, _device.get());
	}


	void _executeOneTimeCommandBuffer(const std::function<void(vk::CommandBuffer)> &fillBuffer) {
		vk::CommandBufferAllocateInfo bufferInfo;
		bufferInfo
			.setCommandPool(_transientCommandPool.get())
			.setLevel(vk::CommandBufferLevel::ePrimary)
			.setCommandBufferCount(1);
		vk::UniqueCommandBuffer buffer = std::move(_device->allocateCommandBuffersUnique(bufferInfo)[0]);

		vk::CommandBufferBeginInfo beginInfo;
		beginInfo.setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
		buffer->begin(beginInfo);
		fillBuffer(buffer.get());
		buffer->end();

		std::array<vk::CommandBuffer, 1> buffers{ buffer.get() };
		vk::SubmitInfo submitInfo;
		submitInfo.setCommandBuffers(buffers);
		_graphicsQueue.submit(submitInfo, nullptr);
		_graphicsQueue.waitIdle();
	}
};