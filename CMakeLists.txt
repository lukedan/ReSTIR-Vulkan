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
		"src/vertex.h"
		"src/passes/demoPass.h"
		"src/passes/gBufferPass.cpp"
		"src/passes/gBufferPass.h"
		"src/passes/lightingPass.h"
		"src/passes/pass.h"
		"src/aabbTreeBuilder.cpp"
		"src/aabbTreeBuilder.h"
		"src/app.cpp"
		"src/app.h"
		"src/camera.h"
		"src/fpsCounter.h"
		"src/glfwWindow.cpp"
		"src/glfwWindow.h"
		"src/main.cpp"
		"src/misc.cpp"
		"src/misc.h"
		"src/sceneBuffers.h"
		"src/shaderIncludes.h"
		"src/swapchain.cpp"
		"src/swapchain.h"
		# "gltf/gltfscene.h"
		"gltf/gltfscene.cpp"
		"src/shader.h"
		"src/vma.cpp"
		"src/vma.h")


find_package(Vulkan REQUIRED)
find_package(glfw3 CONFIG REQUIRED)

target_link_libraries(restir PRIVATE Vulkan::Vulkan glfw)

target_include_directories(restir
	PRIVATE
		"thirdparty/nvmath/"
		"thirdparty/VulkanMemoryAllocator/src/"
		"thirdparty/tinygltf/")

add_shader(restir "src/shaders/simple.vert")
add_shader(restir "src/shaders/simple.frag")

add_shader(restir "src/shaders/gBuffer.vert")
add_shader(restir "src/shaders/gBuffer.frag")

add_shader(restir "src/shaders/quad.vert")
add_shader(restir "src/shaders/lighting.frag")

