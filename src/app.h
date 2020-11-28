#pragma once

#define VK_ENABLE_BETA_EXTENSIONS
#include "misc.h"
#include "vma.h"
#include "glfwWindow.h"
#include "swapchain.h"
#include "transientCommandBuffer.h"
#include "sceneBuffers.h"
#include "passes/demoPass.h"
#include "passes/gBufferPass.h"
#include "passes/lightingPass.h"
#include "passes/rtPass.h"
#include "passes/imguiPass.h"
#include "camera.h"
#include "fpsCounter.h"

struct PhysicalDeviceInfo
{
	vk::PhysicalDeviceMemoryProperties     memoryProperties{};
	std::vector<vk::QueueFamilyProperties> queueProperties;

	vk::PhysicalDeviceFeatures         features10{};
	vk::PhysicalDeviceVulkan11Features features11;
	vk::PhysicalDeviceVulkan12Features features12;

	vk::PhysicalDeviceProperties         properties10{};
	vk::PhysicalDeviceVulkan11Properties properties11;
	vk::PhysicalDeviceVulkan12Properties properties12;
};


class App {
public:
	constexpr static uint32_t vulkanApiVersion = VK_MAKE_VERSION(1, 2, 0);
	constexpr static std::size_t maxFramesInFlight = 2;

	App();
	~App();

	void mainLoop();
	void updateGui();

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
	FpsCounter _fpsCounter;

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
	vk::UniqueCommandPool _imguiCommandPool;
	vk::UniqueDescriptorPool _staticDescriptorPool;
	vk::UniqueDescriptorPool _textureDescriptorPool;
	vk::UniqueDescriptorPool _imguiDescriptorPool;
	vk::UniqueDescriptorPool _rtDescriptorPool;
	TransientCommandBufferPool _transientCommandBufferPool;

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

	/*DemoPass _demoPass;*/

	RtPass _rtPass;

	ImGuiPass _imguiPass;
	std::vector<vk::UniqueCommandBuffer> _imguiCommandBuffers;

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

	// ui
	int _debugMode = GBUFFER_DEBUG_NONE;
	bool _debugModeChanged = false;


	nvmath::vec2f _lastMouse;
	int _pressedMouseButton = -1;
	bool _cameraUpdated = true;

	void _onMouseMoveEvent(double x, double y);
	void _onMouseButtonEvent(int button, int action, int mods);
	void _onScrollEvent(double x, double y);

	void initPhysicalInfo(PhysicalDeviceInfo& info, vk::PhysicalDevice physicalDevice);

	void _createAndRecordSwapchainBuffers() {
		_swapchainBuffers.clear();
		_swapchainBuffers = _swapchain.getBuffers(_device.get(), _lightingPass.getPass(), _commandPool.get());

		// record command buffers
		for (std::size_t i = 0; i < _swapchainBuffers.size(); ++i) {
			const Swapchain::BufferSet &bufferSet = _swapchainBuffers[i];

			vk::CommandBufferBeginInfo beginInfo;
			bufferSet.commandBuffer->begin(beginInfo);

			transitionImageLayout(
				bufferSet.commandBuffer.get(), _swapchain.getImages()[i], _swapchain.getImageFormat(),
				vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal
			);

			_lightingPass.issueCommands(bufferSet.commandBuffer.get(), bufferSet.framebuffer.get());

			bufferSet.commandBuffer->end();
		}
	}

	void _createAndRecordGBufferCommandBuffer() {
		_gBufferCommandBuffer.reset();

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

	void _createImguiCommandBuffers() {
		_imguiCommandBuffers.clear();
		vk::CommandBufferAllocateInfo cmdBufInfo;
		cmdBufInfo
			.setCommandPool(_imguiCommandPool.get())
			.setCommandBufferCount(static_cast<uint32_t>(_swapchainBuffers.size()))
			.setLevel(vk::CommandBufferLevel::ePrimary);
		_imguiCommandBuffers = _device->allocateCommandBuffersUnique(cmdBufInfo);
	}


	void _initializeLightingPassResources() {
		_lightingPassResources = LightingPass::Resources();

		_lightingPassResources.gBuffer = &_gBuffer;
		_lightingPassResources.aabbTreeBuffers = &_aabbTreeBuffers;

		_lightingPassResources.uniformBuffer = _allocator.createTypedBuffer<shader::LightingPassUniforms>(
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

	void createAndRecordRTSwapchainBuffers(
		const Swapchain& swapchain, vk::Device device, vk::CommandPool commandPool, RtPass& rtPass, vk::DispatchLoaderDynamic dld
	) {
		std::size_t imageSize = swapchain.getNumImages();
		vk::CommandBufferAllocateInfo allocInfo;
		allocInfo
			.setCommandPool(commandPool)
			.setLevel(vk::CommandBufferLevel::ePrimary)
			.setCommandBufferCount(static_cast<uint32_t>(imageSize));
		std::vector<vk::UniqueCommandBuffer> commandBuffers = device.allocateCommandBuffersUnique(allocInfo);
		_swapchainBuffers.resize(imageSize);

		for (std::size_t i = 0; i < imageSize; i++)
		{
			_swapchainBuffers[i].commandBuffer = std::move(commandBuffers[i]);
			vk::CommandBufferBeginInfo beginInfo;
			_swapchainBuffers[i].commandBuffer->begin(beginInfo);
			rtPass.issueCommands(_swapchainBuffers[i].commandBuffer.get(),
				_swapchainBuffers[i].framebuffer.get(),
				swapchain.getImageExtent(),
				_swapchain.getImages()[i],
				dld);
			_swapchainBuffers[i].commandBuffer->end();
		}
	}
};
