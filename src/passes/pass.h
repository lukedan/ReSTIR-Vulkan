#pragma once

#include <vulkan/vulkan.hpp>

class Pass {
public:
	Pass(Pass&&) = default;
	Pass &operator=(Pass&&) = default;

	struct PipelineCreationInfo {
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
				.setFrontFace(vk::FrontFace::eClockwise)
				.setLineWidth(1.0f);
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

		vk::PipelineVertexInputStateCreateInfo vertexInputState;
		vk::PipelineInputAssemblyStateCreateInfo inputAssemblyState;
		vk::PipelineViewportStateCreateInfo viewportState;
		vk::PipelineRasterizationStateCreateInfo rasterizationState;
		vk::PipelineMultisampleStateCreateInfo multisampleState;
		std::vector<vk::PipelineColorBlendAttachmentState> attachmentColorBlendStorage;
		vk::PipelineColorBlendStateCreateInfo colorBlendState;
		std::vector<vk::PipelineShaderStageCreateInfo> shaderStages;
		std::vector<vk::DynamicState> dynamicStates;
		vk::PipelineLayout pipelineLayout;
	};

	virtual ~Pass() = default;

	virtual void issueCommands(vk::CommandBuffer) const = 0;

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
			const PipelineCreationInfo &info = pipelineInfo[i];

			// attachmentColorBlendStorage acts as a storage for colorBlendState.pAttachments, so you must set it
			// using setAttachments()
			assert(info.colorBlendState.pAttachments == info.attachmentColorBlendStorage.data());

			vk::PipelineDynamicStateCreateInfo dynamicStateInfo;
			dynamicStateInfo.setDynamicStates(info.dynamicStates);

			vk::GraphicsPipelineCreateInfo pipelineInfo;
			pipelineInfo
				.setPVertexInputState(&info.vertexInputState)
				.setPInputAssemblyState(&info.inputAssemblyState)
				.setPViewportState(&info.viewportState)
				.setPRasterizationState(&info.rasterizationState)
				.setPMultisampleState(&info.multisampleState)
				.setPColorBlendState(&info.colorBlendState)
				.setStages(info.shaderStages)
				.setLayout(info.pipelineLayout)
				.setPDynamicState(&dynamicStateInfo)
				.setRenderPass(_pass.get())
				.setSubpass(static_cast<uint32_t>(i));
			pipelines[i] = dev.createGraphicsPipelineUnique(nullptr, pipelineInfo);
		}
		return pipelines;
	}
	virtual void _initialize(vk::Device dev) {
		_pass = _createPass(dev);
		_pipelines = _createPipelines(dev);
	}
private:
	vk::UniqueRenderPass _pass;
	std::vector<vk::UniquePipeline> _pipelines;
};
