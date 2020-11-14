#include "glfwWindow.h"

#include <iostream>

void _glfwCheck(bool returnValue) {
	if (returnValue != GLFW_TRUE) {
		const char *error = nullptr;
		glfwGetError(&error);
		std::cout << "GLFW error: " << error << "\n";
		std::abort();
	}
}


GlfwWindow::GlfwWindow(std::initializer_list<std::pair<int, int>> hints) {
	_maybeInitGlfw();

	for (auto [key, value] : hints) {
		glfwWindowHint(key, value);
	}
	_window = glfwCreateWindow(800, 600, "Vulkan", nullptr, nullptr);
	_glfwCheck(_window);
}

vk::UniqueSurfaceKHR GlfwWindow::createSurface(vk::Instance instance) {
	VkSurfaceKHR surface;
	if (VkResult res = glfwCreateWindowSurface(instance, _window, nullptr, &surface); res != VK_SUCCESS) {
		std::cout << "Failed to create GLFW window surface: " << vk::to_string(static_cast<vk::Result>(res)) << "\n";
		std::abort();
	}
	return vk::UniqueSurfaceKHR(surface, vk::ObjectDestroy<vk::Instance, VULKAN_HPP_DEFAULT_DISPATCHER_TYPE>(instance));
}

vk::Extent2D GlfwWindow::getFramebufferSize() const {
	int width, height;
	glfwGetFramebufferSize(_window, &width, &height);
	return vk::Extent2D(width, height);
}

std::vector<const char*> GlfwWindow::getRequiredInstanceExtensions() {
	_maybeInitGlfw();

	uint32_t count = 0;
	const char **extensions = glfwGetRequiredInstanceExtensions(&count);
	_glfwCheck(extensions);

	return std::vector<const char*>(extensions, extensions + count);
}

void GlfwWindow::_maybeInitGlfw() {
	static LibraryReference _lib;
}


GlfwWindow::LibraryReference::LibraryReference() {
	_glfwCheck(glfwInit());
}
