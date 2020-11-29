#pragma once

//#define SOFTWARE_RT
#define VK_ENABLE_BETA_EXTENSIONS
#include "misc.h"
#include "vma.h"
#include "glfwWindow.h"
#include "swapchain.h"
#include "transientCommandBuffer.h"
#include "sceneBuffers.h"
#include "camera.h"
#include "fpsCounter.h"

#include "passes/gBufferPass.h"
#include "passes/lightSamplePass.h"
#include "passes/softwareVisibilityTestPass.h"
#include "passes/temporalReusePass.h"
#include "passes/spatialReusePass.h"
#include "passes/lightingPass.h"
#include "passes/rtPass.h"
#include "passes/imguiPass.h"

struct PhysicalDeviceInfo {
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

	uint32_t _graphicsComputeQueueIndex = 0;
	uint32_t _presentQueueIndex = 0;

	vk::Queue _graphicsComputeQueue;
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
	vk::UniqueCommandBuffer _mainCommandBuffer;

	GBuffer _gBuffer;
	GBufferPass _gBufferPass;
	GBufferPass::Resources _gBufferResources;

	vma::UniqueBuffer _restirUniformBuffer;
	vma::UniqueBuffer _reservoirBuffer1;
	vma::UniqueBuffer _reservoirBuffer2;
	vk::DeviceSize _reservoirBufferSize;

	LightSamplePass _lightSamplePass;
	vk::UniqueDescriptorSet _lightSampleDescriptors;

	SoftwareVisibilityTestPass _swVisibilityTestPass;
	vk::UniqueDescriptorSet _swVisibilityTestDescriptors;

	LightingPass _lightingPass;
	LightingPass::Resources _lightingPassResources;

	RtPass _rtPass;

	ImGuiPass _imguiPass;
	std::vector<vk::UniqueCommandBuffer> _imguiCommandBuffers;

	nvh::GltfScene _gltfScene;
	SceneBuffers _sceneBuffers;
	int sampleNum = 50;

	AabbTree _aabbTree;
	AabbTreeBuffers _aabbTreeBuffers;

	// synchronization
	std::vector<vk::UniqueSemaphore> _imageAvailableSemaphore;
	std::vector<vk::UniqueSemaphore> _renderFinishedSemaphore;
	std::vector<vk::UniqueFence> _inFlightFences;
	std::vector<vk::UniqueFence> _inFlightImageFences;

	vk::UniqueFence _mainFence;

	// ui
	int _debugMode = GBUFFER_DEBUG_NONE;
	bool _debugModeChanged = false;
	bool _useHardwareRt = true;
	bool _useHardwareRtChanged = false;


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

	void _createAndRecordMainCommandBuffer() {
		_mainCommandBuffer.reset();

		vk::CommandBufferAllocateInfo bufferInfo;
		bufferInfo
			.setCommandPool(_commandPool.get())
			.setCommandBufferCount(1)
			.setLevel(vk::CommandBufferLevel::ePrimary);
		_mainCommandBuffer = std::move(_device->allocateCommandBuffersUnique(bufferInfo)[0]);

		vk::CommandBufferBeginInfo beginInfo;
		_mainCommandBuffer->begin(beginInfo);

		_gBufferPass.issueCommands(_mainCommandBuffer.get(), _gBuffer.getFramebuffer());
		_lightSamplePass.issueCommands(_mainCommandBuffer.get(), nullptr);
		if (_useHardwareRt) {
			_rtPass.issueCommands(_mainCommandBuffer.get(), _swapchain.getImageExtent(), _dynamicDispatcher);
		} else {
			_swVisibilityTestPass.issueCommands(_mainCommandBuffer.get(), nullptr);
		}

		_mainCommandBuffer->end();
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

	void _updateRestirBuffers() {
		_reservoirBuffer1.reset();
		_reservoirBuffer2.reset();

		uint32_t numPixels = _swapchain.getImageExtent().width * _swapchain.getImageExtent().height;
		_reservoirBufferSize = numPixels * sizeof(shader::Reservoir);
		_reservoirBuffer1 = _allocator.createTypedBuffer<shader::Reservoir>(
			numPixels, vk::BufferUsageFlagBits::eStorageBuffer, VMA_MEMORY_USAGE_GPU_ONLY
			);
		_reservoirBuffer2 = _allocator.createTypedBuffer<shader::Reservoir>(
			numPixels, vk::BufferUsageFlagBits::eStorageBuffer, VMA_MEMORY_USAGE_GPU_ONLY
			);


		_lightSamplePass.initializeDescriptorSetFor(
			_gBuffer, _sceneBuffers, _restirUniformBuffer.get(), _reservoirBuffer1.get(), _reservoirBufferSize,
			_device.get(), _lightSampleDescriptors.get()
		);

		_swVisibilityTestPass.initializeDescriptorSetFor(
			_gBuffer, _aabbTreeBuffers, _restirUniformBuffer.get(), _reservoirBuffer1.get(), _reservoirBufferSize,
			_device.get(), _swVisibilityTestDescriptors.get()
		);

		_rtPass.createDescriptorSetForRayTracing(_device.get(), _staticDescriptorPool.get(),
			_restirUniformBuffer.get(), _reservoirBuffer1.get(), _reservoirBufferSize, _dynamicDispatcher);
	}


	void _initializeLightingPassResources() {
		_lightingPassResources = LightingPass::Resources();

		_lightingPassResources.gBuffer = &_gBuffer;

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

		_lightingPass.initializeDescriptorSetFor(
			_lightingPassResources, _sceneBuffers, _reservoirBuffer1.get(), _reservoirBufferSize, _device.get()
		);
	}
};
