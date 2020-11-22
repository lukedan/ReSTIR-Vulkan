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
	_window.setMouseButtonHandler([this](int button, int action, int mods) {
		_onMouseButtonEvent(button, action, mods);
		});
	_window.setCursorPosHandler([this](double x, double y) {
		_onMouseMoveEvent(x, y);
		});
	_window.setScrollHandler([this](double x, double y) {
		_onScrollEvent(x, y);
		});

	{
		vk::Extent2D extent = _window.getFramebufferSize();
		_camera.aspectRatio = extent.width / static_cast<float>(extent.height);
		_camera.recomputeAttributes();
	}

	std::vector<const char*> requiredExtensions = glfw::getRequiredInstanceExtensions();
	requiredExtensions.emplace_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
	requiredExtensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
	std::vector<const char*> requiredDeviceExtensions{
		VK_KHR_SWAPCHAIN_EXTENSION_NAME,
		// VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
		VK_KHR_MAINTENANCE3_EXTENSION_NAME,
		VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME
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

		// vk::PhysicalDeviceFeatures deviceFeatures;
		vk::PhysicalDeviceFeatures2 deviceFeatures;
		vk::PhysicalDeviceDescriptorIndexingFeatures idxFeature;
		idxFeature
			.setRuntimeDescriptorArray(VkBool32(true))
			.setShaderSampledImageArrayNonUniformIndexing(VkBool32(true));
		deviceFeatures.setPNext(&idxFeature);

		vk::DeviceCreateInfo deviceInfo;
		deviceInfo
			.setQueueCreateInfos(queueInfos)
			.setPEnabledFeatures(&deviceFeatures.features)
			.setPEnabledExtensionNames(requiredDeviceExtensions)
			.setPNext(&idxFeature);
		_device = _physicalDevice.createDeviceUnique(deviceInfo);
	}


	_allocator = vma::Allocator::create(vulkanApiVersion, _instance.get(), _physicalDevice, _device.get());

	{ // create command pool
		vk::CommandPoolCreateInfo poolInfo;
		poolInfo
			.setQueueFamilyIndex(_graphicsQueueIndex)
			.setFlags(vk::CommandPoolCreateFlagBits::eResetCommandBuffer);
		_commandPool = _device->createCommandPoolUnique(poolInfo);

		vk::CommandPoolCreateInfo transientPoolInfo;
		transientPoolInfo
			.setQueueFamilyIndex(_graphicsQueueIndex)
			.setFlags(vk::CommandPoolCreateFlagBits::eTransient);
		_transientCommandPool = _device->createCommandPoolUnique(transientPoolInfo);
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

	// loadScene("../../../scenes/cornellBox/cornellBox.gltf", _gltfScene);
	// loadScene("../../../scenes/boxTextured/boxTextured.gltf", _gltfScene);
	// loadScene("../../../scenes/duck/Duck.gltf", _gltfScene);
	// loadScene("../../../scenes/fish/BarramundiFish.gltf", _gltfScene);
	loadScene("../../../scenes/Sponza/glTF/Sponza.gltf", _gltfScene);

	{ // create descriptor pools
		std::array<vk::DescriptorPoolSize, 4> staticPoolSizes{
			vk::DescriptorPoolSize(vk::DescriptorType::eCombinedImageSampler, 3),
			vk::DescriptorPoolSize(vk::DescriptorType::eStorageBuffer, 2),
			vk::DescriptorPoolSize(vk::DescriptorType::eUniformBuffer, 2),
			vk::DescriptorPoolSize(vk::DescriptorType::eUniformBufferDynamic, 1)
		};
		vk::DescriptorPoolCreateInfo staticPoolInfo;
		staticPoolInfo
			.setFlags(vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet)
			.setPoolSizes(staticPoolSizes)
			.setMaxSets(3);
		_staticDescriptorPool = _device->createDescriptorPoolUnique(staticPoolInfo);

		std::array<vk::DescriptorPoolSize, 1> texturePoolSizes{
			vk::DescriptorPoolSize(vk::DescriptorType::eCombinedImageSampler, _gltfScene.m_textures.size())
		};
		vk::DescriptorPoolCreateInfo texturePoolInfo;
		texturePoolInfo
			.setFlags(vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet)
			.setPoolSizes(texturePoolSizes)
			.setMaxSets(_gltfScene.m_materials.size());
		_textureDescriptorPool = _device->createDescriptorPoolUnique(texturePoolInfo);
	}


	_graphicsQueue = _device->getQueue(_graphicsQueueIndex, 0);
	_presentQueue = _device->getQueue(_presentQueueIndex, 0);

	_sceneBuffers = SceneBuffers::create(_gltfScene, _allocator, _physicalDevice, _device.get(), _graphicsQueue, _commandPool.get());
	std::cout << "Building AABB tree...";
	_aabbTree = AabbTree::build(_gltfScene);
	std::cout << " done\n";
	_aabbTreeBuffers = AabbTreeBuffers::create(_aabbTree, _allocator);

	// create g buffer pass
	GBuffer::Formats::initialize(_physicalDevice);
	// _gBufferPass = Pass::create<GBufferPass>(_device.get(), _swapchain.getImageExtent());
	_gBufferPass = Pass::create<GBufferPass>(_device.get(), &_sceneBuffers, _swapchain.getImageExtent());

	{
		_gBufferResources.uniformBuffer = _allocator.createTypedBuffer<GBufferPass::Uniforms>(
			1, vk::BufferUsageFlagBits::eUniformBuffer, VMA_MEMORY_USAGE_CPU_TO_GPU
			);

		std::array<vk::DescriptorSetLayout, 1> gBufferUniformLayout{ _gBufferPass.getUniformsDescriptorSetLayout() };
		vk::DescriptorSetAllocateInfo gBufferUniformAlloc;
		gBufferUniformAlloc
			.setDescriptorPool(_staticDescriptorPool.get())
			.setSetLayouts(gBufferUniformLayout);
		_gBufferResources.uniformDescriptor = std::move(_device->allocateDescriptorSetsUnique(gBufferUniformAlloc)[0]);

		std::array<vk::DescriptorSetLayout, 1> gBufferMatricesLayout{ _gBufferPass.getMatricesDescriptorSetLayout() };
		vk::DescriptorSetAllocateInfo gBufferMatricesAlloc;
		gBufferMatricesAlloc
			.setDescriptorPool(_staticDescriptorPool.get())
			.setSetLayouts(gBufferMatricesLayout);
		_gBufferResources.matrixDescriptor = std::move(_device->allocateDescriptorSetsUnique(gBufferMatricesAlloc)[0]);

		// Scene textures
		std::vector<vk::DescriptorSetLayout> gBufferTexturesLayout(_gltfScene.m_materials.size());
		std::fill(gBufferTexturesLayout.begin(), gBufferTexturesLayout.end(), _gBufferPass.getTexturesDescriptorSetLayout());
		vk::DescriptorSetAllocateInfo gBufferSceneTexturesAlloc;
		gBufferSceneTexturesAlloc
			.setDescriptorPool(_textureDescriptorPool.get())
			.setSetLayouts(gBufferTexturesLayout);
		_gBufferResources.materialTexturesDescriptors = _device->allocateDescriptorSetsUnique(gBufferSceneTexturesAlloc);
	}

	_gBufferPass.initializeResourcesFor(_gltfScene, _sceneBuffers, _device, _allocator, _gBufferResources);
	_gBufferPass.descriptorSets = &_gBufferResources;
	_gBufferPass.scene = &_gltfScene;
	_gBufferPass.sceneBuffers = &_sceneBuffers;

	_gBuffer = GBuffer::create(_allocator, _device.get(), _swapchain.getImageExtent(), _gBufferPass);

	/*_demoPass = Pass::create<DemoPass>(_device.get(), _swapchain.getImageFormat());
	_demoPass.imageExtent = _swapchain.getImageExtent();*/


	// create lighting pass
	_lightingPass = Pass::create<LightingPass>(_device.get(), _swapchain.getImageFormat());

	_initializeLightingPassResources();

	_lightingPass.imageExtent = _swapchain.getImageExtent();
	_lightingPass.descriptorSet = _lightingPassResources.descriptorSet.get();


	_createAndRecordGBufferCommandBuffer();
	_createAndRecordSwapchainBuffers(_lightingPass);


	// semaphores & fences
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

	{
		vk::FenceCreateInfo fenceInfo;
		fenceInfo
			.setFlags(vk::FenceCreateFlagBits::eSignaled);
		_gBufferFence = _device->createFenceUnique(fenceInfo);
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
			windowSize = newWindowSize;

			_swapchainInfo.setImageExtent(chooseSwapExtent(
				_physicalDevice.getSurfaceCapabilitiesKHR(_surface.get()), _window
			));


			_swapchainBuffers.clear();
			_swapchain.reset();

			_swapchain = Swapchain::create(_device.get(), _swapchainInfo);

			_gBuffer.resize(_allocator, _device.get(), _swapchain.getImageExtent(), _gBufferPass);
			_gBufferPass.onResized(_device.get(), _swapchain.getImageExtent());

			/*_demoPass.imageExtent = _swapchain.getImageExtent();*/

			_lightingPass.imageExtent = _swapchain.getImageExtent();
			_lightingPassResources = LightingPass::Resources();
			_initializeLightingPassResources();
			_lightingPass.descriptorSet = _lightingPassResources.descriptorSet.get();

			_gBufferCommandBuffer.reset();
			_createAndRecordGBufferCommandBuffer();

			_swapchainBuffers.clear();
			_createAndRecordSwapchainBuffers(_lightingPass);

			_camera.aspectRatio = _swapchain.getImageExtent().width / static_cast<float>(_swapchain.getImageExtent().height);
			_camera.recomputeAttributes();
			_cameraUpdated = true;
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
			_device->waitForFences(_gBufferFence.get(), true, std::numeric_limits<uint64_t>::max());
			_device->resetFences(_gBufferFence.get());

			if (_cameraUpdated) {
				_graphicsQueue.waitIdle();

				auto *gBufferUniforms = _gBufferResources.uniformBuffer.mapAs<GBufferPass::Uniforms>();
				gBufferUniforms->projectionViewMatrix = _camera.projectionViewMatrix;
				_gBufferResources.uniformBuffer.unmap();
				_gBufferResources.uniformBuffer.flush();

				auto *lightingPassUniforms = _lightingPassResources.uniformBuffer.mapAs<LightingPass::Uniforms>();
				lightingPassUniforms->inverseViewMatrix = _camera.inverseViewMatrix;
				lightingPassUniforms->tempLightPoint = _camera.lookAt;
				lightingPassUniforms->cameraNear = _camera.zNear;
				lightingPassUniforms->cameraFar = _camera.zFar;
				lightingPassUniforms->tanHalfFovY = std::tan(0.5f * _camera.fovYRadians);
				lightingPassUniforms->aspectRatio = _camera.aspectRatio;
				_lightingPassResources.uniformBuffer.unmap();
				_lightingPassResources.uniformBuffer.flush();

				_cameraUpdated = false;
			}

			std::array<vk::CommandBuffer, 1> gBufferCommandBuffers{ _gBufferCommandBuffer.get() };
			vk::SubmitInfo submitInfo;
			submitInfo
				.setCommandBuffers(gBufferCommandBuffers);
			_graphicsQueue.submit(submitInfo, _gBufferFence.get());
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
			_graphicsQueue.submit(submitInfo, _inFlightFences[currentFrame].get());
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

void App::_onMouseButtonEvent(int button, int action, int mods) {
	if (action == GLFW_PRESS) {
		if (_pressedMouseButton == -1) {
			_pressedMouseButton = button;
		}
	} else {
		if (button == _pressedMouseButton) {
			_pressedMouseButton = -1;
		}
	}
}

void App::_onMouseMoveEvent(double x, double y) {
	nvmath::vec2f newPos(static_cast<float>(x), static_cast<float>(y));
	nvmath::vec2f offset = newPos - _lastMouse;
	bool cameraChanged = true;
	switch (_pressedMouseButton) {
	case GLFW_MOUSE_BUTTON_LEFT:
		{
			nvmath::vec3f camOffset = _camera.position - _camera.lookAt;

			nvmath::vec3f y = nvmath::normalize(_camera.worldUp);
			nvmath::vec3f x = nvmath::normalize(nvmath::cross(y, camOffset));
			nvmath::vec3f z = nvmath::cross(x, y);

			nvmath::vec2f angles = offset * -1.0f * 0.005f;
			nvmath::vec2f vert(std::cos(angles.y), std::sin(angles.y));
			nvmath::vec2f hori(std::cos(angles.x), std::sin(angles.x));

			nvmath::vec3f angle(0.0f, nvmath::dot(camOffset, y), nvmath::dot(camOffset, z));
			float newZ = angle.y * vert.y + angle.z * vert.x;
			if (newZ > 0.0f) {
				angle = nvmath::vec3f(0.0f, angle.y * vert.x - angle.z * vert.y, newZ);
			}
			angle = nvmath::vec3f(angle.z * hori.y, angle.y, angle.z * hori.x);

			_camera.position = _camera.lookAt + x * angle.x + y * angle.y + z * angle.z;
		}
		break;
	case GLFW_MOUSE_BUTTON_RIGHT:
		_camera.fovYRadians = std::clamp(_camera.fovYRadians + 0.005f * offset.y, 0.01f, nv_pi - 0.01f);
		break;
	case GLFW_MOUSE_BUTTON_MIDDLE:
		{
			nvmath::vec3f offset3D = -offset.x * _camera.unitRight + offset.y * _camera.unitUp;
			offset3D *= 0.05f;
			_camera.position += offset3D;
			_camera.lookAt += offset3D;
		}
		break;
	default:
		cameraChanged = false;
		break;
	}
	if (cameraChanged) {
		_camera.recomputeAttributes();
		_cameraUpdated = true;
	}
	_lastMouse = newPos;
}

void App::_onScrollEvent(double x, double y) {
	_camera.position += _camera.unitForward * static_cast<float>(y) * 1.0f;
	_camera.recomputeAttributes();
	_cameraUpdated = true;
}
