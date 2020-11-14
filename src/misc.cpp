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

vk::UniqueShaderModule loadShader(vk::Device dev, const std::filesystem::path &path) {
	std::vector<char> binary = readFile(path);

	vk::ShaderModuleCreateInfo shaderInfo;
	shaderInfo
		.setCodeSize(binary.size())
		.setPCode(reinterpret_cast<const uint32_t*>(binary.data()));
	return dev.createShaderModuleUnique(shaderInfo);
}

vk::PipelineShaderStageCreateInfo getShaderStageInfo(
	const vk::UniqueShaderModule &mod, vk::ShaderStageFlagBits stage, const char *entry
) {
	vk::PipelineShaderStageCreateInfo info;
	return info
		.setModule(mod.get())
		.setStage(stage)
		.setPName(entry);
}

void vkCheck(vk::Result res) {
	if (static_cast<int>(res) < 0) {
		std::cout << "Vulkan error: " << vk::to_string(res) << "\n";
		std::abort();
	}
}

void loadScene(const std::string& filename, nvh::GltfScene& m_gltfScene) {
	tinygltf::Model    tmodel;
	tinygltf::TinyGLTF tcontext;
	std::string        warn, error;
	if (!tcontext.LoadASCIIFromFile(&tmodel, &error, &warn, filename))
	{
		assert(!"Error while loading scene");
	}
	m_gltfScene.importDrawableNodes(tmodel, nvh::GltfAttributes::Normal);
	
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
