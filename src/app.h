#pragma once

/*#define RENDERDOC_CAPTURE*/

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
	constexpr static std::size_t numGBuffers = 2;

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
	std::array<vk::UniqueCommandBuffer, numGBuffers> _mainCommandBuffers;

	GBuffer _gBuffers[2];
	GBufferPass _gBufferPass;
	GBufferPass::Resources _gBufferResources;

	vma::UniqueBuffer _restirUniformBuffer;
	std::array<vma::UniqueBuffer, 2> _reservoirBuffers;
	vk::DeviceSize _reservoirBufferSize;

	LightSamplePass _lightSamplePass;
	std::array<vk::UniqueDescriptorSet, numGBuffers> _lightSampleDescriptors;

	SoftwareVisibilityTestPass _swVisibilityTestPass;
	std::array<vk::UniqueDescriptorSet, numGBuffers> _swVisibilityTestDescriptors;

	SpatialReusePass _spatialReusePass;
	std::array<vk::UniqueDescriptorSet, numGBuffers> _spatialReuseDescriptors;
	std::array<vk::UniqueDescriptorSet, numGBuffers> _spatialReuseSecondDescriptors;

	TemporalReusePass _temporalReusePass;
	std::array<vk::UniqueDescriptorSet, numGBuffers> _temporalReuseDescriptors;

	LightingPass _lightingPass;
	vma::UniqueBuffer _lightingPassUniformBuffer;
	std::array<vk::UniqueDescriptorSet, numGBuffers> _lightingPassDescriptorSets;

	RtPass _rtPass;
	std::array<vk::UniqueDescriptorSet, numGBuffers> _rtPassDescriptors;

	ImGuiPass _imguiPass;

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
	bool _useHardwareRt = true;
	bool _enableTemporalReuse = true;
	bool _enableSpatialReuse = true;

	bool _debugModeChanged = false;
	bool _renderPathChanged = false;


	nvmath::vec2f _lastMouse;
	int _pressedMouseButton = -1;
	bool _cameraUpdated = true;


	void _onMouseMoveEvent(double x, double y);
	void _onMouseButtonEvent(int button, int action, int mods);
	void _onScrollEvent(double x, double y);

	void _createSwapchainBuffers() {
		_swapchainBuffers.clear();
		_swapchainBuffers = _swapchain.getBuffers(_device.get(), _lightingPass.getPass(), _commandPool.get());
	}

	void _transitionGBufferLayouts() {
		TransientCommandBuffer cmdBuf = _transientCommandBufferPool.begin(_graphicsComputeQueue);
		for (std::size_t i = 1; i < numGBuffers; ++i) {
			transitionImageLayout(
				cmdBuf.get(), _gBuffers[i].getAlbedoBuffer(), GBuffer::Formats::get().albedo,
				vk::ImageLayout::eUndefined, vk::ImageLayout::eShaderReadOnlyOptimal
			);
			transitionImageLayout(
				cmdBuf.get(), _gBuffers[i].getNormalBuffer(), GBuffer::Formats::get().normal,
				vk::ImageLayout::eUndefined, vk::ImageLayout::eShaderReadOnlyOptimal
			);
			transitionImageLayout(
				cmdBuf.get(), _gBuffers[i].getDepthBuffer(), GBuffer::Formats::get().depth,
				vk::ImageLayout::eUndefined, vk::ImageLayout::eShaderReadOnlyOptimal
			);
		}
	}

	void _recordMainCommandBuffers() {
		for (std::size_t i = 0; i < numGBuffers; ++i) {
			_lightSamplePass.descriptorSet = _lightSampleDescriptors[i].get();
			_swVisibilityTestPass.descriptorSet = _swVisibilityTestDescriptors[i].get();
			_rtPass.descriptorSet = _rtPassDescriptors[i].get();
			_temporalReusePass.descriptorSet = _temporalReuseDescriptors[i].get();
			_spatialReusePass.descriptorSet = _spatialReuseDescriptors[i].get();

			vk::CommandBufferBeginInfo beginInfo;
			_mainCommandBuffers[i]->begin(beginInfo);

			_gBufferPass.issueCommands(_mainCommandBuffers[i].get(), _gBuffers[i].getFramebuffer());
			_lightSamplePass.issueCommands(_mainCommandBuffers[i].get(), nullptr);
			if (_useHardwareRt) {
#ifndef RENDERDOC_CAPTURE
				_rtPass.issueCommands(_mainCommandBuffers[i].get(), _swapchain.getImageExtent(), _dynamicDispatcher);
#endif
			} else {
				_swVisibilityTestPass.issueCommands(_mainCommandBuffers[i].get(), nullptr);
			}
			if (_enableTemporalReuse) {
				_temporalReusePass.issueCommands(_mainCommandBuffers[i].get(), nullptr);
			}
			if (_enableSpatialReuse) 
			{
				_spatialReusePass.issueCommands(_mainCommandBuffers[i].get(), nullptr);
				_spatialReusePass.descriptorSet = _spatialReuseSecondDescriptors[i].get();
				_spatialReusePass.issueCommands(_mainCommandBuffers[i].get(), nullptr);
				_spatialReusePass.descriptorSet = _spatialReuseDescriptors[i].get();
			}

			_mainCommandBuffers[i]->end();
		}
	}

	void _updateRestirBuffers() {
		uint32_t numPixels = _swapchain.getImageExtent().width * _swapchain.getImageExtent().height;
		_reservoirBufferSize = numPixels * sizeof(shader::Reservoir);
		{
			TransientCommandBuffer cmdBuf = _transientCommandBufferPool.begin(_graphicsComputeQueue);
			for (std::size_t i = 0; i < numGBuffers; ++i) {
				_reservoirBuffers[i] = _allocator.createTypedBuffer<shader::Reservoir>(
					numPixels,
					vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst,
					VMA_MEMORY_USAGE_GPU_ONLY
					);
				// zero-initialize reservoir buffers
				cmdBuf->fillBuffer(_reservoirBuffers[i].get(), 0, VK_WHOLE_SIZE, 0);
			}
		}

		for (std::size_t i = 0; i < numGBuffers; ++i) {
			_lightSamplePass.initializeDescriptorSetFor(
				_gBuffers[i], _sceneBuffers, _restirUniformBuffer.get(), _reservoirBuffers[i].get(), _reservoirBufferSize,
				_device.get(), _lightSampleDescriptors[i].get()
			);

			_swVisibilityTestPass.initializeDescriptorSetFor(
				_gBuffers[i], _aabbTreeBuffers, _restirUniformBuffer.get(), _reservoirBuffers[i].get(), _reservoirBufferSize,
				_device.get(), _swVisibilityTestDescriptors[i].get()
			);

#ifndef RENDERDOC_CAPTURE
			_rtPass.createDescriptorSetForRayTracing(
				_device.get(),
				_gBuffers[i], _restirUniformBuffer.get(), _reservoirBuffers[i].get(), _reservoirBufferSize,
				_rtPassDescriptors[i].get(),
				_dynamicDispatcher
			);
#endif
          
			_temporalReusePass.initializeDescriptorSetFor(
				_gBuffers[i], _gBuffers[(i + numGBuffers - 1) % numGBuffers], _restirUniformBuffer.get(),
				_reservoirBuffers[i].get(), _reservoirBuffers[(i + numGBuffers - 1) % numGBuffers].get(), _reservoirBufferSize,
				_device.get(), _temporalReuseDescriptors[i].get()
			);

			_spatialReusePass.initializeDescriptorSetFor(
				_gBuffers[i], _restirUniformBuffer.get(), _reservoirBuffers[i].get(), _reservoirBufferSize,
				_reservoirBuffers[(i + numGBuffers - 1) % numGBuffers].get(), _device.get(), _spatialReuseDescriptors[i].get()
			);

			_spatialReusePass.initializeDescriptorSetFor(
				_gBuffers[i], _restirUniformBuffer.get(), _reservoirBuffers[(i + numGBuffers - 1) % numGBuffers].get(), _reservoirBufferSize,
				_reservoirBuffers[i].get(), _device.get(), _spatialReuseSecondDescriptors[i].get()
			);
		}
	}

	void _initializeLightingPassResources() {
		_lightingPassUniformBuffer = _allocator.createTypedBuffer<shader::LightingPassUniforms>(
			1, vk::BufferUsageFlagBits::eUniformBuffer, VMA_MEMORY_USAGE_CPU_TO_GPU
			);

		std::array<vk::DescriptorSetLayout, numGBuffers> lightingPassDescLayout;
		std::fill(lightingPassDescLayout.begin(), lightingPassDescLayout.end(), _lightingPass.getDescriptorSetLayout());
		vk::DescriptorSetAllocateInfo lightingPassDescAlloc;
		lightingPassDescAlloc
			.setDescriptorPool(_staticDescriptorPool.get())
			.setSetLayouts(lightingPassDescLayout);
		auto descriptorSets = _device->allocateDescriptorSetsUnique(lightingPassDescAlloc);
		std::move(descriptorSets.begin(), descriptorSets.end(), _lightingPassDescriptorSets.begin());

		for (std::size_t i = 0; i < numGBuffers; ++i) {
			_lightingPass.initializeDescriptorSetFor(
				_gBuffers[i], _sceneBuffers,
				_lightingPassUniformBuffer.get(), _reservoirBuffers[i].get(), _reservoirBufferSize,
				_device.get(), _lightingPassDescriptorSets[i].get()
			);
		}
	}
};
