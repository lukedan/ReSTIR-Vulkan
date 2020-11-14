#pragma once

#include <unordered_set>
#include <vector>
#include <filesystem>
#include <iostream>

#include <vulkan/vulkan.hpp>
#include "../gltf/gltfscene.h"

template <auto MPtr, typename T> bool checkSupport(
	const std::vector<const char*> &required, const std::vector<T> &supported,
	std::string_view name, std::string_view indent = ""
) {
	std::unordered_set<std::string> pending(required.begin(), required.end());
	std::cout << indent << "Supported " << name << ":\n";
	for (const T &ext : supported) {
		if (auto it = pending.find(ext.*MPtr); it != pending.end()) {
			pending.erase(it);
			std::cout << indent << "  * ";
		} else {
			std::cout << indent << "    ";
		}
		std::cout << (ext.*MPtr) << "\n";
	}
	if (!pending.empty()) {
		std::cout << indent << "The following " << name << " are not supported:\n";
		for (const std::string &ext : pending) {
			std::cout << indent << "    " << ext << "\n";
		}
	}
	std::cout << "\n";
	return pending.empty();
}

[[nodiscard]] std::vector<char> readFile(const std::filesystem::path&);

[[nodiscard]] vk::UniqueShaderModule loadShader(vk::Device, const std::filesystem::path&);

void vkCheck(vk::Result);


// Load a gltf scene
void loadScene(const std::string& filename, GltfScene& m_gltfScene);
