#include <iostream>
#include <unordered_set>

#include <vulkan/vulkan.hpp>

// Gltf defines
#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define PROJPATH "C:\\JiaruiYan\\MasterDegreeProjects\\CIS565\\Proj6\\project6_project"

#include "glfwWindow.h"
#include "misc.h"
#include "swapchain.h"
#include "vma.h"
#include "sceneBuffers.h"
#include "passes/demoPass.h"
#include "passes/gBufferPass.h"

vk::SurfaceFormatKHR chooseSurfaceFormat(const vk::PhysicalDevice &dev, const vk::SurfaceKHR &surface) {
	std::vector<vk::SurfaceFormatKHR> available = dev.getSurfaceFormatsKHR(surface);
	vk::SurfaceFormatKHR desired(vk::Format::eB8G8R8A8Srgb, vk::ColorSpaceKHR::eSrgbNonlinear);
	if (std::find(available.begin(), available.end(), desired) != available.end()) {
		return desired;
	}
	return available[0];
}

vk::PresentModeKHR choosePresentMode(const vk::PhysicalDevice &dev, vk::SurfaceKHR surface) {
	std::vector<vk::PresentModeKHR> available = dev.getSurfacePresentModesKHR(surface);
	if (std::find(available.begin(), available.end(), vk::PresentModeKHR::eMailbox) != available.end()) {
		return vk::PresentModeKHR::eMailbox;
	}
	return vk::PresentModeKHR::eFifo;
}

vk::Extent2D chooseSwapExtent(
	const vk::SurfaceCapabilitiesKHR &capabilities, const GlfwWindow &wnd
) {
	if (capabilities.currentExtent.width != std::numeric_limits<std::uint32_t>::max()) {
		return capabilities.currentExtent;
	}
	vk::Extent2D result = wnd.getFramebufferSize();
	result.width = std::clamp(result.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
	result.height = std::clamp(result.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
	return result;
}


std::vector<Swapchain::BufferSet> createAndRecordSwapchainBuffers(
	const Swapchain &swapchain, vk::Device device, vk::CommandPool commandPool, Pass &pass
) {
	std::vector<Swapchain::BufferSet> swapchainBuffers = swapchain.getBuffers(device, pass.getPass(), commandPool);

	// record command buffers
	for (const Swapchain::BufferSet &bufferSet : swapchainBuffers) {
		vk::CommandBufferBeginInfo beginInfo;
		bufferSet.commandBuffer->begin(beginInfo);
		pass.issueCommands(bufferSet.commandBuffer.get(), bufferSet.framebuffer.get());
		bufferSet.commandBuffer->end();
	}

	return swapchainBuffers;
}


uint32_t chooseImageCount(const vk::SurfaceCapabilitiesKHR &capabilities) {
	uint32_t imageCount = capabilities.minImageCount + 1;
	if (capabilities.maxImageCount != 0) {
		imageCount = std::min(imageCount, capabilities.maxImageCount);
	}
	return imageCount;
}


int main() {
	constexpr uint32_t vulkanApiVersion = VK_MAKE_VERSION(1, 0, 0);

	// Load scene
	nvh::GltfScene m_gltfScene;
	// loadScene("../../../scenes/cornellBox.gltf", m_gltfScene);
	loadScene("../../../scenes/boxTextured/BoxTextured.gltf", m_gltfScene);


	std::vector<const char*> requiredExtensions = GlfwWindow::getRequiredInstanceExtensions();
	std::vector<const char*> requiredDeviceExtensions{
		"VK_KHR_swapchain"
	};
	std::vector<const char*> requiredLayers{
		"VK_LAYER_KHRONOS_validation"
	};

	{
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

	vk::UniqueInstance instance;
	{
		vk::ApplicationInfo appInfo;
		appInfo
			.setPApplicationName("ReSTIR")
			.setApiVersion(vulkanApiVersion);
		vk::InstanceCreateInfo instanceInfo;
		instanceInfo
			.setPApplicationInfo(&appInfo)
			.setPEnabledExtensionNames(requiredExtensions)
			.setPEnabledLayerNames(requiredLayers);
		instance = vk::createInstanceUnique(instanceInfo);
	}

	vk::PhysicalDevice physicalDevice;
	{ // pick physical device
		auto physicalDevices = instance->enumeratePhysicalDevices();
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
			physicalDevice = dev;
		}
	}
	if (!physicalDevice) {
		std::cout << "Failed to find suitable discrete gpu device\n";
		std::abort();
	}

	GlfwWindow window(
		{
			{ GLFW_CLIENT_API, GLFW_NO_API }
		}
	);

	vk::UniqueSurfaceKHR surface = window.createSurface(instance.get());

	vk::UniqueDevice device;
	uint32_t graphicsQueueIndex = 0;
	uint32_t presentQueueIndex = 0;
	{
		auto queueFamilyProps = physicalDevice.getQueueFamilyProperties();
		std::cout << "Queue families:\n";
		for (std::size_t i = 0; i < queueFamilyProps.size(); ++i) {
			const vk::QueueFamilyProperties &props = queueFamilyProps[i];
			std::cout << "    " << i << ": " << vk::to_string(props.queueFlags);
			if (props.queueFlags & vk::QueueFlagBits::eGraphics) {
				graphicsQueueIndex = static_cast<uint32_t>(i);
			}
			if (physicalDevice.getSurfaceSupportKHR(static_cast<uint32_t>(i), surface.get())) {
				presentQueueIndex = static_cast<uint32_t>(i);
				std::cout << " [Surface support]";
			}
			std::cout << "\n";
		}
		std::cout << "\n";
		graphicsQueueIndex = static_cast<uint32_t>(std::find_if(
			queueFamilyProps.begin(), queueFamilyProps.end(),
			[](const vk::QueueFamilyProperties &props) {
				return props.queueFlags & vk::QueueFlagBits::eGraphics;
			}
		) - queueFamilyProps.begin());

		std::vector<vk::DeviceQueueCreateInfo> queueInfos;
		std::vector<float> queuePriorities{ 1.0f };
		queueInfos.emplace_back()
			.setQueueFamilyIndex(graphicsQueueIndex)
			.setQueueCount(1)
			.setQueuePriorities(queuePriorities);
		queueInfos.emplace_back()
			.setQueueFamilyIndex(presentQueueIndex)
			.setQueueCount(1)
			.setQueuePriorities(queuePriorities);

		vk::PhysicalDeviceFeatures deviceFeatures;

		vk::DeviceCreateInfo deviceInfo;
		deviceInfo
			.setQueueCreateInfos(queueInfos)
			.setPEnabledFeatures(&deviceFeatures)
			.setPEnabledExtensionNames(requiredDeviceExtensions);
		device = physicalDevice.createDeviceUnique(deviceInfo);
	}


	vma::Allocator allocator = vma::Allocator::create(vulkanApiVersion, instance.get(), physicalDevice, device.get());

	vk::Queue graphicsQueue = device->getQueue(graphicsQueueIndex, 0);
	vk::Queue presentQueue = device->getQueue(presentQueueIndex, 0);

	// create command pool
	vk::UniqueCommandPool commandPool;
	{
		vk::CommandPoolCreateInfo poolInfo;
		poolInfo
			.setQueueFamilyIndex(graphicsQueueIndex)
			.setFlags(vk::CommandPoolCreateFlags());

		commandPool = device->createCommandPoolUnique(poolInfo);
	}

	Swapchain swapchain;
	vk::SwapchainCreateInfoKHR swapchainInfo;
	std::vector<uint32_t> swapchainSharedQueues{ graphicsQueueIndex, presentQueueIndex };
	{
		vk::SurfaceCapabilitiesKHR capabilities = physicalDevice.getSurfaceCapabilitiesKHR(surface.get());
		vk::SurfaceFormatKHR surfaceFormat = chooseSurfaceFormat(physicalDevice, surface.get());

		swapchainInfo
			.setSurface(surface.get())
			.setMinImageCount(chooseImageCount(capabilities))
			.setImageFormat(surfaceFormat.format)
			.setImageColorSpace(surfaceFormat.colorSpace)
			.setImageExtent(chooseSwapExtent(capabilities, window))
			.setPresentMode(choosePresentMode(physicalDevice, surface.get()))
			.setImageArrayLayers(1)
			.setPreTransform(capabilities.currentTransform)
			.setCompositeAlpha(vk::CompositeAlphaFlagBitsKHR::eOpaque)
			.setClipped(true)
			.setImageUsage(vk::ImageUsageFlagBits::eColorAttachment);

		if (graphicsQueueIndex == presentQueueIndex) {
			swapchainInfo.setImageSharingMode(vk::SharingMode::eExclusive);
		} else {
			swapchainInfo
				.setImageSharingMode(vk::SharingMode::eConcurrent)
				.setQueueFamilyIndices(swapchainSharedQueues);
		}

		swapchain = Swapchain::create(device.get(), swapchainInfo);
	}


	DemoPass pass = Pass::create<DemoPass>(device.get(), swapchain.getImageFormat());

	pass.imageExtent = swapchain.getImageExtent();
	std::vector<Swapchain::BufferSet> swapchainBuffers = createAndRecordSwapchainBuffers(
		swapchain, device.get(), commandPool.get(), pass
	);


	// semaphores
	const std::size_t maxFramesInFlight = 2;
	std::vector<vk::UniqueSemaphore> imageAvailableSemaphore(maxFramesInFlight);
	std::vector<vk::UniqueSemaphore> renderFinishedSemaphore(maxFramesInFlight);
	std::vector<vk::UniqueFence> inFlightFences(maxFramesInFlight);
	std::vector<vk::UniqueFence> inFlightImageFences(swapchainBuffers.size());
	std::size_t currentFrame = 0;
	for (std::size_t i = 0; i < maxFramesInFlight; ++i) {
		vk::SemaphoreCreateInfo semaphoreInfo;
		imageAvailableSemaphore[i] = device->createSemaphoreUnique(semaphoreInfo);
		renderFinishedSemaphore[i] = device->createSemaphoreUnique(semaphoreInfo);

		vk::FenceCreateInfo fenceInfo;
		fenceInfo
			.setFlags(vk::FenceCreateFlagBits::eSignaled);
		inFlightFences[i] = device->createFenceUnique(fenceInfo);
	}


	// main loop
	bool needsResize = false;
	vk::Extent2D windowSize = window.getFramebufferSize();
	while (!window.shouldClose()) {
		glfwPollEvents();

		vk::Extent2D newWindowSize = window.getFramebufferSize();
		if (needsResize || newWindowSize != windowSize) {
			device->waitIdle();

			while (newWindowSize.width == 0 && newWindowSize.height == 0) {
				glfwWaitEvents();
				newWindowSize = window.getFramebufferSize();
			}
			swapchainInfo.setImageExtent(chooseSwapExtent(physicalDevice.getSurfaceCapabilitiesKHR(surface.get()), window));

			swapchainBuffers.clear();
			swapchain.reset();
			swapchain = Swapchain::create(device.get(), swapchainInfo);
			pass.imageExtent = swapchain.getImageExtent();
			swapchainBuffers = createAndRecordSwapchainBuffers(swapchain, device.get(), commandPool.get(), pass);
		}

		device->waitForFences(
			{ inFlightFences[currentFrame].get() }, true, std::numeric_limits<std::uint64_t>::max()
		);

		auto [result, imageIndex] = device->acquireNextImageKHR(
			swapchain.getSwapchain().get(), std::numeric_limits<std::uint64_t>::max(),
			imageAvailableSemaphore[currentFrame].get(), nullptr
		);
		if (result == vk::Result::eErrorOutOfDateKHR || result == vk::Result::eSuboptimalKHR) {
			needsResize = true;
			continue;
		}

		if (inFlightImageFences[imageIndex]) {
			device->waitForFences(
				{ inFlightImageFences[imageIndex].get() }, true, std::numeric_limits<std::uint64_t>::max()
			);
		}

		std::vector<vk::Semaphore> waitSemaphores{ imageAvailableSemaphore[currentFrame].get() };
		std::vector<vk::PipelineStageFlags> waitStages{ vk::PipelineStageFlagBits::eColorAttachmentOutput };
		std::vector<vk::CommandBuffer> cmdBuffers{ swapchainBuffers[imageIndex].commandBuffer.get() };
		std::vector<vk::Semaphore> signalSemaphores{ renderFinishedSemaphore[currentFrame].get() };

		device->resetFences({ inFlightFences[currentFrame].get() });

		vk::SubmitInfo submitInfo;
		submitInfo
			.setWaitSemaphores(waitSemaphores)
			.setWaitDstStageMask(waitStages)
			.setCommandBuffers(cmdBuffers)
			.setSignalSemaphores(signalSemaphores);
		graphicsQueue.submit({ submitInfo }, inFlightFences[currentFrame].get());

		std::vector<vk::SwapchainKHR> swapchains{ swapchain.getSwapchain().get() };
		std::vector<std::uint32_t> imageIndices{ imageIndex };

		vk::PresentInfoKHR presentInfo;
		presentInfo
			.setWaitSemaphores(signalSemaphores)
			.setSwapchains(swapchains)
			.setImageIndices(imageIndices);
		try {
			presentQueue.presentKHR(presentInfo);
		} catch (const vk::OutOfDateKHRError&) {
			needsResize = true;
		}

		currentFrame = (currentFrame + 1) % maxFramesInFlight;
	}

	device->waitIdle();

	return 0;
}
