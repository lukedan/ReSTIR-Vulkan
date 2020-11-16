#include "app.h"

VKAPI_ATTR VkBool32 VKAPI_CALL _debugCallback(
	VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
	VkDebugUtilsMessageTypeFlagsEXT messageType,
	const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
	void *pUserData
) {
	if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
		std::cerr << pCallbackData->pMessage << std::endl;
#ifdef _MSC_VER
		__debugbreak();
#endif
	}
	return VK_FALSE;
}

App::App() : _window({ { GLFW_CLIENT_API, GLFW_NO_API } }) {
	{
		vk::Extent2D extent = _window.getFramebufferSize();
		_camera.aspectRatio = extent.width / static_cast<float>(extent.height);
		_camera.recomputeAttributes();
	}

	std::vector<const char*> requiredExtensions = GlfwWindow::getRequiredInstanceExtensions();
	requiredExtensions.emplace_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
	std::vector<const char*> requiredDeviceExtensions{
		VK_KHR_SWAPCHAIN_EXTENSION_NAME
	};
	std::vector<const char*> requiredLayers{
		"VK_LAYER_KHRONOS_validation"
	};

	{ // check extension & layer support
		bool supportsExtensions = checkSupport<&vk::ExtensionProperties::extensionName>(
			requiredExtensions, vk::enumerateInstanceExtensionProperties(), "extensions"
			);
		if (!supportsExtensions) {
			std::abort();
		}
		bool supportsLayers = checkSupport<&vk::LayerProperties::layerName>(
			requiredLayers, vk::enumerateInstanceLayerProperties(), "layers"
			);
		if (!supportsLayers) {
			std::abort();
		}
	}

	{ // create instance
		vk::ApplicationInfo appInfo;
		appInfo
			.setPApplicationName("ReSTIR")
			.setApiVersion(vulkanApiVersion);
		vk::InstanceCreateInfo instanceInfo;
		instanceInfo
			.setPApplicationInfo(&appInfo)
			.setPEnabledExtensionNames(requiredExtensions)
			.setPEnabledLayerNames(requiredLayers);
		_instance = vk::createInstanceUnique(instanceInfo);
	}

	_dynamicDispatcher = vk::DispatchLoaderDynamic(_instance.get(), vkGetInstanceProcAddr);

	{ // create debug messanger
		vk::DebugUtilsMessengerCreateInfoEXT messangerInfo;
		messangerInfo
			.setMessageSeverity(
				vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose |
				vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo |
				vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
				vk::DebugUtilsMessageSeverityFlagBitsEXT::eError
			)
			.setMessageType(
				vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
				vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance |
				vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation
			)
			.setPfnUserCallback(_debugCallback);
		_messanger = _instance->createDebugUtilsMessengerEXTUnique(messangerInfo, nullptr, _dynamicDispatcher);
	}

	{ // pick physical device
		auto physicalDevices = _instance->enumeratePhysicalDevices();
		for (const vk::PhysicalDevice &dev : physicalDevices) {
			auto props = dev.getProperties();
			std::cout << "Device " << props.deviceName << ":\n";
			std::cout << "    Vendor ID: " << props.vendorID << "\n";
			std::cout << "    Device Type: " << vk::to_string(props.deviceType) << "\n";
			std::cout << "    Driver version: " << props.driverVersion << "\n";
			std::cout << "\n";
			if (props.deviceType != vk::PhysicalDeviceType::eDiscreteGpu) {
				continue;
			}
			bool supportsExtensions = checkSupport<&vk::ExtensionProperties::extensionName>(
				requiredDeviceExtensions, dev.enumerateDeviceExtensionProperties(),
				"device extensions", "    "
				);
			if (!supportsExtensions) {
				continue;
			}
			_physicalDevice = dev;
		}
	}
	if (!_physicalDevice) {
		std::cout << "Failed to find suitable discrete gpu device\n";
		std::abort();
	}

	_surface = _window.createSurface(_instance.get());

	{
		auto queueFamilyProps = _physicalDevice.getQueueFamilyProperties();
		std::cout << "Queue families:\n";
		for (std::size_t i = 0; i < queueFamilyProps.size(); ++i) {
			const vk::QueueFamilyProperties &props = queueFamilyProps[i];
			std::cout << "    " << i << ": " << vk::to_string(props.queueFlags);
			if (props.queueFlags & vk::QueueFlagBits::eGraphics) {
				_graphicsQueueIndex = static_cast<uint32_t>(i);
			}
			if (_physicalDevice.getSurfaceSupportKHR(static_cast<uint32_t>(i), _surface.get())) {
				_presentQueueIndex = static_cast<uint32_t>(i);
				std::cout << " [Surface support]";
			}
			std::cout << "\n";
		}
		std::cout << "\n";
		_graphicsQueueIndex = static_cast<uint32_t>(std::find_if(
			queueFamilyProps.begin(), queueFamilyProps.end(),
			[](const vk::QueueFamilyProperties &props) {
				return props.queueFlags & vk::QueueFlagBits::eGraphics;
			}
		) - queueFamilyProps.begin());

		std::vector<vk::DeviceQueueCreateInfo> queueInfos;
		std::vector<float> queuePriorities{ 1.0f };
		queueInfos.emplace_back()
			.setQueueFamilyIndex(_graphicsQueueIndex)
			.setQueueCount(1)
			.setQueuePriorities(queuePriorities);
		queueInfos.emplace_back()
			.setQueueFamilyIndex(_presentQueueIndex)
			.setQueueCount(1)
			.setQueuePriorities(queuePriorities);

		vk::PhysicalDeviceFeatures deviceFeatures;

		vk::DeviceCreateInfo deviceInfo;
		deviceInfo
			.setQueueCreateInfos(queueInfos)
			.setPEnabledFeatures(&deviceFeatures)
			.setPEnabledExtensionNames(requiredDeviceExtensions);
		_device = _physicalDevice.createDeviceUnique(deviceInfo);
	}


	_allocator = vma::Allocator::create(vulkanApiVersion, _instance.get(), _physicalDevice, _device.get());

	loadScene("../../../scenes/boxTextured/BoxTextured.gltf", _gltfScene);
	_sceneBuffers = SceneBuffers::create(_gltfScene, _allocator);

	{ // create command pool
		vk::CommandPoolCreateInfo poolInfo;
		poolInfo
			.setQueueFamilyIndex(_graphicsQueueIndex)
			.setFlags(vk::CommandPoolCreateFlags());

		_commandPool = _device->createCommandPoolUnique(poolInfo);
	}

	{
		_swapchainSharedQueues = { _graphicsQueueIndex, _presentQueueIndex };
		vk::SurfaceCapabilitiesKHR capabilities = _physicalDevice.getSurfaceCapabilitiesKHR(_surface.get());
		vk::SurfaceFormatKHR surfaceFormat = chooseSurfaceFormat(_physicalDevice, _surface.get());

		_swapchainInfo
			.setSurface(_surface.get())
			.setMinImageCount(chooseImageCount(capabilities))
			.setImageFormat(surfaceFormat.format)
			.setImageColorSpace(surfaceFormat.colorSpace)
			.setImageExtent(chooseSwapExtent(capabilities, _window))
			.setPresentMode(choosePresentMode(_physicalDevice, _surface.get()))
			.setImageArrayLayers(1)
			.setPreTransform(capabilities.currentTransform)
			.setCompositeAlpha(vk::CompositeAlphaFlagBitsKHR::eOpaque)
			.setClipped(true)
			.setImageUsage(vk::ImageUsageFlagBits::eColorAttachment);

		if (_graphicsQueueIndex == _presentQueueIndex) {
			_swapchainInfo.setImageSharingMode(vk::SharingMode::eExclusive);
		} else {
			_swapchainInfo
				.setImageSharingMode(vk::SharingMode::eConcurrent)
				.setQueueFamilyIndices(_swapchainSharedQueues);
		}

		_swapchain = Swapchain::create(_device.get(), _swapchainInfo);
	}

	{ // create descriptor pool
		std::array<vk::DescriptorPoolSize, 1> poolSizes{
			vk::DescriptorPoolSize(vk::DescriptorType::eCombinedImageSampler, 3)
		};
		vk::DescriptorPoolCreateInfo poolInfo;
		poolInfo.setPoolSizes(poolSizes);
		poolInfo.setMaxSets(1);
		_descriptorPool = _device->createDescriptorPoolUnique(poolInfo);
	}


	_graphicsQueue = _device->getQueue(_graphicsQueueIndex, 0);
	_presentQueue = _device->getQueue(_presentQueueIndex, 0);


	GBuffer::Formats::initialize(_physicalDevice);
	_gBufferPass = Pass::create<GBufferPass>(_device.get(), _swapchain.getImageExtent());
	_gBufferPass.camera = &_camera;
	_gBufferPass.scene = &_gltfScene;
	_gBufferPass.sceneBuffers = &_sceneBuffers;
	_gBuffer = GBuffer::create(_allocator, _device.get(), _swapchain.getImageExtent(), _gBufferPass);
	{
		vk::CommandBufferAllocateInfo bufferInfo;
		bufferInfo
			.setCommandPool(_commandPool.get())
			.setCommandBufferCount(1)
			.setLevel(vk::CommandBufferLevel::ePrimary);
		_gBufferCommandBuffer = std::move(_device->allocateCommandBuffersUnique(bufferInfo)[0]);
	}

	_demoPass = Pass::create<DemoPass>(_device.get(), _swapchain.getImageFormat());
	_demoPass.imageExtent = _swapchain.getImageExtent();

	_lightingPass = Pass::create<LightingPass>(_device.get(), _swapchain.getImageFormat());
	_lightingPassDescriptor = _lightingPass.createDescriptorSetFor(_gBuffer, _device.get(), _descriptorPool.get());

	_lightingPass.imageExtent = _swapchain.getImageExtent();
	_lightingPass.descriptorSet = _lightingPassDescriptor.get();

	createAndRecordSwapchainBuffers(
		_swapchain, _device.get(), _commandPool.get(), _lightingPass
	);

	{
		vk::CommandBufferBeginInfo beginInfo;
		beginInfo.setFlags(vk::CommandBufferUsageFlagBits::eSimultaneousUse);
		_gBufferCommandBuffer->begin(beginInfo);
		_gBufferPass.issueCommands(_gBufferCommandBuffer.get(), _gBuffer.getFramebuffer());
		_gBufferCommandBuffer->end();
	}


	// semaphores
	_imageAvailableSemaphore.resize(maxFramesInFlight);
	_renderFinishedSemaphore.resize(maxFramesInFlight);
	_inFlightFences.resize(maxFramesInFlight);
	_inFlightImageFences.resize(_swapchainBuffers.size());
	std::size_t currentFrame = 0;
	for (std::size_t i = 0; i < maxFramesInFlight; ++i) {
		vk::SemaphoreCreateInfo semaphoreInfo;
		_imageAvailableSemaphore[i] = _device->createSemaphoreUnique(semaphoreInfo);
		_renderFinishedSemaphore[i] = _device->createSemaphoreUnique(semaphoreInfo);

		vk::FenceCreateInfo fenceInfo;
		fenceInfo
			.setFlags(vk::FenceCreateFlagBits::eSignaled);
		_inFlightFences[i] = _device->createFenceUnique(fenceInfo);
	}
}

void App::mainLoop() {
	std::size_t currentFrame = 0;
	bool needsResize = false;
	vk::Extent2D windowSize = _window.getFramebufferSize();
	while (!_window.shouldClose()) {
		glfwPollEvents();

		vk::Extent2D newWindowSize = _window.getFramebufferSize();
		if (needsResize || newWindowSize != windowSize) {
			_device->waitIdle();

			while (newWindowSize.width == 0 && newWindowSize.height == 0) {
				glfwWaitEvents();
				newWindowSize = _window.getFramebufferSize();
			}
			_swapchainInfo.setImageExtent(chooseSwapExtent(
				_physicalDevice.getSurfaceCapabilitiesKHR(_surface.get()), _window
			));


			_swapchainBuffers.clear();
			_swapchain.reset();

			_swapchain = Swapchain::create(_device.get(), _swapchainInfo);

			_gBuffer.resize(_allocator, _device.get(), _swapchain.getImageExtent(), _gBufferPass);
			_gBufferPass.onResized(_device.get(), _swapchain.getImageExtent());

			_demoPass.imageExtent = _swapchain.getImageExtent();

			_lightingPass.imageExtent = _swapchain.getImageExtent();

			createAndRecordSwapchainBuffers(_swapchain, _device.get(), _commandPool.get(), _lightingPass);
		}

		_device->waitForFences(
			{ _inFlightFences[currentFrame].get() }, true, std::numeric_limits<std::uint64_t>::max()
		);

		auto [result, imageIndex] = _device->acquireNextImageKHR(
			_swapchain.getSwapchain().get(), std::numeric_limits<std::uint64_t>::max(),
			_imageAvailableSemaphore[currentFrame].get(), nullptr
		);
		if (result == vk::Result::eErrorOutOfDateKHR || result == vk::Result::eSuboptimalKHR) {
			needsResize = true;
			continue;
		}

		if (_inFlightImageFences[imageIndex]) {
			_device->waitForFences(
				{ _inFlightImageFences[imageIndex].get() }, true, std::numeric_limits<std::uint64_t>::max()
			);
		}


		_device->resetFences({ _inFlightFences[currentFrame].get() });

		std::array<vk::Semaphore, 1> signalSemaphores{ _renderFinishedSemaphore[currentFrame].get() };

		{
			std::array<vk::CommandBuffer, 1> gBufferCommandBuffers{ _gBufferCommandBuffer.get() };
			vk::SubmitInfo submitInfo;
			submitInfo
				.setCommandBuffers(gBufferCommandBuffers);
			_graphicsQueue.submit({ submitInfo }, nullptr);
		}

		{
			std::vector<vk::Semaphore> waitSemaphores{ _imageAvailableSemaphore[currentFrame].get() };
			std::vector<vk::CommandBuffer> cmdBuffers{ _swapchainBuffers[imageIndex].commandBuffer.get() };
			std::vector<vk::PipelineStageFlags> waitStages{ vk::PipelineStageFlagBits::eColorAttachmentOutput };
			vk::SubmitInfo submitInfo;
			submitInfo
				.setWaitSemaphores(waitSemaphores)
				.setWaitDstStageMask(waitStages)
				.setCommandBuffers(cmdBuffers)
				.setSignalSemaphores(signalSemaphores);
			_graphicsQueue.submit({ submitInfo }, _inFlightFences[currentFrame].get());
		}

		std::vector<vk::SwapchainKHR> swapchains{ _swapchain.getSwapchain().get() };
		std::vector<std::uint32_t> imageIndices{ imageIndex };

		vk::PresentInfoKHR presentInfo;
		presentInfo
			.setWaitSemaphores(signalSemaphores)
			.setSwapchains(swapchains)
			.setImageIndices(imageIndices);
		try {
			_presentQueue.presentKHR(presentInfo);
		} catch (const vk::OutOfDateKHRError&) {
			needsResize = true;
		}

		currentFrame = (currentFrame + 1) % maxFramesInFlight;
	}
}
