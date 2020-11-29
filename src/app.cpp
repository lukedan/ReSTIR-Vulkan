#include "app.h"

/*#define RENDERDOC_CAPTURE*/

#include <sstream>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>

VKAPI_ATTR VkBool32 VKAPI_CALL _debugCallback(
	VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
	VkDebugUtilsMessageTypeFlagsEXT messageType,
	const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
	void*
) {
	if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
		std::cerr << pCallbackData->pMessage << std::endl;
#ifdef _MSC_VER
		__debugbreak();
#endif
	}
	return VK_FALSE;
}

void App::initPhysicalInfo(PhysicalDeviceInfo& info, vk::PhysicalDevice physicalDevice) {
	vk::PhysicalDeviceFeatures2   features2;
	vk::PhysicalDeviceProperties2 properties2;

	info.memoryProperties = physicalDevice.getMemoryProperties();
	info.queueProperties = physicalDevice.getQueueFamilyProperties();

	features2.pNext = &info.features11;
	info.features11.pNext = &info.features12;
	info.features12.pNext = nullptr;

	info.properties12.driverID = vk::DriverId::eNvidiaProprietary;
	info.properties12.supportedDepthResolveModes = vk::ResolveModeFlagBits::eMax;
	info.properties12.supportedStencilResolveModes = vk::ResolveModeFlagBits::eMax;

	properties2.pNext = &info.properties11;
	info.properties11.pNext = &info.properties12;
	info.properties12.pNext = nullptr;

	physicalDevice.getFeatures2(&features2);
	physicalDevice.getProperties2(&properties2);

	info.properties10 = properties2.properties;
	info.features10 = features2.features;
}

App::App() : _window({ { GLFW_CLIENT_API, GLFW_NO_API } }) {
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	// the callbacks are installed here but they're overriden below, so we still need to manually call those
	// functions in the handlers
	ImGui_ImplGlfw_InitForVulkan(_window.getRawHandle(), true);
	// imgui-vulkan is initialized later with the queue & render pass

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
	requiredExtensions.emplace_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
	std::vector<const char*> requiredDeviceExtensions{
		VK_KHR_SWAPCHAIN_EXTENSION_NAME,
#ifndef RENDERDOC_CAPTURE
		VK_KHR_PIPELINE_LIBRARY_EXTENSION_NAME,
		VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME
#endif
	};
	std::vector<const char*> requiredLayers{
		"VK_LAYER_KHRONOS_validation"
	};

	vk::PhysicalDeviceRayTracingFeaturesKHR raytracingFeature;
	raytracingFeature.setRayTracing(true);
	std::vector<const char*> requiredDeviceRayTracingExtensions{
#ifndef RENDERDOC_CAPTURE
		VK_KHR_RAY_TRACING_EXTENSION_NAME
#endif
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

	_dynamicDispatcher.init(_instance.get());
	std::vector<void*> featureStructs; // VKRay
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

			// #VKRay Extension Checking
			bool supportsRTExtensions = checkSupport<&vk::ExtensionProperties::extensionName>(
				requiredDeviceRayTracingExtensions, dev.enumerateDeviceExtensionProperties(),
				"device extensions", "    "
				);
			if (supportsRTExtensions) {
				featureStructs.emplace_back(&raytracingFeature);
				requiredDeviceExtensions.insert(
					requiredDeviceExtensions.end(),
					requiredDeviceRayTracingExtensions.begin(), requiredDeviceRayTracingExtensions.end()
				);
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
			if (props.queueFlags & (vk::QueueFlagBits::eGraphics | vk::QueueFlagBits::eCompute)) {
				_graphicsComputeQueueIndex = static_cast<uint32_t>(i);
			}
			if (_physicalDevice.getSurfaceSupportKHR(static_cast<uint32_t>(i), _surface.get())) {
				_presentQueueIndex = static_cast<uint32_t>(i);
				std::cout << " [Surface support]";
			}
			std::cout << "\n";
		}
		std::cout << "\n";
		_graphicsComputeQueueIndex = static_cast<uint32_t>(std::find_if(
			queueFamilyProps.begin(), queueFamilyProps.end(),
			[](const vk::QueueFamilyProperties &props) {
				return props.queueFlags & vk::QueueFlagBits::eGraphics;
			}
		) - queueFamilyProps.begin());

		// Setup Vulkan 1.2 Physical Device Info
		PhysicalDeviceInfo physicalDeviceInfo;
		initPhysicalInfo(physicalDeviceInfo, _physicalDevice);
		vk::PhysicalDeviceFeatures2 features10;
		vk::PhysicalDeviceVulkan11Features features11;
		vk::PhysicalDeviceVulkan12Features features12;
		features10.features
			.setSamplerAnisotropy(true)
			.setShaderInt64(true);
		features12
			.setBufferDeviceAddress(true);
		features10.pNext = &features11;
		features11.pNext = &features12;

		struct ExtensionHeader { // Helper struct to link extensions together
			vk::StructureType sType;
			void* pNext;
		};

		// Use the feature2 chain to append extensions
		if (!featureStructs.empty()) {
			for (size_t i = 0; i < featureStructs.size(); i++) {
				auto* header = reinterpret_cast<ExtensionHeader*>(featureStructs[i]);
				header->pNext = i < featureStructs.size() - 1 ? featureStructs[i + 1] : nullptr;
			}

			ExtensionHeader* lastCoreFeature = (ExtensionHeader*)&features12;
			while (lastCoreFeature->pNext != nullptr) {
				lastCoreFeature = (ExtensionHeader*)lastCoreFeature->pNext;
			}
			lastCoreFeature->pNext = featureStructs[0];
		}


		std::array<float, 1> queuePriorities{ 1.0f };
		std::vector<vk::DeviceQueueCreateInfo> queueInfos{
			vk::DeviceQueueCreateInfo({}, _graphicsComputeQueueIndex, queuePriorities),
			vk::DeviceQueueCreateInfo({}, _presentQueueIndex, queuePriorities)
		};

		vk::DeviceCreateInfo deviceInfo;
		deviceInfo
			.setQueueCreateInfos(queueInfos)
			.setPEnabledExtensionNames(requiredDeviceExtensions)
			.setPNext(&features10);
		_device = _physicalDevice.createDeviceUnique(deviceInfo);
		_dynamicDispatcher.init(_device.get());
	}


	_allocator = vma::Allocator::create(vulkanApiVersion, _instance.get(), _physicalDevice, _device.get());

	// create command pools
	{
		vk::CommandPoolCreateInfo poolInfo;
		poolInfo
			.setQueueFamilyIndex(_graphicsComputeQueueIndex);
		_commandPool = _device->createCommandPoolUnique(poolInfo);

		vk::CommandPoolCreateInfo imguiPoolInfo;
		imguiPoolInfo
			.setQueueFamilyIndex(_graphicsComputeQueueIndex)
			.setFlags(vk::CommandPoolCreateFlagBits::eResetCommandBuffer);
		_imguiCommandPool = _device->createCommandPoolUnique(imguiPoolInfo);
	}
	_transientCommandBufferPool = TransientCommandBufferPool(_device.get(), _graphicsComputeQueueIndex);

	{
		_swapchainSharedQueues = { _graphicsComputeQueueIndex, _presentQueueIndex };
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
			.setImageUsage(vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferDst);

		if (_graphicsComputeQueueIndex == _presentQueueIndex) {
			_swapchainInfo.setImageSharingMode(vk::SharingMode::eExclusive);
		} else {
			_swapchainInfo
				.setImageSharingMode(vk::SharingMode::eConcurrent)
				.setQueueFamilyIndices(_swapchainSharedQueues);
		}

		_swapchain = Swapchain::create(_device.get(), _swapchainInfo);
	}

	/** NOTE: A scene without emissive materials will be given 8 point lights and scenes with lightning material won't have point lights **/
	/** cornellBox has emissive materials and others don't have **/
	/*loadScene("../../../scenes/cornellBox/cornellBox.gltf", _gltfScene);*/
	// loadScene("../../../scenes/boxTextured/boxTextured.gltf", _gltfScene);
	// loadScene("../../../scenes/duck/Duck.gltf", _gltfScene);
	// loadScene("../../../scenes/fish/BarramundiFish.gltf", _gltfScene);
	loadScene("../../../scenes/Sponza/glTF/Sponza.gltf", _gltfScene);

	{ // create descriptor pools
		std::array<vk::DescriptorPoolSize, 6> staticPoolSizes{
			vk::DescriptorPoolSize(vk::DescriptorType::eCombinedImageSampler, 100),
			vk::DescriptorPoolSize(vk::DescriptorType::eStorageBuffer, 100),
			vk::DescriptorPoolSize(vk::DescriptorType::eUniformBuffer, 100),
			vk::DescriptorPoolSize(vk::DescriptorType::eUniformBufferDynamic, 100),
			vk::DescriptorPoolSize(vk::DescriptorType::eAccelerationStructureKHR, 100),
			vk::DescriptorPoolSize(vk::DescriptorType::eStorageImage, 100)
		};
		vk::DescriptorPoolCreateInfo staticPoolInfo;
		staticPoolInfo
			.setFlags(vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet)
			.setPoolSizes(staticPoolSizes)
			.setMaxSets(100);
		_staticDescriptorPool = _device->createDescriptorPoolUnique(staticPoolInfo);

		std::array<vk::DescriptorPoolSize, 1> texturePoolSizes{
			vk::DescriptorPoolSize(vk::DescriptorType::eCombinedImageSampler, static_cast<uint32_t>(3 * _gltfScene.m_materials.size()))
		};
		vk::DescriptorPoolCreateInfo texturePoolInfo;
		texturePoolInfo
			.setFlags(vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet)
			.setPoolSizes(texturePoolSizes)
			.setMaxSets(static_cast<uint32_t>(_gltfScene.m_materials.size()));
		_textureDescriptorPool = _device->createDescriptorPoolUnique(texturePoolInfo);

		// initialize imgui descriptor pool
		// this is taken from official imgui example at https://github.com/ocornut/imgui/blob/master/examples/example_glfw_vulkan/main.cpp
		// which is wayyyy overkill, but fuck it - we're not low on memory here
		constexpr uint32_t imguiDescriptorCount = 1000;
		std::array<vk::DescriptorPoolSize, 11> imguiPoolSizes{
			vk::DescriptorPoolSize(vk::DescriptorType::eSampler, imguiDescriptorCount),
			vk::DescriptorPoolSize(vk::DescriptorType::eCombinedImageSampler, imguiDescriptorCount),
			vk::DescriptorPoolSize(vk::DescriptorType::eSampledImage, imguiDescriptorCount),
			vk::DescriptorPoolSize(vk::DescriptorType::eStorageImage, imguiDescriptorCount),
			vk::DescriptorPoolSize(vk::DescriptorType::eUniformTexelBuffer, imguiDescriptorCount),
			vk::DescriptorPoolSize(vk::DescriptorType::eStorageTexelBuffer, imguiDescriptorCount),
			vk::DescriptorPoolSize(vk::DescriptorType::eUniformBuffer, imguiDescriptorCount),
			vk::DescriptorPoolSize(vk::DescriptorType::eStorageBuffer, imguiDescriptorCount),
			vk::DescriptorPoolSize(vk::DescriptorType::eUniformBufferDynamic, imguiDescriptorCount),
			vk::DescriptorPoolSize(vk::DescriptorType::eStorageBufferDynamic, imguiDescriptorCount),
			vk::DescriptorPoolSize(vk::DescriptorType::eInputAttachment, imguiDescriptorCount)
		};
		vk::DescriptorPoolCreateInfo imguiPoolInfo;
		imguiPoolInfo
			.setFlags(vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet)
			.setMaxSets(static_cast<uint32_t>(imguiPoolSizes.size() * imguiDescriptorCount))
			.setPoolSizes(imguiPoolSizes);
		_imguiDescriptorPool = _device->createDescriptorPoolUnique(imguiPoolInfo);
	}


	_graphicsComputeQueue = _device->getQueue(_graphicsComputeQueueIndex, 0);
	_presentQueue = _device->getQueue(_presentQueueIndex, 0);


	_sceneBuffers = SceneBuffers::create(
		_gltfScene,
		_allocator, _transientCommandBufferPool,
		_device.get(), _graphicsComputeQueue
	);
	std::cout << "Building AABB tree...";
	_aabbTree = AabbTree::build(_gltfScene);
	std::cout << " done\n";
	_aabbTreeBuffers = AabbTreeBuffers::create(_aabbTree, _allocator);


	// create g buffer pass
	GBuffer::Formats::initialize(_physicalDevice);
	_gBufferPass = Pass::create<GBufferPass>(_device.get(), _swapchain.getImageExtent());

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

		std::array<vk::DescriptorSetLayout, 1> gBufferMaterialsLayout{ _gBufferPass.getMaterialDescriptorSetLayout() };
		vk::DescriptorSetAllocateInfo gBufferMaterialsAlloc;
		gBufferMaterialsAlloc
			.setDescriptorPool(_staticDescriptorPool.get())
			.setSetLayouts(gBufferMaterialsLayout);
		_gBufferResources.materialDescriptor = std::move(_device->allocateDescriptorSetsUnique(gBufferMaterialsAlloc)[0]);

		// Scene textures
		std::vector<vk::DescriptorSetLayout> gBufferTexturesLayout(_gltfScene.m_materials.size());
		std::fill(gBufferTexturesLayout.begin(), gBufferTexturesLayout.end(), _gBufferPass.getTexturesDescriptorSetLayout());
		vk::DescriptorSetAllocateInfo gBufferSceneTexturesAlloc;
		gBufferSceneTexturesAlloc
			.setDescriptorPool(_textureDescriptorPool.get())
			.setSetLayouts(gBufferTexturesLayout);
		_gBufferResources.materialTexturesDescriptors = _device->allocateDescriptorSetsUnique(gBufferSceneTexturesAlloc);
	}
	_gBufferPass.initializeResourcesFor(_gltfScene, _sceneBuffers, _device, _gBufferResources);

	_gBufferPass.descriptorSets = &_gBufferResources;
	_gBufferPass.scene = &_gltfScene;
	_gBufferPass.sceneBuffers = &_sceneBuffers;

	_gBuffer = GBuffer::create(_allocator, _device.get(), _swapchain.getImageExtent(), _gBufferPass);


	_restirUniformBuffer = _allocator.createTypedBuffer<shader::RestirUniforms>(
		1, vk::BufferUsageFlagBits::eUniformBuffer, VMA_MEMORY_USAGE_CPU_TO_GPU
		);
	{
		auto *restirUniforms = _restirUniformBuffer.mapAs<shader::RestirUniforms>();
		restirUniforms->screenSize = nvmath::uvec2(_swapchain.getImageExtent().width, _swapchain.getImageExtent().height);
		restirUniforms->frame = 0;
		_restirUniformBuffer.unmap();
		_restirUniformBuffer.flush();
	}


	_lightSamplePass = Pass::create<LightSamplePass>(_device.get());
	{
		std::array<vk::DescriptorSetLayout, 1> setLayouts{ _lightSamplePass.getDescriptorSetLayout() };
		vk::DescriptorSetAllocateInfo allocInfo;
		allocInfo
			.setDescriptorPool(_staticDescriptorPool.get())
			.setSetLayouts(setLayouts);
		_lightSampleDescriptors = std::move(_device->allocateDescriptorSetsUnique(allocInfo)[0]);
	}
	_lightSamplePass.descriptorSet = _lightSampleDescriptors.get();
	_lightSamplePass.screenSize = _swapchain.getImageExtent();


	_swVisibilityTestPass = Pass::create<SoftwareVisibilityTestPass>(_device.get());
	{
		std::array<vk::DescriptorSetLayout, 1> setLayouts{ _swVisibilityTestPass.getDescriptorSetLayout() };
		vk::DescriptorSetAllocateInfo allocInfo;
		allocInfo
			.setDescriptorPool(_staticDescriptorPool.get())
			.setSetLayouts(setLayouts);
		_swVisibilityTestDescriptors = std::move(_device->allocateDescriptorSetsUnique(allocInfo)[0]);
	}
	_swVisibilityTestPass.descriptorSet = _swVisibilityTestDescriptors.get();
	_swVisibilityTestPass.screenSize = _swapchain.getImageExtent();

	_spatialReusePass = Pass::create<SpatialReusePass>(_device.get());
	{
		std::array<vk::DescriptorSetLayout, 1> setLayouts{ _spatialReusePass.getDescriptorSetLayout() };
		vk::DescriptorSetAllocateInfo allocInfo;
		allocInfo
			.setDescriptorPool(_staticDescriptorPool.get())
			.setSetLayouts(setLayouts);
		_spatialReuseDescriptors = std::move(_device->allocateDescriptorSetsUnique(allocInfo)[0]);
	}
	_spatialReusePass.descriptorSet = _spatialReuseDescriptors.get();
	_spatialReusePass.screenSize = _swapchain.getImageExtent();

	// Hardware RT pass for visibility test
	_rtPass = RtPass::create(_device.get(), _dynamicDispatcher);
	_rtPass._gBuffer = &_gBuffer;
	_rtPass.createAccelerationStructure(_device.get(), _physicalDevice, _allocator, _dynamicDispatcher,
		_commandPool.get(), _graphicsComputeQueue, _sceneBuffers, _gltfScene);
	_rtPass.createShaderBindingTable(_device.get(), _allocator, _physicalDevice, _dynamicDispatcher);


	_updateRestirBuffers();


	// create lighting pass
	_lightingPass = Pass::create<LightingPass>(_device.get(), _swapchain.getImageFormat());
	_initializeLightingPassResources();

	_lightingPass.imageExtent = _swapchain.getImageExtent();
	_lightingPass.descriptorSet = _lightingPassResources.descriptorSet.get();
	_createAndRecordSwapchainBuffers();


	_imguiPass = Pass::create<ImGuiPass>(_device.get(), _swapchain.getImageFormat());
	_imguiPass.imageExtent = _swapchain.getImageExtent();

	// finish initializing imgui
	{
		ImGui_ImplVulkan_InitInfo imguiInit{};
		imguiInit.Instance = _instance.get();
		imguiInit.PhysicalDevice = _physicalDevice;
		imguiInit.Device = _device.get();
		imguiInit.QueueFamily = _graphicsComputeQueueIndex;
		imguiInit.Queue = _graphicsComputeQueue;
		imguiInit.DescriptorPool = _imguiDescriptorPool.get();
		imguiInit.MinImageCount = _swapchainInfo.minImageCount;
		imguiInit.ImageCount = static_cast<uint32_t>(_swapchain.getImages().size());
		ImGui_ImplVulkan_Init(&imguiInit, _imguiPass.getPass());
	}
	{
		TransientCommandBuffer cmdBuffer = _transientCommandBufferPool.begin(_graphicsComputeQueue);
		ImGui_ImplVulkan_CreateFontsTexture(cmdBuffer.get());
	}

	_createAndRecordMainCommandBuffer();
	_createImguiCommandBuffers();


	// semaphores & fences
	_imageAvailableSemaphore.resize(maxFramesInFlight);
	_renderFinishedSemaphore.resize(maxFramesInFlight);
	_inFlightFences.resize(maxFramesInFlight);
	_inFlightImageFences.resize(_swapchainBuffers.size());
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
		_mainFence = _device->createFenceUnique(fenceInfo);
	}
}

App::~App() {
	_device->waitIdle();

	ImGui_ImplVulkan_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
	_rtPass.freeDeviceMemory(_device.get());
}

void App::updateGui() {
	ImGui_ImplVulkan_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();

	const char *items[]{
		"None",
		"Albedo",
		"Normal",
		"MaterialProperties",
		"WorldPosition",
		"DisneyBRDF"
	};
	_debugModeChanged = ImGui::Combo("Debug Mode", &_debugMode, items, IM_ARRAYSIZE(items));

	_useHardwareRtChanged = ImGui::Checkbox("Use Hardware Ray Tracing", &_useHardwareRt);
	_useSpatialReuseChanged = ImGui::Checkbox("Use Spatial Resuing", &_useSpatialReuse);

	ImGui::Render();
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

			auto *restirUniforms = _restirUniformBuffer.mapAs<shader::RestirUniforms>();
			restirUniforms->screenSize = nvmath::uvec2(windowSize.width, windowSize.height);
			restirUniforms->frame = 0;
			_restirUniformBuffer.unmap();
			_restirUniformBuffer.flush();

			_lightSamplePass.screenSize = windowSize;

			_swVisibilityTestPass.screenSize = windowSize;

			_spatialReusePass.screenSize = windowSize;

			_updateRestirBuffers();

			_initializeLightingPassResources();

			_lightingPass.imageExtent = _swapchain.getImageExtent();
			_lightingPass.descriptorSet = _lightingPassResources.descriptorSet.get();

			_imguiPass.imageExtent = _swapchain.getImageExtent();

			_createAndRecordMainCommandBuffer();
			_createAndRecordSwapchainBuffers();

			_camera.aspectRatio = _swapchain.getImageExtent().width / static_cast<float>(_swapchain.getImageExtent().height);
			_camera.recomputeAttributes();
			_cameraUpdated = true;
		}

		while (_device->waitForFences(
			{ _inFlightFences[currentFrame].get() }, true, std::numeric_limits<std::uint64_t>::max()
		) == vk::Result::eTimeout) {
		}

		_fpsCounter.tick();
		std::stringstream ss;
		ss <<
			"ReSTIR | FPS: " <<
			std::fixed << std::setprecision(2) << _fpsCounter.getFpsAverageWindow() << " (Window: " << _fpsCounter.timeWindow << "s)  " <<
			std::fixed << std::setprecision(2) << _fpsCounter.getFpsRunningAverage() << " (RA: " << _fpsCounter.alpha << ")";
		_window.setTitle(ss.str());

		auto [result, imageIndex] = _device->acquireNextImageKHR(
			_swapchain.getSwapchain().get(), std::numeric_limits<std::uint64_t>::max(),
			_imageAvailableSemaphore[currentFrame].get(), nullptr
		);

		if (result == vk::Result::eErrorOutOfDateKHR || result == vk::Result::eSuboptimalKHR) {
			needsResize = true;
			continue;
		}

		if (_inFlightImageFences[imageIndex]) {
			while (_device->waitForFences(
				{ _inFlightImageFences[imageIndex].get() }, true, std::numeric_limits<std::uint64_t>::max()
			) == vk::Result::eTimeout) {
			}
		}

		auto* lightingPassUniforms = _lightingPassResources.uniformBuffer.mapAs<shader::LightingPassUniforms>();
		std::clock_t app_time_now = std::clock();
		_lightingPassResources.uniformBuffer.unmap();
		_lightingPassResources.uniformBuffer.flush();

		_device->resetFences({ _inFlightFences[currentFrame].get() });

		{
			while (_device->waitForFences(_mainFence.get(), true, std::numeric_limits<uint64_t>::max()) == vk::Result::eTimeout) {
			}
			_device->resetFences(_mainFence.get());

			auto *restirUniforms = _restirUniformBuffer.mapAs<shader::RestirUniforms>();
			++restirUniforms->frame;

			if (_cameraUpdated || _debugModeChanged) {
				_graphicsComputeQueue.waitIdle();

				auto *gBufferUniforms = _gBufferResources.uniformBuffer.mapAs<GBufferPass::Uniforms>();
				gBufferUniforms->projectionViewMatrix = _camera.projectionViewMatrix;
				_gBufferResources.uniformBuffer.unmap();
				_gBufferResources.uniformBuffer.flush();

				restirUniforms->cameraPos = _camera.position;

				auto *lightingPassUniforms = _lightingPassResources.uniformBuffer.mapAs<shader::LightingPassUniforms>();
				lightingPassUniforms->cameraPos = _camera.position;
				lightingPassUniforms->bufferSize = nvmath::uvec2(_swapchain.getImageExtent().width, _swapchain.getImageExtent().height);
				lightingPassUniforms->debugMode = _debugMode;
				_lightingPassResources.uniformBuffer.unmap();
				_lightingPassResources.uniformBuffer.flush();

				_cameraUpdated = false;
				_debugModeChanged = false;
			} else if (_useHardwareRtChanged) {
				_createAndRecordMainCommandBuffer();
			} else if (_useSpatialReuseChanged) {
				_createAndRecordMainCommandBuffer();
				_updateLightingPassDescriptor();
			}

			_restirUniformBuffer.unmap();
			_restirUniformBuffer.flush();

			std::array<vk::CommandBuffer, 1> gBufferCommandBuffers{ _mainCommandBuffer.get() };
			vk::SubmitInfo submitInfo;
			submitInfo
				.setCommandBuffers(gBufferCommandBuffers);
			_graphicsComputeQueue.submit(submitInfo, _mainFence.get());
		}

		updateGui();
		{ // re-record imgui command buffer
			vk::CommandBuffer buffer = _imguiCommandBuffers[imageIndex].get();
			vk::CommandBufferBeginInfo beginInfo;
			buffer.begin(beginInfo);

			_imguiPass.issueCommands(buffer, _swapchainBuffers[imageIndex].framebuffer.get());

			buffer.end();
		}

		std::array<vk::Semaphore, 1> signalSemaphores{ _renderFinishedSemaphore[currentFrame].get() };
		{
			std::array<vk::Semaphore, 1> waitSemaphores{ _imageAvailableSemaphore[currentFrame].get() };
			std::array<vk::PipelineStageFlags, 1> waitStages{ vk::PipelineStageFlagBits::eColorAttachmentOutput };
			std::array<vk::CommandBuffer, 2> cmdBuffers{
				_swapchainBuffers[imageIndex].commandBuffer.get(),
				_imguiCommandBuffers[imageIndex].get()
			};
			vk::SubmitInfo submitInfo;
			submitInfo
				.setWaitSemaphores(waitSemaphores)
				.setWaitDstStageMask(waitStages)
				.setCommandBuffers(cmdBuffers)
				.setSignalSemaphores(signalSemaphores);
			_graphicsComputeQueue.submit(submitInfo, _inFlightFences[currentFrame].get());
		}

		std::vector<vk::SwapchainKHR> swapchains{ _swapchain.getSwapchain().get() };
		std::vector<std::uint32_t> imageIndices{ imageIndex };

		vk::PresentInfoKHR presentInfo;
		presentInfo
			.setWaitSemaphores(signalSemaphores)
			.setSwapchains(swapchains)
			.setImageIndices(imageIndices);
		try {
			if (_presentQueue.presentKHR(presentInfo) == vk::Result::eSuboptimalKHR) {
				needsResize = true;
			}
		} catch (const vk::OutOfDateKHRError&) {
			needsResize = true;
		}

		currentFrame = (currentFrame + 1) % maxFramesInFlight;
	}
}

void App::_onMouseButtonEvent(int button, int action, int mods) {
	if (ImGui::GetIO().WantCaptureMouse) {
		ImGui_ImplGlfw_MouseButtonCallback(_window.getRawHandle(), button, action, mods);
		return;
	}

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

			nvmath::vec3f yAxis = nvmath::normalize(_camera.worldUp);
			nvmath::vec3f xAxis = nvmath::normalize(nvmath::cross(yAxis, camOffset));
			nvmath::vec3f zAxis = nvmath::cross(xAxis, yAxis);

			nvmath::vec2f angles = offset * -1.0f * 0.005f;
			nvmath::vec2f vert(std::cos(angles.y), std::sin(angles.y));
			nvmath::vec2f hori(std::cos(angles.x), std::sin(angles.x));

			nvmath::vec3f angle(0.0f, nvmath::dot(camOffset, yAxis), nvmath::dot(camOffset, zAxis));
			float newZ = angle.y * vert.y + angle.z * vert.x;
			if (newZ > 0.0f) {
				angle = nvmath::vec3f(0.0f, angle.y * vert.x - angle.z * vert.y, newZ);
			}
			angle = nvmath::vec3f(angle.z * hori.y, angle.y, angle.z * hori.x);

			_camera.position = _camera.lookAt + xAxis * angle.x + yAxis * angle.y + zAxis * angle.z;
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
	if (ImGui::GetIO().WantCaptureMouse) {
		ImGui_ImplGlfw_ScrollCallback(_window.getRawHandle(), x, y);
		return;
	}

	_camera.position += _camera.unitForward * static_cast<float>(y) * 1.0f;
	_camera.recomputeAttributes();
	_cameraUpdated = true;
}
