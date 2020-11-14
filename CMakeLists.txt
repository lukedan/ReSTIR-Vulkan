cmake_minimum_required(VERSION 3.17)

cmake_policy(SET CMP0092 NEW) # don't add /W3 for MSVC

project(restir)

include("cmake/glslc.cmake")

add_executable(restir)

target_compile_features(restir PUBLIC cxx_std_20)
# set warning level
if(MSVC)
	target_compile_options(restir
		PRIVATE /W4 /permissive-)
elseif(CMAKE_COMPILER_IS_GNUCXX)
	target_compile_options(restir
		PRIVATE -Wall -Wextra -Wconversion)
endif()

target_sources(restir
	PRIVATE
		"src/glfwWindow.cpp"
		"src/glfwWindow.h"
		"src/main.cpp"
		"src/misc.cpp"
		"src/misc.h"
		"src/swapchain.cpp"
		"src/swapchain.h"
		"gltf/json.hpp"
		"gltf/stb_image.h"
		"gltf/stb_image_write.h"
		"gltf/tiny_gltf.h"
		 "gltf/gltfscene.h" "gltf/gltfscene.cpp")

find_package(Vulkan REQUIRED)
find_package(glfw3 REQUIRED)

target_include_directories(restir PUBLIC "external_source/nvmath/")
target_link_libraries(restir PRIVATE Vulkan::Vulkan glfw)

add_shader(restir "src/shaders/simple.vert")
add_shader(restir "src/shaders/simple.frag")
