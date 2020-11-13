cmake_minimum_required(VERSION 3.17)

project(restir)

include("cmake/glslc.cmake")

add_executable(restir)

target_compile_features(restir PUBLIC cxx_std_20)

target_sources(restir
	PRIVATE
		"src/glfwWindow.cpp"
		"src/glfwWindow.h"
		"src/main.cpp"
		"src/misc.cpp"
		"src/misc.h"
		"src/swapchain.cpp"
		"src/swapchain.h" "src/vkReSTIR.h" "src/vkReSTIR.cpp")

find_package(Vulkan REQUIRED)
find_package(glfw3 REQUIRED)

target_link_libraries(restir PRIVATE Vulkan::Vulkan glfw)

add_shader(restir "src/shaders/simple.vert")
add_shader(restir "src/shaders/simple.frag")
