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
	struct Uniforms {
		nvmath::mat4 projectionViewMatrix;
	};

	struct Resources {
		vma::UniqueBuffer uniformBuffer;
		vk::UniqueDescriptorSet uniformDescriptor;
		vk::UniqueDescriptorSet matrixDescriptor;
		vk::UniqueDescriptorSet tempTexture;
		std::vector<vk::UniqueDescriptorSet> textures;
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

		// Keeping track of the last material to avoid binding them again
		uint32_t lastMaterial = -1;

		for (std::size_t i = 0; i < scene->m_nodes.size(); ++i) {
			const nvh::GltfNode &node = scene->m_nodes[i];
			const nvh::GltfPrimMesh &mesh = scene->m_primMeshes[node.primMesh];
			/*const nvh::GltfMaterial &material = scene->m_materials[mesh.materialIndex];*/
			// Material Push Constants
			if (lastMaterial != mesh.materialIndex) {
				lastMaterial = mesh.materialIndex;
				const nvh::GltfMaterial& mat(scene->m_materials[lastMaterial]);
				commandBuffer.pushConstants<nvh::GltfMaterial>(_pipelineLayout.get(), vk::ShaderStageFlagBits::eFragment, 0, mat);
			}

			// TODO textures
			std::vector<vk::DescriptorSet> bindedDescripotrs{ descriptorSets->uniformDescriptor.get(), descriptorSets->matrixDescriptor.get(), descriptorSets->tempTexture.get() };
			/*
			if (sceneBuffers->_textureImages.size() > 0) {
				bindedDescripotrs.push_back(descriptorSets->tempTexture.get());
			}
			*/
			commandBuffer.bindDescriptorSets(
				vk::PipelineBindPoint::eGraphics, _pipelineLayout.get(), 0,
				// { descriptorSets->uniformDescriptor.get(), descriptorSets->matrixDescriptor.get(), descriptorSets->tempTexture.get() },
				bindedDescripotrs,
				{ static_cast<uint32_t>(i * sizeof(SceneBuffers::ModelMatrices)) }
			);
			commandBuffer.drawIndexed(mesh.indexCount, 1, mesh.firstIndex, mesh.vertexOffset, 0);
		}

		commandBuffer.endRenderPass();
	}

	void onResized(vk::Device dev, vk::Extent2D extent) {
		_bufferExtent = extent;
		_recreatePipelines(dev);
	}

	[[nodiscard]] vk::DescriptorSetLayout getMatricesDescriptorSetLayout() const {
		return _matricesDescriptorSetLayout.get();
	}
	[[nodiscard]] vk::DescriptorSetLayout getUniformsDescriptorSetLayout() const {
		return _uniformsDescriptorSetLayout.get();
	}
	[[nodiscard]] vk::DescriptorSetLayout getSceneTexturesDescriptorSetLayout() const {
		return _sceneTexturesDescriptorSetLayout.get();
	}

	void initializeResourcesFor(
		const nvh::GltfScene &scene, const SceneBuffers &buffers,
		vk::UniqueDevice& device, vma::Allocator &alloc, Resources &sets
	) {
		std::vector<vk::WriteDescriptorSet> bufferWrite;
		bufferWrite.reserve(sets.textures.size() + 2 + scene.m_textures.size());

		/*std::vector<vk::DescriptorImageInfo> imageInfo(sets.textures.size());
		for (std::size_t i = 0; i < sets.textures.size(); ++i) {
			imageInfo[i] = vk::DescriptorImageInfo(_sampler.get(), );
			bufferWrite.emplace_back()
				.setDstSet(sets.textures[i].get())
				.setDstBinding(0)
				.setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
				.setImageInfo(imageInfo[i]);
		}*/

		std::array<vk::DescriptorBufferInfo, 1> uniformBufferInfo{
			vk::DescriptorBufferInfo(sets.uniformBuffer.get(), 0, sizeof(Uniforms))
		};
		bufferWrite.emplace_back()
			.setDstSet(sets.uniformDescriptor.get())
			.setDstBinding(0)
			.setDescriptorType(vk::DescriptorType::eUniformBuffer)
			.setBufferInfo(uniformBufferInfo);

		std::array<vk::DescriptorBufferInfo, 1> matricesBufferInfo{
			vk::DescriptorBufferInfo(buffers.getMatrices(), 0, sizeof(SceneBuffers::ModelMatrices))
		};
		bufferWrite.emplace_back()
			.setDstSet(sets.matrixDescriptor.get())
			.setDstBinding(0)
			.setDescriptorType(vk::DescriptorType::eUniformBufferDynamic)
			.setBufferInfo(matricesBufferInfo);

		// Init Textures in the scene -- Get combined sampler
		// TODO
		vk::DescriptorImageInfo sceneTextureImageInfo{};
		sceneTextureImageInfo.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
		sceneTextureImageInfo.imageView = buffers._textureImages[0].imageView.get();
		sceneTextureImageInfo.sampler = buffers._textureImages[0].sampler.get();
		bufferWrite.emplace_back()
			.setDstSet(sets.tempTexture.get())
			.setDstBinding(0)
			.setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
			.setImageInfo(sceneTextureImageInfo);
		/*
		if (sceneBuffers->_textureImages.size() > 0) {
			
		}
		*/
		device.get().updateDescriptorSets(bufferWrite, {});
	}

	const nvh::GltfScene *scene = nullptr;
	const SceneBuffers *sceneBuffers = nullptr;
	const Resources *descriptorSets;
protected:
	explicit GBufferPass(vk::Extent2D extent) : _bufferExtent(extent) {
	}

	vk::Extent2D _bufferExtent;
	Shader _vert, _frag;
	vk::UniqueDescriptorSetLayout _uniformsDescriptorSetLayout;
	vk::UniqueDescriptorSetLayout _matricesDescriptorSetLayout;
	vk::UniqueDescriptorSetLayout _textureDescriptorSetLayout;
	vk::UniqueDescriptorSetLayout _sceneTexturesDescriptorSetLayout;
	vk::UniquePipelineLayout _pipelineLayout;
	// SceneBuffers& _scenebuffers;

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
	

	void _initialize(vk::Device dev, SceneBuffers* i_scene_buffer) override {
		_vert = Shader::load(dev, "shaders/gBuffer.vert.spv", "main", vk::ShaderStageFlagBits::eVertex);
		_frag = Shader::load(dev, "shaders/gBuffer.frag.spv", "main", vk::ShaderStageFlagBits::eFragment);

		std::array<vk::DescriptorSetLayoutBinding, 1> uniformsDescriptorBindings{
			vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eVertex)
		};
		vk::DescriptorSetLayoutCreateInfo uniformsDescriptorSetInfo;
		uniformsDescriptorSetInfo.setBindings(uniformsDescriptorBindings);
		_uniformsDescriptorSetLayout = dev.createDescriptorSetLayoutUnique(uniformsDescriptorSetInfo);

		std::array<vk::DescriptorSetLayoutBinding, 1> matricesDescriptorBindings{
			vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eUniformBufferDynamic, 1, vk::ShaderStageFlagBits::eVertex)
		};
		vk::DescriptorSetLayoutCreateInfo matricesDescriptorSetInfo;
		matricesDescriptorSetInfo.setBindings(matricesDescriptorBindings);
		_matricesDescriptorSetLayout = dev.createDescriptorSetLayoutUnique(matricesDescriptorSetInfo);

		std::array<vk::DescriptorSetLayoutBinding, 1> textureDescriptorBindings{
			vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment)
		};
		vk::DescriptorSetLayoutCreateInfo textureDescriptorSetInfo;
		textureDescriptorSetInfo.setBindings(textureDescriptorBindings);
		_textureDescriptorSetLayout = dev.createDescriptorSetLayoutUnique(textureDescriptorSetInfo);

		std::vector<vk::DescriptorSetLayout> descriptorSetLayouts{
			_uniformsDescriptorSetLayout.get(),
			_matricesDescriptorSetLayout.get(),
			_textureDescriptorSetLayout.get()
		};

		sceneBuffers = i_scene_buffer;

		// Scene textures -- Temporily only one texture:
		vk::DescriptorSetLayoutBinding sceneTextureSamplerLayoutBinding{};
		sceneTextureSamplerLayoutBinding.binding = 0;
		sceneTextureSamplerLayoutBinding.descriptorCount = 1;
		sceneTextureSamplerLayoutBinding.descriptorType = vk::DescriptorType::eCombinedImageSampler;
		sceneTextureSamplerLayoutBinding.pImmutableSamplers = nullptr;
		sceneTextureSamplerLayoutBinding.stageFlags = vk::ShaderStageFlagBits::eFragment;
		std::array<vk::DescriptorSetLayoutBinding, 1> sceneTextureDescriptorBindings;
		sceneTextureDescriptorBindings[0] = std::move(sceneTextureSamplerLayoutBinding);
		vk::DescriptorSetLayoutCreateInfo sceneTextureLayoutInfo;
		sceneTextureLayoutInfo.setBindings(sceneTextureDescriptorBindings);
		_sceneTexturesDescriptorSetLayout = dev.createDescriptorSetLayoutUnique(sceneTextureLayoutInfo);
		descriptorSetLayouts.push_back(_sceneTexturesDescriptorSetLayout.get());

		/*
		if (sceneBuffers->_textureImages.size() > 0) {
			
		}
		*/
		/*
		std::array<vk::DescriptorSetLayout, 4> descriptorSetLayouts{
			_uniformsDescriptorSetLayout.get(),
			_matricesDescriptorSetLayout.get(),
			_textureDescriptorSetLayout.get(),
			_sceneTexturesDescriptorSetLayout.get()
		};
		*/

		// Push constants in the fragment shader
		vk::PushConstantRange pushConstantRanges = { vk::ShaderStageFlagBits::eFragment, 0, sizeof(nvh::GltfMaterial) };

		vk::PipelineLayoutCreateInfo pipelineInfo;
		pipelineInfo
			.setSetLayouts(descriptorSetLayouts)
			.setPushConstantRangeCount(1)
			.setPPushConstantRanges(&pushConstantRanges);
		_pipelineLayout = dev.createPipelineLayoutUnique(pipelineInfo);

		Pass::_initialize(dev);
	}


	void _initialize(vk::Device dev) override {
		_vert = Shader::load(dev, "shaders/gBuffer.vert.spv", "main", vk::ShaderStageFlagBits::eVertex);
		_frag = Shader::load(dev, "shaders/gBuffer.frag.spv", "main", vk::ShaderStageFlagBits::eFragment);

		std::array<vk::DescriptorSetLayoutBinding, 1> uniformsDescriptorBindings{
			vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eVertex)
		};
		vk::DescriptorSetLayoutCreateInfo uniformsDescriptorSetInfo;
		uniformsDescriptorSetInfo.setBindings(uniformsDescriptorBindings);
		_uniformsDescriptorSetLayout = dev.createDescriptorSetLayoutUnique(uniformsDescriptorSetInfo);

		std::array<vk::DescriptorSetLayoutBinding, 1> matricesDescriptorBindings{
			vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eUniformBufferDynamic, 1, vk::ShaderStageFlagBits::eVertex)
		};
		vk::DescriptorSetLayoutCreateInfo matricesDescriptorSetInfo;
		matricesDescriptorSetInfo.setBindings(matricesDescriptorBindings);
		_matricesDescriptorSetLayout = dev.createDescriptorSetLayoutUnique(matricesDescriptorSetInfo);

		std::array<vk::DescriptorSetLayoutBinding, 1> textureDescriptorBindings{
			vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment)
		};
		vk::DescriptorSetLayoutCreateInfo textureDescriptorSetInfo;
		textureDescriptorSetInfo.setBindings(textureDescriptorBindings);
		_textureDescriptorSetLayout = dev.createDescriptorSetLayoutUnique(textureDescriptorSetInfo);

		// Scene textures:


		std::array<vk::DescriptorSetLayout, 3> descriptorSetLayouts{
			_uniformsDescriptorSetLayout.get(),
			_matricesDescriptorSetLayout.get(),
			_textureDescriptorSetLayout.get()
		};
		vk::PipelineLayoutCreateInfo pipelineInfo;
		pipelineInfo
			.setSetLayouts(descriptorSetLayouts);
		_pipelineLayout = dev.createPipelineLayoutUnique(pipelineInfo);

		Pass::_initialize(dev);
	}
};
