#pragma once

#include <unordered_set>
#include <vector>
#include <filesystem>
#include <iostream>
#include <optional>

#include <vulkan/vulkan.hpp>
#include "../gltf/gltfscene.h"


[[nodiscard]] std::vector<char> readFile(const std::filesystem::path&);


// vulkan helpers
void vkCheck(vk::Result);
inline void vkCheck(VkResult res) {
	return vkCheck(static_cast<vk::Result>(res));
}

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

[[nodiscard]] vk::Format findSupportedFormat(
	const std::vector<vk::Format> &candidates, vk::PhysicalDevice, vk::ImageTiling, vk::FormatFeatureFlags
);

[[nodiscard]] vk::UniqueImageView createImageView2D(
	vk::Device device, vk::Image image, vk::Format format, vk::ImageAspectFlags aspect,
	uint32_t baseMipLevel = 0, uint32_t mipLevelCount = 1,
	uint32_t baseArrayLayer = 0, uint32_t arrayLayerCount = 1
);

[[nodiscard]] inline vk::UniqueSampler createSampler(
	vk::Device device, vk::Filter magFilter = vk::Filter::eLinear, vk::Filter minFilter = vk::Filter::eLinear,
	vk::SamplerMipmapMode mipmapMode = vk::SamplerMipmapMode::eLinear,
	float minMipLod = 0.0f, float maxMipLod = std::numeric_limits<float>::max(), float mipLodBias = 0.0f,
	vk::SamplerAddressMode addressMode = vk::SamplerAddressMode::eRepeat,
	std::optional<float> anisotropy = std::nullopt,
	std::optional<vk::CompareOp> compare = std::nullopt,
	bool unnormalizedCoordinates = false
) {
	vk::SamplerCreateInfo samplerInfo;
	samplerInfo
		.setMinFilter(minFilter)
		.setMagFilter(magFilter)
		.setMipmapMode(mipmapMode)
		.setMinLod(minMipLod)
		.setMaxLod(maxMipLod)
		.setMipLodBias(mipLodBias)
		.setAddressModeU(addressMode)
		.setAddressModeV(addressMode)
		.setAddressModeW(addressMode)
		.setAnisotropyEnable(anisotropy.has_value())
		.setCompareEnable(compare.has_value())
		.setUnnormalizedCoordinates(unnormalizedCoordinates);
	if (anisotropy) {
		samplerInfo.setMaxAnisotropy(anisotropy.value());
	}
	if (compare) {
		samplerInfo.setCompareOp(compare.value());
	}
	return device.createSamplerUnique(samplerInfo);
}


// Load a gltf scene
void loadScene(const std::string& filename, nvh::GltfScene& m_gltfScene);
