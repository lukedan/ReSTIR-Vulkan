#pragma once

#include <variant>

#include <vulkan/vulkan.hpp>

#include "../misc.h"

class SceneBuffers;

class Pass {
public:
	Pass(Pass&&) = default;
	Pass &operator=(Pass&&) = default;

	struct GraphicsPipelineCreationInfo {
		[[nodiscard]] inline static vk::PipelineInputAssemblyStateCreateInfo
			getTriangleListWithoutPrimitiveRestartInputAssembly() {

			vk::PipelineInputAssemblyStateCreateInfo info;
			info.setTopology(vk::PrimitiveTopology::eTriangleList);
			return info;
		}
		[[nodiscard]] inline static vk::PipelineRasterizationStateCreateInfo getDefaultRasterizationState() {
			vk::PipelineRasterizationStateCreateInfo info;
			info
				.setPolygonMode(vk::PolygonMode::eFill)
				.setCullMode(vk::CullModeFlagBits::eBack)
				.setFrontFace(vk::FrontFace::eCounterClockwise)
				.setLineWidth(1.0f);
			return info;
		}
		[[nodiscard]] inline static vk::PipelineDepthStencilStateCreateInfo getDefaultDepthTestState() {
			vk::PipelineDepthStencilStateCreateInfo info;
			info
				.setDepthTestEnable(true)
				.setDepthWriteEnable(true)
				.setDepthCompareOp(vk::CompareOp::eLess);
			return info;
		}
		[[nodiscard]] inline static vk::PipelineMultisampleStateCreateInfo getNoMultisampleState() {
			vk::PipelineMultisampleStateCreateInfo info;
			info.setRasterizationSamples(vk::SampleCountFlagBits::e1);
			return info;
		}
		[[nodiscard]] inline static vk::PipelineColorBlendAttachmentState getDefaultBlendAttachment() {
			vk::PipelineColorBlendAttachmentState info;
			info
				.setColorWriteMask(
					vk::ColorComponentFlagBits::eA | vk::ColorComponentFlagBits::eR |
					vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB
				)
				.setBlendEnable(true)
				.setSrcColorBlendFactor(vk::BlendFactor::eSrcAlpha)
				.setDstColorBlendFactor(vk::BlendFactor::eOneMinusSrcAlpha)
				.setColorBlendOp(vk::BlendOp::eAdd)
				.setSrcAlphaBlendFactor(vk::BlendFactor::eOne)
				.setDstAlphaBlendFactor(vk::BlendFactor::eZero)
				.setAlphaBlendOp(vk::BlendOp::eAdd);
			return info;
		}
		[[nodiscard]] inline static vk::PipelineColorBlendAttachmentState getNoBlendAttachment() {
			vk::PipelineColorBlendAttachmentState info;
			info
				.setColorWriteMask(
					vk::ColorComponentFlagBits::eA | vk::ColorComponentFlagBits::eR |
					vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB
				)
				.setBlendEnable(false);
			return info;
		}

		std::vector<vk::VertexInputBindingDescription> vertexInputBindingStorage;
		std::vector<vk::VertexInputAttributeDescription> vertexInputAttributeStorage;
		vk::PipelineVertexInputStateCreateInfo vertexInputState;

		vk::PipelineInputAssemblyStateCreateInfo inputAssemblyState;

		std::vector<vk::Viewport> viewportStorage;
		std::vector<vk::Rect2D> scissorStorage;
		vk::PipelineViewportStateCreateInfo viewportState;

		vk::PipelineRasterizationStateCreateInfo rasterizationState;

		vk::PipelineDepthStencilStateCreateInfo depthStencilState;

		vk::PipelineMultisampleStateCreateInfo multisampleState;

		std::vector<vk::PipelineColorBlendAttachmentState> attachmentColorBlendStorage;
		vk::PipelineColorBlendStateCreateInfo colorBlendState;

		std::vector<vk::PipelineShaderStageCreateInfo> shaderStages;

		std::vector<vk::DynamicState> dynamicStates;

		vk::PipelineLayout pipelineLayout;
	};
	using PipelineCreationInfo = std::variant<GraphicsPipelineCreationInfo, vk::ComputePipelineCreateInfo>;


	virtual ~Pass() = default;

	virtual void issueCommands(vk::CommandBuffer, vk::Framebuffer) const = 0;

	[[nodiscard]] vk::RenderPass getPass() const {
		return _pass.get();
	}
	[[nodiscard]] const std::vector<vk::UniquePipeline> &getPipelines() const {
		return _pipelines;
	}

	template <typename PassT, typename ...Args> [[nodiscard]] inline static PassT create(
		vk::Device dev, Args &&...args
	) {
		static_assert(std::is_base_of_v<Pass, PassT>, "Passes must derive from class Pass");
		PassT pass(std::forward<Args>(args)...);
		static_cast<Pass*>(&pass)->_initialize(dev);
		return std::move(pass);
	}
protected:
	Pass() = default;

	[[nodiscard]] virtual vk::UniqueRenderPass _createPass(vk::Device) = 0;
	[[nodiscard]] virtual std::vector<PipelineCreationInfo> _getPipelineCreationInfo() = 0;
	[[nodiscard]] virtual std::vector<vk::UniquePipeline> _createPipelines(vk::Device dev) {
		std::vector<PipelineCreationInfo> pipelineInfo = _getPipelineCreationInfo();
		std::vector<vk::UniquePipeline> pipelines(pipelineInfo.size());
		for (std::size_t i = 0; i < pipelineInfo.size(); ++i) {
			if (std::holds_alternative<GraphicsPipelineCreationInfo>(pipelineInfo[i])) {
				const auto &info = std::get<GraphicsPipelineCreationInfo>(pipelineInfo[i]);

				// checks that storages are properly bound
				assert(info.vertexInputState.pVertexBindingDescriptions == info.vertexInputBindingStorage.data());
				assert(info.vertexInputState.pVertexAttributeDescriptions == info.vertexInputAttributeStorage.data());
				assert(info.colorBlendState.pAttachments == info.attachmentColorBlendStorage.data());
				assert(info.viewportState.pViewports == info.viewportStorage.data());
				assert(info.viewportState.pScissors == info.scissorStorage.data());

				vk::PipelineDynamicStateCreateInfo dynamicStateInfo;
				dynamicStateInfo.setDynamicStates(info.dynamicStates);

				vk::GraphicsPipelineCreateInfo thisPipelineInfo;
				thisPipelineInfo
					.setPVertexInputState(&info.vertexInputState)
					.setPInputAssemblyState(&info.inputAssemblyState)
					.setPViewportState(&info.viewportState)
					.setPRasterizationState(&info.rasterizationState)
					.setPDepthStencilState(&info.depthStencilState)
					.setPMultisampleState(&info.multisampleState)
					.setPColorBlendState(&info.colorBlendState)
					.setStages(info.shaderStages)
					.setLayout(info.pipelineLayout)
					.setPDynamicState(&dynamicStateInfo)
					.setRenderPass(_pass.get())
					.setSubpass(static_cast<uint32_t>(i));
				auto [result, pipe] = dev.createGraphicsPipelineUnique(nullptr, thisPipelineInfo).asTuple();
				vkCheck(result);
				pipelines[i] = std::move(pipe);
			} else if (std::holds_alternative<vk::ComputePipelineCreateInfo>(pipelineInfo[i])) {
				const auto &info = std::get<vk::ComputePipelineCreateInfo>(pipelineInfo[i]);
				auto [result, pipe] = dev.createComputePipelineUnique(nullptr, info);
				vkCheck(result);
				pipelines[i] = std::move(pipe);
			}
		}
		return pipelines;
	}
	void _recreatePipelines(vk::Device dev) {
		_pipelines.clear();
		_pipelines = _createPipelines(dev);
	}
	virtual void _initialize(vk::Device dev) {
		_pass = _createPass(dev);
		_pipelines = _createPipelines(dev);
	}
private:
	vk::UniqueRenderPass _pass;
	std::vector<vk::UniquePipeline> _pipelines;
};
