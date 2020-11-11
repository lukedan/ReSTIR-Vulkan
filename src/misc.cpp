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

vk::UniqueShaderModule loadShader(const vk::UniqueDevice &dev, const std::filesystem::path &path) {
	std::vector<char> binary = readFile(path);

	vk::ShaderModuleCreateInfo shaderInfo;
	shaderInfo
		.setCodeSize(binary.size())
		.setPCode(reinterpret_cast<const uint32_t*>(binary.data()));
	return dev->createShaderModuleUnique(shaderInfo);
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