#include "glfwWindow.h"

#include <iostream>

namespace glfw {
	void _glfwCheck(bool returnValue) {
		if (returnValue != GLFW_TRUE) {
			const char *error = nullptr;
			glfwGetError(&error);
			std::cout << "GLFW error: " << error << "\n";
			std::abort();
		}
	}

	struct LibraryReference {
		LibraryReference() {
			_glfwCheck(glfwInit());
		}
		~LibraryReference() {
			glfwTerminate();
		}
	};

	void _maybeInitGlfw() {
		static LibraryReference _lib;
	}


	std::vector<const char*> getRequiredInstanceExtensions() {
		_maybeInitGlfw();

		uint32_t count = 0;
		const char **extensions = glfwGetRequiredInstanceExtensions(&count);
		_glfwCheck(extensions);

		return std::vector<const char*>(extensions, extensions + count);
	}


	Window::Window(std::initializer_list<std::pair<int, int>> hints) {
		_maybeInitGlfw();

		for (auto [key, value] : hints) {
			glfwWindowHint(key, value);
		}
		_window = glfwCreateWindow(800, 600, "Vulkan", nullptr, nullptr);
		glfwSetWindowUserPointer(_window, this);
		_glfwCheck(_window);
	}

	vk::UniqueSurfaceKHR Window::createSurface(vk::Instance instance) {
		VkSurfaceKHR surface;
		if (VkResult res = glfwCreateWindowSurface(instance, _window, nullptr, &surface); res != VK_SUCCESS) {
			std::cout << "Failed to create GLFW window surface: " << vk::to_string(static_cast<vk::Result>(res)) << "\n";
			std::abort();
		}
		return vk::UniqueSurfaceKHR(surface, instance);
	}

	vk::Extent2D Window::getFramebufferSize() const {
		int width, height;
		glfwGetFramebufferSize(_window, &width, &height);
		return vk::Extent2D(width, height);
	}

	nvmath::vec2f Window::getSavedCursorPosition() const {
		double x;
		double y;
		glfwGetCursorPos(_window, &x, &y);
		return nvmath::vec2f(static_cast<float>(x), static_cast<float>(y));
	}
}
