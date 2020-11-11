#pragma once

#include <cassert>
#include <initializer_list>
#include <utility>
#include <vector>

// include vulkan first to enable certain glfw functions
#include <vulkan/vulkan.hpp>
#include <GLFW/glfw3.h>

class GlfwWindow {
public:
	GlfwWindow(std::initializer_list<std::pair<int, int>> hints);
	GlfwWindow(const GlfwWindow&) = delete;
	GlfwWindow(GlfwWindow &&src) : _window(src._window) {
		assert(&src != this);
		src._window = nullptr;
	}
	GlfwWindow &operator=(const GlfwWindow&) = delete;
	GlfwWindow &operator=(GlfwWindow &&src) {
		assert(&src != this);
		reset();
		_window = src._window;
		src._window = nullptr;
		return *this;
	}
	~GlfwWindow() {
		reset();
	}

	[[nodiscard]] vk::UniqueSurfaceKHR createSurface(const vk::UniqueInstance&);

	[[nodiscard]] vk::Extent2D getFramebufferSize() const;
	[[nodiscard]] bool shouldClose() const {
		return glfwWindowShouldClose(_window);
	}

	void reset() {
		if (_window) {
			glfwDestroyWindow(_window);
			_window = nullptr;
		}
	}

	[[nodiscard]] static std::vector<const char*> getRequiredInstanceExtensions();
protected:
	GLFWwindow *_window = nullptr;

private:
	struct LibraryReference {
		LibraryReference();
		~LibraryReference() {
			glfwTerminate();
		}
	};
	static void _maybeInitGlfw();
};
