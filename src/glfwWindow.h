#pragma once

#include <cassert>
#include <initializer_list>
#include <utility>
#include <vector>

// include vulkan first to enable certain glfw functions
#include <vulkan/vulkan.hpp>
#include <GLFW/glfw3.h>
#include <nvmath.h>

namespace glfw {
	[[nodiscard]] std::vector<const char*> getRequiredInstanceExtensions();

	class Window {
	public:
		using CursorPosCallback = std::function<void(double, double)>;
		using MouseButtonCallback = std::function<void(int button, int action, int mods)>;
		using ScrollCallback = std::function<void(double, double)>;

		Window(std::initializer_list<std::pair<int, int>> hints);
		Window(const Window&) = delete;
		Window(Window &&src) : _window(src._window) {
			assert(&src != this);
			src._window = nullptr;
		}
		Window &operator=(const Window&) = delete;
		Window &operator=(Window &&src) {
			assert(&src != this);
			reset();
			_window = src._window;
			src._window = nullptr;
			return *this;
		}
		~Window() {
			reset();
		}

		void setCursorPosHandler(CursorPosCallback cb) {
			_setEventHandler<&Window::_cursorPosCallback>(std::move(cb), glfwSetCursorPosCallback);
		}
		void setMouseButtonHandler(MouseButtonCallback cb) {
			_setEventHandler<&Window::_mouseButtonCallback>(std::move(cb), glfwSetMouseButtonCallback);
		}
		void setScrollHandler(ScrollCallback cb) {
			_setEventHandler<&Window::_scrollCallback>(std::move(cb), glfwSetScrollCallback);
		}

		[[nodiscard]] vk::UniqueSurfaceKHR createSurface(vk::Instance);

		[[nodiscard]] vk::Extent2D getFramebufferSize() const;
		[[nodiscard]] nvmath::vec2f getSavedCursorPosition() const;

		[[nodiscard]] bool shouldClose() const {
			return glfwWindowShouldClose(_window);
		}

		void reset() {
			if (_window) {
				glfwDestroyWindow(_window);
				_window = nullptr;
			}
		}
	protected:
		CursorPosCallback _cursorPosCallback;
		MouseButtonCallback _mouseButtonCallback;
		ScrollCallback _scrollCallback;
		GLFWwindow *_window = nullptr;

		template <auto CallbackMember, typename CallbackFunc, typename ...Args> void _setEventHandler(
			CallbackFunc &&func,
			void (*(*setCallback)(GLFWwindow*, void (*)(GLFWwindow*, Args...)))(GLFWwindow*, Args...)
		) {
			this->*CallbackMember = std::forward<CallbackFunc>(func);
			if (this->*CallbackMember) {
				setCallback(
					_window, [](GLFWwindow *wndHandle, Args... args) {
						auto *wnd = static_cast<Window*>(glfwGetWindowUserPointer(wndHandle));
						(wnd->*CallbackMember)(args...);
					}
				);
			} else {
				setCallback(_window, nullptr);
			}
		}
	};
}
