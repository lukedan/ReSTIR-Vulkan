#pragma once

#include "misc.h"
#include "vma.h"
#include "glfwWindow.h"
#include "swapchain.h"
#include "transientCommandBuffer.h"
#include "sceneBuffers.h"
#include "camera.h"
#include "fpsCounter.h"

#include "passes/gBufferPass.h"
#include "passes/spatialReusePass.h"
#include "passes/lightingPass.h"
#include "passes/restirPass.h"
#include "passes/unbiasedReusePass.h"
#include "passes/imguiPass.h"

enum class VisibilityTestMethod {
	disabled,
	software,
	hardware
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
		const vk::PhysicalDevice& dev, const vk::SurfaceKHR& surface
	) {
		std::vector<vk::SurfaceFormatKHR> available = dev.getSurfaceFormatsKHR(surface);
		vk::SurfaceFormatKHR desired(vk::Format::eB8G8R8A8Srgb, vk::ColorSpaceKHR::eSrgbNonlinear);
		if (std::find(available.begin(), available.end(), desired) != available.end()) {
			return desired;
		}
		return available[0];
	}
	[[nodiscard]] inline static vk::PresentModeKHR choosePresentMode(
		const vk::PhysicalDevice& dev, vk::SurfaceKHR surface
	) {
		std::vector<vk::PresentModeKHR> available = dev.getSurfacePresentModesKHR(surface);
		if (std::find(available.begin(), available.end(), vk::PresentModeKHR::eMailbox) != available.end()) {
			return vk::PresentModeKHR::eMailbox;
		}
		return vk::PresentModeKHR::eFifo;
	}
	[[nodiscard]] inline static vk::Extent2D chooseSwapExtent(
		const vk::SurfaceCapabilitiesKHR& capabilities, const glfw::Window& wnd
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
	[[nodiscard]] inline static uint32_t chooseImageCount(const vk::SurfaceCapabilitiesKHR& capabilities) {
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
	std::array<vma::UniqueBuffer, numGBuffers> _reservoirBuffers;
	vma::UniqueBuffer _reservoirTemporaryBuffer;
	vk::DeviceSize _reservoirBufferSize;

	SpatialReusePass _spatialReusePass;
	std::array<vk::UniqueDescriptorSet, numGBuffers> _spatialReuseDescriptors;
	std::array<vk::UniqueDescriptorSet, numGBuffers> _spatialReuseSecondDescriptors;

	LightingPass _lightingPass;
	vma::UniqueBuffer _lightingPassUniformBuffer;
	std::array<vk::UniqueDescriptorSet, numGBuffers> _lightingPassDescriptorSets;

	RestirPass _restirPass;
	std::array<vk::UniqueDescriptorSet, numGBuffers> _restirFrameDescriptors;
	vk::UniqueDescriptorSet _restirStaticDescriptor;
	vk::UniqueDescriptorSet _restirHardwareRayTraceDescriptor;
	vk::UniqueDescriptorSet _restirSoftwareRayTraceDescriptor;

	UnbiasedReusePass _unbiasedReusePass;
	std::array<vk::UniqueDescriptorSet, numGBuffers> _unbiasedReusePassFrameDescriptors;
	vk::UniqueDescriptorSet _unbiasedReusePassSwRaytraceDescriptors;
	vk::UniqueDescriptorSet _unbiasedReusePassHwRaytraceDescriptors;

	ImGuiPass _imguiPass;

	nvh::GltfScene _gltfScene;
	SceneBuffers _sceneBuffers;
	SceneRaytraceBuffers _sceneRtBuffers;

	AabbTree _aabbTree;
	AabbTreeBuffers _aabbTreeBuffers;

	float posThreshold = 0.1f;
	float norThreshold = 25.0f;
	int spatialReuseNeighbors = 5;

	// synchronization
	std::vector<vk::UniqueSemaphore> _imageAvailableSemaphore;
	std::vector<vk::UniqueSemaphore> _renderFinishedSemaphore;
	std::vector<vk::UniqueFence> _inFlightFences;
	std::vector<vk::UniqueFence> _inFlightImageFences;

	vk::UniqueFence _mainFence;

	// ui
	int _debugMode = GBUFFER_DEBUG_NONE;
	float _gamma = 1.0f;
	int _log2InitialLightSamples = 5;
	VisibilityTestMethod _visibilityTestMethod = VisibilityTestMethod::hardware;
	bool _enableTemporalReuse = true;
	int _temporalReuseSampleMultiplier = 20;
	int _spatialReuseIterations = 1;

	bool _viewParamChanged = false;
	bool _renderPathChanged = false;

	bool _unbiasedSpatialReuse = true;

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
				cmdBuf.get(), _gBuffers[i].getWorldPositionBuffer(), GBuffer::Formats::get().worldPosition,
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
			vk::CommandBufferBeginInfo beginInfo;
			_mainCommandBuffers[i]->begin(beginInfo);

			_gBufferPass.issueCommands(_mainCommandBuffers[i].get(), _gBuffers[i].getFramebuffer());

			_restirPass.staticDescriptorSet = _restirStaticDescriptor.get();
			_restirPass.frameDescriptorSet = _restirFrameDescriptors[i].get();
#ifdef RENDERDOC_CAPTURE
			_restirPass.useSoftwareRayTracing = true;
#else
			_restirPass.useSoftwareRayTracing = _visibilityTestMethod != VisibilityTestMethod::hardware;
#endif
			_restirPass.raytraceDescriptorSet =
				_restirPass.useSoftwareRayTracing ?
				_restirSoftwareRayTraceDescriptor.get() :
				_restirHardwareRayTraceDescriptor.get();

			_restirPass.bufferExtent = _swapchain.getImageExtent();
			_restirPass.issueCommands(_mainCommandBuffers[i].get(), nullptr);

			if (_unbiasedSpatialReuse) {
				_unbiasedReusePass.frameDescriptorSet = _unbiasedReusePassFrameDescriptors[i].get();
#ifdef RENDERDOC_CAPTURE
				_unbiasedReusePass.useSoftwareRayTracing = true;
#else
				_unbiasedReusePass.useSoftwareRayTracing = _visibilityTestMethod != VisibilityTestMethod::hardware;
#endif
				_unbiasedReusePass.raytraceDescriptorSet =
					_unbiasedReusePass.useSoftwareRayTracing ?
					_unbiasedReusePassSwRaytraceDescriptors.get() :
					_unbiasedReusePassHwRaytraceDescriptors.get();
				_unbiasedReusePass.bufferExtent = _swapchain.getImageExtent();
				_unbiasedReusePass.issueCommands(_mainCommandBuffers[i].get(), _dynamicDispatcher);
			} else {
				for (int j = 0; j < _spatialReuseIterations; ++j) {
					_spatialReusePass.descriptorSet = _spatialReuseDescriptors[i].get();
					_spatialReusePass.issueCommands(_mainCommandBuffers[i].get(), nullptr);
					_spatialReusePass.descriptorSet = _spatialReuseSecondDescriptors[i].get();
					_spatialReusePass.issueCommands(_mainCommandBuffers[i].get(), nullptr);
				}
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
			_reservoirTemporaryBuffer = _allocator.createTypedBuffer<shader::Reservoir>(
				numPixels,
				vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst,
				VMA_MEMORY_USAGE_GPU_ONLY
				);
			cmdBuf->fillBuffer(_reservoirTemporaryBuffer.get(), 0, VK_WHOLE_SIZE, 0);
		}

		_restirPass.initializeStaticDescriptorSetFor(
			_sceneBuffers, _restirUniformBuffer.get(), _device.get(), _restirStaticDescriptor.get()
		);
#ifndef RENDERDOC_CAPTURE
		_restirPass.initializeHardwareRayTracingDescriptorSet(
			_sceneRtBuffers, _device.get(), _restirHardwareRayTraceDescriptor.get()
		);
#endif
		_restirPass.initializeSoftwareRayTracingDescriptorSet(
			_aabbTreeBuffers, _device.get(), _restirSoftwareRayTraceDescriptor.get()
		);
		for (std::size_t i = 0; i < numGBuffers; ++i) {
			_restirPass.initializeFrameDescriptorSetFor(
				_gBuffers[i], _gBuffers[(i + numGBuffers - 1) % numGBuffers],
				_unbiasedSpatialReuse ?
				_reservoirTemporaryBuffer.get() :
				_reservoirBuffers[i].get(),
				_reservoirBuffers[(i + numGBuffers - 1) % numGBuffers].get(),
				_reservoirBufferSize,
				_device.get(), _restirFrameDescriptors[i].get()
			);

			if (_unbiasedSpatialReuse) {
				_unbiasedReusePass.initializeFrameDescriptorSet(
					_device.get(),
					_gBuffers[i], _restirUniformBuffer.get(),
					_reservoirTemporaryBuffer.get(), _reservoirBuffers[i].get(), _reservoirBufferSize,
					_unbiasedReusePassFrameDescriptors[i].get()
				);
				_unbiasedReusePass.initializeSoftwareRaytraceDescriptorSet(
					_device.get(), _aabbTreeBuffers, _unbiasedReusePassSwRaytraceDescriptors.get()
				);
#ifndef RENDERDOC_CAPTURE
				_unbiasedReusePass.initializeHardwareRaytraceDescriptorSet(
					_device.get(), _sceneRtBuffers, _unbiasedReusePassHwRaytraceDescriptors.get()
				);
#endif
			} else {
				_spatialReusePass.initializeDescriptorSetFor(
					_gBuffers[i], _restirUniformBuffer.get(), _reservoirBuffers[i].get(), _reservoirBufferSize,
					_reservoirBuffers[(i + numGBuffers - 1) % numGBuffers].get(),
					_device.get(), _spatialReuseDescriptors[i].get()
				);
				_spatialReusePass.initializeDescriptorSetFor(
					_gBuffers[i], _restirUniformBuffer.get(), _reservoirBuffers[(i + numGBuffers - 1) % numGBuffers].get(), _reservoirBufferSize,
					_reservoirBuffers[i].get(), _device.get(), _spatialReuseSecondDescriptors[i].get()
				);
			}

		}
	}

	void _initializeLightingPassResources() {
		for (std::size_t i = 0; i < numGBuffers; ++i) {
			_lightingPass.initializeDescriptorSetFor(
				_gBuffers[i], _sceneBuffers,
				_lightingPassUniformBuffer.get(), _reservoirBuffers[i].get(), _reservoirBufferSize,
				_device.get(), _lightingPassDescriptorSets[i].get()
			);
		}
	}
};
