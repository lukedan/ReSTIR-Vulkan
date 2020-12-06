cmake_minimum_required(VERSION 3.17)

cmake_policy(SET CMP0092 NEW) # don't add /W3 for MSVC

project(tangent)

add_library(MikkTSpace STATIC)

target_sources(MikkTSpace
	PUBLIC
		mikktspace.h
	PRIVATE
		mikktspace.c  )
target_include_directories(MikkTSpace
	INTERFACE
		"./")