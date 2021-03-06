#pragma once

#include <unordered_set>
#include <vector>
#include <filesystem>
#include <iostream>
#include <optional>
#include <random>

#include <vulkan/vulkan.hpp>

#include <gltfscene.h>

#include "transientCommandBuffer.h"
#include "shaderIncludes.h"


namespace vma {
	class Allocator;
	struct UniqueImage;
}


template <typename T> [[nodiscard]] constexpr T ceilDiv(T a, T b) {
	return (a + b - static_cast<T>(1)) / b;
}

template <typename Struct, typename PreArray> [[nodiscard]] constexpr std::size_t alignPreArrayBlock() {
	return ceilDiv(sizeof(PreArray), alignof(Struct)) * alignof(Struct);
}

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
	std::optional<float> anisotropy = std::nullopt,
	float minMipLod = 0.0f, float maxMipLod = std::numeric_limits<float>::max(), float mipLodBias = 0.0f,
	vk::SamplerAddressMode addressMode = vk::SamplerAddressMode::eRepeat,
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

void transitionImageLayout(
	vk::CommandBuffer commandBuffer,
	vk::Image image,
	vk::Format format,
	vk::ImageLayout oldImageLayout,
	vk::ImageLayout newImageLayout,
	uint32_t baseMipLevel = 0,
	uint32_t numMipLevels = 1
);

vma::UniqueImage loadTexture(
	const unsigned char *data, uint32_t width, uint32_t height, vk::Format, uint32_t mipLevels,
	vma::Allocator&, TransientCommandBufferPool&, vk::Queue
);

vma::UniqueImage loadTexture(
	const tinygltf::Image&, vk::Format, uint32_t mipLevels,
	vma::Allocator&, TransientCommandBufferPool&, vk::Queue
);


// gltf utilities
void loadScene(const std::string& filename, nvh::GltfScene& m_gltfScene);

[[nodiscard]] std::vector<shader::pointLight> collectPointLightsFromScene(const nvh::GltfScene&);
[[nodiscard]] std::vector<shader::pointLight> generateRandomPointLights(
	std::size_t count, nvmath::vec3 min, nvmath::vec3 max,
	std::uniform_real_distribution<float> distR = std::uniform_real_distribution<float>(0.0f, 1.0f),
	std::uniform_real_distribution<float> distG = std::uniform_real_distribution<float>(0.0f, 1.0f),
	std::uniform_real_distribution<float> distB = std::uniform_real_distribution<float>(0.0f, 1.0f)
);

[[nodiscard]] std::vector<shader::triLight> collectTriangleLightsFromScene(const nvh::GltfScene&);

[[nodiscard]] std::vector<shader::aliasTableColumn> createAliasTable(std::vector<shader::pointLight>& ptLights, std::vector<shader::triLight>& triLights);
