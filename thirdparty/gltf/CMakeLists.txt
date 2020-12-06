cmake_minimum_required(VERSION 3.17)

cmake_policy(SET CMP0092 NEW) # don't add /W3 for MSVC

project(nvh)

add_library(gltf STATIC)

target_sources(gltf
	PUBLIC
		gltfscene.h
	PRIVATE
		mikktWrapper.h
		gltfscene.cpp  
		mikktWrapper.cpp)
target_include_directories(gltf
	INTERFACE
		"./")

target_include_directories(gltf
	PUBLIC
		"../nvmath/"
		"../tinygltf/"
		"../MikkTSpace/")

find_package(Vulkan REQUIRED)