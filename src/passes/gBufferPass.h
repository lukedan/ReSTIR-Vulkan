#pragma once

#include <vulkan/vulkan.hpp>

#include "pass.h"
#include "../vma.h"
#include "../misc.h"
#include "../shader.h"
#include "../sceneBuffers.h"
#include "../camera.h"

class GBufferPass;

class GBuffer {
public:
	[[nodiscard]] vk::Image getAlbedoBuffer() const {
		return _albedoBuffer.get();
	}
	[[nodiscard]] vk::Image getNormalBuffer() const {
		return _normalBuffer.get();
	}
	[[nodiscard]] vk::Image getDepthBuffer() const {
		return _depthBuffer.get();
	}

	[[nodiscard]] vk::ImageView getAlbedoView() const {
		return _albedoView.get();
	}
	[[nodiscard]] vk::ImageView getNormalView() const {
		return _normalView.get();
	}
	[[nodiscard]] vk::ImageView getDepthView() const {
		return _depthView.get();
	}

	[[nodiscard]] vk::Framebuffer getFramebuffer() const {
		return _framebuffer.get();
	}

	void resize(vma::Allocator&, vk::Device, vk::Extent2D, Pass&);

	[[nodiscard]] inline static GBuffer create(
		vma::Allocator &allocator, vk::Device device, vk::Extent2D bufferExtent, Pass &pass
	) {
		GBuffer result;
		result.resize(allocator, device, bufferExtent, pass);
		return result;
	}

	struct Formats {
		vk::Format albedo;
		vk::Format normal;
		vk::Format depth;
		vk::ImageAspectFlags depthAspect;

		[[nodiscard]] static void initialize(vk::PhysicalDevice);
		[[nodiscard]] static const Formats &get();
	};
private:
	vma::UniqueImage _albedoBuffer;
	vma::UniqueImage _normalBuffer;
	vma::UniqueImage _depthBuffer;

	vk::UniqueImageView _albedoView;
	vk::UniqueImageView _normalView;
	vk::UniqueImageView _depthView;

	vk::UniqueFramebuffer _framebuffer;
};

class GBufferPass : public Pass {
	friend Pass;
public:
	struct PushConstants {
		nvmath::mat4 transform;
	};

	GBufferPass() = default;

	void issueCommands(vk::CommandBuffer commandBuffer, vk::Framebuffer framebuffer) const override {
		std::array<vk::ClearValue, 3> clearValues{
			vk::ClearColorValue(std::array<float, 4>{ 0.0f, 0.0f, 0.0f, 1.0f }),
			vk::ClearColorValue(std::array<float, 4>{ 0.0f, 0.0f, 0.0f, 1.0f }),
			vk::ClearDepthStencilValue(1.0f)
		};
		vk::RenderPassBeginInfo passBeginInfo;
		passBeginInfo
			.setRenderPass(getPass())
			.setFramebuffer(framebuffer)
			.setRenderArea(vk::Rect2D(vk::Offset2D(0, 0), _bufferExtent))
			.setClearValues(clearValues);
		commandBuffer.beginRenderPass(passBeginInfo, vk::SubpassContents::eInline);

		commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, getPipelines()[0].get());
		commandBuffer.bindVertexBuffers(0, { sceneBuffers->getVertices() }, { 0 });
		commandBuffer.bindIndexBuffer(sceneBuffers->getIndices(), 0, vk::IndexType::eUint32);
		for (const nvh::GltfNode &node : scene->m_nodes) {
			PushConstants constants;
			constants.transform = camera->projectionViewMatrix * node.worldMatrix;
			commandBuffer.pushConstants(
				_pipelineLayout.get(), vk::ShaderStageFlagBits::eVertex, 0, sizeof(PushConstants), &constants
			);

			const nvh::GltfPrimMesh &mesh = scene->m_primMeshes[node.primMesh];
			commandBuffer.drawIndexed(mesh.indexCount, 1, mesh.firstIndex, mesh.vertexOffset, 0);
		}

		commandBuffer.endRenderPass();
	}

	void onResized(vk::Device dev, vk::Extent2D extent) {
		_bufferExtent = extent;
		_recreatePipelines(dev);
	}

	const nvh::GltfScene *scene = nullptr;
	const SceneBuffers *sceneBuffers = nullptr;
	const Camera *camera = nullptr;
protected:
	explicit GBufferPass(vk::Extent2D extent) : _bufferExtent(extent) {
	}

	vk::Extent2D _bufferExtent;
	Shader _vert, _frag;
	vk::UniquePipelineLayout _pipelineLayout;

	vk::UniqueRenderPass _createPass(vk::Device device) override {
		const GBuffer::Formats &formats = GBuffer::Formats::get();

		std::vector<vk::AttachmentDescription> attachments;
		attachments.emplace_back()
			.setFormat(formats.albedo)
			.setSamples(vk::SampleCountFlagBits::e1)
			.setLoadOp(vk::AttachmentLoadOp::eClear)
			.setStoreOp(vk::AttachmentStoreOp::eStore)
			.setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
			.setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
			.setInitialLayout(vk::ImageLayout::eUndefined)
			.setFinalLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
		attachments.emplace_back()
			.setFormat(formats.normal)
			.setSamples(vk::SampleCountFlagBits::e1)
			.setLoadOp(vk::AttachmentLoadOp::eClear)
			.setStoreOp(vk::AttachmentStoreOp::eStore)
			.setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
			.setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
			.setInitialLayout(vk::ImageLayout::eUndefined)
			.setFinalLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
		attachments.emplace_back()
			.setFormat(formats.depth)
			.setSamples(vk::SampleCountFlagBits::e1)
			.setLoadOp(vk::AttachmentLoadOp::eClear)
			.setStoreOp(vk::AttachmentStoreOp::eStore)
			.setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
			.setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
			.setInitialLayout(vk::ImageLayout::eUndefined)
			.setFinalLayout(vk::ImageLayout::eShaderReadOnlyOptimal);

		std::vector<vk::AttachmentReference> colorAttachmentReferences;
		colorAttachmentReferences.emplace_back()
			.setAttachment(0)
			.setLayout(vk::ImageLayout::eColorAttachmentOptimal);
		colorAttachmentReferences.emplace_back()
			.setAttachment(1)
			.setLayout(vk::ImageLayout::eColorAttachmentOptimal);

		vk::AttachmentReference depthAttachmentReference;
		depthAttachmentReference
			.setAttachment(2)
			.setLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal);

		std::vector<vk::SubpassDescription> subpasses;
		subpasses.emplace_back()
			.setPipelineBindPoint(vk::PipelineBindPoint::eGraphics)
			.setColorAttachments(colorAttachmentReferences)
			.setPDepthStencilAttachment(&depthAttachmentReference);

		std::vector<vk::SubpassDependency> dependencies;
		dependencies.emplace_back()
			.setSrcSubpass(VK_SUBPASS_EXTERNAL)
			.setDstSubpass(0)
			.setSrcStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput)
			.setSrcAccessMask(vk::AccessFlags())
			.setDstStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput)
			.setDstAccessMask(vk::AccessFlagBits::eColorAttachmentWrite);

		vk::RenderPassCreateInfo renderPassInfo;
		renderPassInfo
			.setAttachments(attachments)
			.setSubpasses(subpasses)
			.setDependencies(dependencies);

		return device.createRenderPassUnique(renderPassInfo);
	}
	std::vector<PipelineCreationInfo> _getPipelineCreationInfo() {
		std::vector<PipelineCreationInfo> result;
		PipelineCreationInfo &info = result.emplace_back();

		info.vertexInputBindingStorage.emplace_back(0, sizeof(Vertex), vk::VertexInputRate::eVertex);
		info.vertexInputAttributeStorage.emplace_back(0, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, position));
		info.vertexInputAttributeStorage.emplace_back(1, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, normal));
		info.vertexInputAttributeStorage.emplace_back(2, 0, vk::Format::eR32G32B32A32Sfloat, offsetof(Vertex, color));
		info.vertexInputAttributeStorage.emplace_back(3, 0, vk::Format::eR32G32Sfloat, offsetof(Vertex, uv));
		info.vertexInputState
			.setVertexBindingDescriptions(info.vertexInputBindingStorage)
			.setVertexAttributeDescriptions(info.vertexInputAttributeStorage);

		info.inputAssemblyState = PipelineCreationInfo::getTriangleListWithoutPrimitiveRestartInputAssembly();

		info.viewportStorage.emplace_back(
			0.0f, 0.0f, _bufferExtent.width, _bufferExtent.height, 0.0f, 1.0f
		);
		info.scissorStorage.emplace_back(vk::Offset2D(0, 0), _bufferExtent);
		info.viewportState
			.setViewports(info.viewportStorage)
			.setScissors(info.scissorStorage);

		info.rasterizationState = PipelineCreationInfo::getDefaultRasterizationState();

		info.depthStencilState = PipelineCreationInfo::getDefaultDepthTestState();

		info.multisampleState = PipelineCreationInfo::getNoMultisampleState();

		info.attachmentColorBlendStorage.emplace_back(PipelineCreationInfo::getNoBlendAttachment());
		info.attachmentColorBlendStorage.emplace_back(PipelineCreationInfo::getNoBlendAttachment());
		info.colorBlendState.setAttachments(info.attachmentColorBlendStorage);

		info.shaderStages.emplace_back(_frag.getStageInfo());
		info.shaderStages.emplace_back(_vert.getStageInfo());

		info.pipelineLayout = _pipelineLayout.get();

		return result;
	}
	void _initialize(vk::Device dev) override {
		_vert = Shader::load(dev, "shaders/gBuffer.vert.spv", "main", vk::ShaderStageFlagBits::eVertex);
		_frag = Shader::load(dev, "shaders/gBuffer.frag.spv", "main", vk::ShaderStageFlagBits::eFragment);

		std::array<vk::PushConstantRange, 1> pushConstants{
			vk::PushConstantRange(vk::ShaderStageFlagBits::eVertex, 0, sizeof(PushConstants))
		};
		vk::PipelineLayoutCreateInfo pipelineInfo;
		pipelineInfo.setPushConstantRanges(pushConstants);
		_pipelineLayout = dev.createPipelineLayoutUnique(pipelineInfo);

		Pass::_initialize(dev);
	}
};
