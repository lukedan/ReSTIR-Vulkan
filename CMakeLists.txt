cmake_minimum_required(VERSION 3.17)

cmake_policy(SET CMP0092 NEW) # don't add /W3 for MSVC

project(restir)

file(GLOB_RECURSE ALL_SHADER_FILES LIST_DIRECTORIES false "src/shaders/*.*")

function(add_shader TARGET SHADER)
	# Find glslc shader compiler.
	find_program(GLSLC glslc)

	get_filename_component(SHADER_FILE ${SHADER} NAME)
	set(OUTPUT_PATH ${CMAKE_CURRENT_BINARY_DIR}/shaders/${SHADER_FILE}.spv)

	# Add a custom command to compile GLSL to SPIR-V.
	get_filename_component(OUTPUT_DIR ${OUTPUT_PATH} DIRECTORY)
	file(MAKE_DIRECTORY ${OUTPUT_DIR})
	add_custom_command(
		OUTPUT ${OUTPUT_PATH}
		COMMAND ${GLSLC} -g -O -o ${OUTPUT_PATH}  ${SHADER} --target-env=vulkan1.2
		DEPENDS ${ALL_SHADER_FILES}
		IMPLICIT_DEPENDS CXX ${SHADER}
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
		VERBATIM)

	# Make sure our native build depends on this output.
	set_source_files_properties(${OUTPUT_PATH} PROPERTIES GENERATED TRUE)
	target_sources(${TARGET} PRIVATE ${OUTPUT_PATH})
endfunction(add_shader)

add_executable(restir)

target_compile_features(restir PUBLIC cxx_std_20)
# set warning level
if(MSVC)
	target_compile_options(restir
		PRIVATE /W4 /permissive- /experimental:external /external:anglebrackets /external:W3)
elseif(CMAKE_COMPILER_IS_GNUCXX)
	target_compile_options(restir
		PRIVATE -Wall -Wextra -Wconversion)
endif()

target_compile_definitions(restir
	PRIVATE
#		RENDERDOC_CAPTURE
		VK_ENABLE_BETA_EXTENSIONS)

target_sources(restir
	PRIVATE
		"src/vertex.h"
		"src/passes/demoPass.h"
		"src/passes/imguiPass.h"
		"src/passes/gBufferPass.cpp"
		"src/passes/gBufferPass.h"
		"src/passes/lightingPass.h"
		"src/passes/pass.h"
		"src/passes/restirPass.h"
		"src/passes/spatialReusePass.h"
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
		"src/transientCommandBuffer.h"
		"src/shader.h"
		"src/vma.cpp"
		"src/vma.h")


find_package(Vulkan REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
add_subdirectory("thirdparty/MikkTSpace/")
add_subdirectory("thirdparty/gltf/")


target_link_libraries(restir PRIVATE Vulkan::Vulkan glfw imgui::imgui MikkTSpace gltf)

target_include_directories(restir
	PRIVATE
		"thirdparty/nvmath/"
		"thirdparty/VulkanMemoryAllocator/src/"
		"thirdparty/tinygltf/")

add_shader(restir "src/shaders/simple.vert")
add_shader(restir "src/shaders/simple.frag")

add_shader(restir "src/shaders/gBuffer.vert")
add_shader(restir "src/shaders/gBuffer.frag")

add_shader(restir "src/shaders/spatialReuse.comp")

add_shader(restir "src/shaders/quad.vert")
add_shader(restir "src/shaders/lighting.frag")

add_shader(restir "src/shaders/hwVisibilityTest.rchit")
add_shader(restir "src/shaders/hwVisibilityTest.rmiss")
add_shader(restir "src/shaders/hwVisibilityTestShadow.rmiss")

add_shader(restir "src/shaders/restirOmniHardware.rgen")
add_shader(restir "src/shaders/restirOmniSoftware.comp")

add_shader(restir "src/shaders/unbiasedReuseHardware.rgen")
add_shader(restir "src/shaders/unbiasedReuseSoftware.comp")
