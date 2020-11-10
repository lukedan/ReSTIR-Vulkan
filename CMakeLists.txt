cmake_minimum_required(VERSION 3.17)

project(restir)

add_executable(restir)

target_compile_features(restir PUBLIC cxx_std_20)

target_sources(restir
	PRIVATE
		"src/main.cpp")

find_package(Vulkan REQUIRED)

target_link_libraries(restir PUBLIC Vulkan::Vulkan)
