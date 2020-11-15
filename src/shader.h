#pragma once

#include <vulkan/vulkan.hpp>

#include "misc.h"

class Shader {
public:
	[[nodiscard]] vk::ShaderModule get() const {
		return _module.get();
	}
	[[nodiscard]] const vk::PipelineShaderStageCreateInfo &getStageInfo() const {
		return _shaderInfo;
	}

	[[nodiscard]] bool empty() const {
		return !_module;
	}
	[[nodiscard]] explicit operator bool() const {
		return static_cast<bool>(_module);
	}

	[[nodiscard]] inline static Shader load(
		vk::Device dev, const std::filesystem::path &path, const char *entry, vk::ShaderStageFlagBits stage
	) {
		Shader result;

		std::vector<char> binary = readFile(path);

		vk::ShaderModuleCreateInfo shaderInfo;
		shaderInfo
			.setCodeSize(binary.size())
			.setPCode(reinterpret_cast<const uint32_t*>(binary.data()));
		result._module = dev.createShaderModuleUnique(shaderInfo);

		result._shaderInfo
			.setModule(result._module.get())
			.setPName(entry)
			.setStage(stage);

		return result;
	}
private:
	vk::UniqueShaderModule _module;
	vk::PipelineShaderStageCreateInfo _shaderInfo;
};
