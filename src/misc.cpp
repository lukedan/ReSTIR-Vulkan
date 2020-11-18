#include "misc.h"

#include <fstream>

std::vector<char> readFile(const std::filesystem::path &path) {
	std::ifstream fin(path, std::ios::ate | std::ios::binary);
	std::streampos size = fin.tellg();
	std::vector<char> result(static_cast<std::size_t>(size));
	fin.seekg(0);
	fin.read(result.data(), size);
	return result;
}

void vkCheck(vk::Result res) {
	if (static_cast<int>(res) < 0) {
		std::cout << "Vulkan error: " << vk::to_string(res) << "\n";
		std::abort();
	}
}

[[nodiscard]] vk::Format findSupportedFormat(
	const std::vector<vk::Format> &candidates, vk::PhysicalDevice physicalDevice,
	vk::ImageTiling requiredTiling, vk::FormatFeatureFlags requiredFeatures
) {
	for (vk::Format fmt : candidates) {
		vk::FormatProperties properties = physicalDevice.getFormatProperties(fmt);
		vk::FormatFeatureFlags features =
			requiredTiling == vk::ImageTiling::eLinear ?
			properties.linearTilingFeatures :
			properties.optimalTilingFeatures;
		if ((features & requiredFeatures) == requiredFeatures) {
			return fmt;
		}
	}
	std::cout <<
		"Failed to find format with tiling " << vk::to_string(requiredTiling) <<
		" and features " << vk::to_string(requiredFeatures) << "\n";
	std::abort();
}

vk::UniqueImageView createImageView2D(
	vk::Device device, vk::Image image, vk::Format format, vk::ImageAspectFlags aspect,
	uint32_t baseMipLevel, uint32_t mipLevelCount, uint32_t baseArrayLayer, uint32_t arrayLayerCount
) {
	vk::ImageSubresourceRange range;
	range
		.setAspectMask(aspect)
		.setBaseMipLevel(baseMipLevel)
		.setLevelCount(mipLevelCount)
		.setBaseArrayLayer(baseArrayLayer)
		.setLayerCount(arrayLayerCount);

	vk::ImageViewCreateInfo imageViewInfo;
	imageViewInfo
		.setImage(image)
		.setViewType(vk::ImageViewType::e2D)
		.setFormat(format)
		.setSubresourceRange(range);

	return device.createImageViewUnique(imageViewInfo);
}


void loadScene(const std::string& filename, nvh::GltfScene& m_gltfScene) {
	tinygltf::Model    tmodel;
	tinygltf::TinyGLTF tcontext;
	std::string        warn, error;
	if (!tcontext.LoadASCIIFromFile(&tmodel, &error, &warn, filename)) {
		assert(!"Error while loading scene");
	}
	m_gltfScene.importDrawableNodes(tmodel, nvh::GltfAttributes::Normal);
	m_gltfScene.importMaterials(tmodel);
	m_gltfScene.importTexutureImages(tmodel);

	for (size_t i = 0; i < tmodel.images.size(); i++) {

	}

	// Show gltf scene info
	std::cout << "Show gltf scene info" << std::endl;
	std::cout << "scene center:[" << m_gltfScene.m_dimensions.center.x << ", "
		<< m_gltfScene.m_dimensions.center.y << ", "
		<< m_gltfScene.m_dimensions.center.z << "]" << std::endl;

	std::cout << "max:[" << m_gltfScene.m_dimensions.max.x << ", "
		<< m_gltfScene.m_dimensions.max.y << ", "
		<< m_gltfScene.m_dimensions.max.z << "]" << std::endl;

	std::cout << "min:[" << m_gltfScene.m_dimensions.min.x << ", "
		<< m_gltfScene.m_dimensions.min.y << ", "
		<< m_gltfScene.m_dimensions.min.z << "]" << std::endl;

	std::cout << "radius:" << m_gltfScene.m_dimensions.radius << std::endl;

	std::cout << "size:[" << m_gltfScene.m_dimensions.size.x << ", "
		<< m_gltfScene.m_dimensions.size.y << ", "
		<< m_gltfScene.m_dimensions.size.z << "]" << std::endl;

	std::cout << "vertex num:" << m_gltfScene.m_positions.size() << std::endl;
}
