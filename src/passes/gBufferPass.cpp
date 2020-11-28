#include "gBufferPass.h"

#include <cassert>

bool _formatsInitialized = false;
GBuffer::Formats _gBufferFormats;

void GBuffer::resize(vma::Allocator &allocator, vk::Device device, vk::Extent2D extent, Pass &pass) {
	_framebuffer.reset();

	_albedoView.reset();
	_normalView.reset();
	_depthView.reset();

	_albedoBuffer.reset();
	_normalBuffer.reset();
	_depthBuffer.reset();

	const Formats &formats = Formats::get();

	_albedoBuffer = allocator.createImage2D(
		extent, formats.albedo,
		vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled
	);
	_normalBuffer = allocator.createImage2D(
		extent, formats.normal,
		vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled
	);
	_materialPropertiesBuffer = allocator.createImage2D(
		extent, formats.materialProperties,
		vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled
	);
	_depthBuffer = allocator.createImage2D(
		extent, formats.depth,
		vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled
	);

	_albedoView = createImageView2D(
		device, _albedoBuffer.get(), formats.albedo, vk::ImageAspectFlagBits::eColor
	);
	_normalView = createImageView2D(
		device, _normalBuffer.get(), formats.normal, vk::ImageAspectFlagBits::eColor
	);
	_materialPropertiesView = createImageView2D(
		device, _materialPropertiesBuffer.get(), formats.materialProperties, vk::ImageAspectFlagBits::eColor
	);
	_depthView = createImageView2D(
		device, _depthBuffer.get(), formats.depth, formats.depthAspect
	);

	std::array<vk::ImageView, 4> attachments{
		_albedoView.get(), _normalView.get(), _materialPropertiesView.get(), _depthView.get()
	};
	vk::FramebufferCreateInfo framebufferInfo;
	framebufferInfo
		.setRenderPass(pass.getPass())
		.setAttachments(attachments)
		.setWidth(extent.width)
		.setHeight(extent.height)
		.setLayers(1);
	_framebuffer = device.createFramebufferUnique(framebufferInfo);
}

void GBuffer::Formats::initialize(vk::PhysicalDevice physicalDevice) {
	assert(!_formatsInitialized);
	_gBufferFormats.albedo = findSupportedFormat(
		{ vk::Format::eR8G8B8A8Srgb },
		physicalDevice, vk::ImageTiling::eOptimal, vk::FormatFeatureFlagBits::eColorAttachment
	);
	_gBufferFormats.normal = findSupportedFormat(
		{
			vk::Format::eR16G16B16Snorm,
			vk::Format::eR16G16B16Sfloat,
			vk::Format::eR16G16B16A16Snorm,
			vk::Format::eR16G16B16A16Sfloat,
			vk::Format::eR32G32B32Sfloat
		},
		physicalDevice, vk::ImageTiling::eOptimal, vk::FormatFeatureFlagBits::eColorAttachment
	);
	_gBufferFormats.depth = findSupportedFormat(
		{ vk::Format::eD32Sfloat, vk::Format::eD32SfloatS8Uint, vk::Format::eD24UnormS8Uint },
		physicalDevice, vk::ImageTiling::eOptimal, vk::FormatFeatureFlagBits::eDepthStencilAttachment
	);
	_gBufferFormats.materialProperties = findSupportedFormat(
		{ vk::Format::eR16G16Unorm },
		physicalDevice, vk::ImageTiling::eOptimal, vk::FormatFeatureFlagBits::eColorAttachment
	);
	_gBufferFormats.depthAspect = vk::ImageAspectFlagBits::eDepth;
	if (_gBufferFormats.depth != vk::Format::eD32Sfloat) {
		_gBufferFormats.depthAspect |= vk::ImageAspectFlagBits::eStencil;
	}
	_formatsInitialized = true;
}

const GBuffer::Formats &GBuffer::Formats::get() {
	assert(_formatsInitialized);
	return _gBufferFormats;
}


void GBufferPass::issueCommands(vk::CommandBuffer commandBuffer, vk::Framebuffer framebuffer) const {
	std::array<vk::ClearValue, 4> clearValues{
		vk::ClearColorValue(std::array<float, 4>{ 0.0f, 0.0f, 0.0f, 1.0f }),
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

	for (std::size_t i = 0; i < scene->m_nodes.size(); ++i) {
		const nvh::GltfNode &node = scene->m_nodes[i];
		const nvh::GltfPrimMesh &mesh = scene->m_primMeshes[node.primMesh];

		commandBuffer.bindDescriptorSets(
			vk::PipelineBindPoint::eGraphics, _pipelineLayout.get(), 0,
			{
				descriptorSets->uniformDescriptor.get(),
				descriptorSets->matrixDescriptor.get(),
				descriptorSets->materialDescriptor.get(),
				descriptorSets->materialTexturesDescriptors[mesh.materialIndex].get()
			},
				{
					static_cast<uint32_t>(i * sizeof(shader::ModelMatrices)),
					static_cast<uint32_t>(mesh.materialIndex * sizeof(shader::MaterialUniforms))
				}
				);
		commandBuffer.drawIndexed(mesh.indexCount, 1, mesh.firstIndex, mesh.vertexOffset, 0);
	}

	commandBuffer.endRenderPass();
}

void GBufferPass::initializeResourcesFor(
	const nvh::GltfScene &targetScene, const SceneBuffers &buffers, vk::UniqueDevice& device, Resources &sets
) {
	std::vector<vk::WriteDescriptorSet> bufferWrite;
	bufferWrite.reserve(2 + 3 * targetScene.m_materials.size());

	std::array<vk::DescriptorBufferInfo, 1> uniformBufferInfo{
		vk::DescriptorBufferInfo(sets.uniformBuffer.get(), 0, sizeof(Uniforms))
	};
	bufferWrite.emplace_back()
		.setDstSet(sets.uniformDescriptor.get())
		.setDstBinding(0)
		.setDescriptorType(vk::DescriptorType::eUniformBuffer)
		.setBufferInfo(uniformBufferInfo);

	std::array<vk::DescriptorBufferInfo, 1> matricesBufferInfo{
		vk::DescriptorBufferInfo(buffers.getMatrices(), 0, sizeof(shader::ModelMatrices))
	};
	bufferWrite.emplace_back()
		.setDstSet(sets.matrixDescriptor.get())
		.setDstBinding(0)
		.setDescriptorType(vk::DescriptorType::eUniformBufferDynamic)
		.setBufferInfo(matricesBufferInfo);

	std::array<vk::DescriptorBufferInfo, 1> materialsBufferInfo{
		vk::DescriptorBufferInfo(buffers.getMaterials(), 0, sizeof(shader::MaterialUniforms))
	};
	bufferWrite.emplace_back()
		.setDstSet(sets.materialDescriptor.get())
		.setDstBinding(0)
		.setDescriptorType(vk::DescriptorType::eUniformBufferDynamic)
		.setBufferInfo(materialsBufferInfo);

	std::vector<vk::DescriptorImageInfo> materialTextureInfo(buffers.getTextures().size());
	vk::DescriptorImageInfo defaultNormalInfo = buffers.getDefaultNormal().getDescriptorInfo();
	vk::DescriptorImageInfo defaultWhiteInfo = buffers.getDefaultWhite().getDescriptorInfo();
	for (std::size_t i = 0; i < buffers.getTextures().size(); ++i) {
		materialTextureInfo[i] = buffers.getTextures()[i].getDescriptorInfo();
	}
	for (std::size_t i = 0; i < targetScene.m_materials.size(); ++i) {
		vk::DescriptorSet set = sets.materialTexturesDescriptors[i].get();
		const nvh::GltfMaterial &mat = targetScene.m_materials[i];

		bufferWrite.emplace_back()
			.setDstSet(set)
			.setDstBinding(0)
			.setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
			.setImageInfo(mat.pbrBaseColorTexture >= 0 ? materialTextureInfo[mat.pbrBaseColorTexture] : defaultWhiteInfo);
		bufferWrite.emplace_back()
			.setDstSet(set)
			.setDstBinding(1)
			.setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
			.setImageInfo(mat.normalTexture >= 0 ? materialTextureInfo[mat.normalTexture] : defaultNormalInfo);
		switch (mat.shadingModel) {
		case SHADING_MODEL_METALLIC_ROUGHNESS:
			bufferWrite.emplace_back()
				.setDstSet(set)
				.setDstBinding(2)
				.setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
				.setImageInfo(mat.pbrMetallicRoughnessTexture >= 0 ? materialTextureInfo[mat.pbrMetallicRoughnessTexture] : defaultWhiteInfo);
			break;
		case SHADING_MODEL_SPECULAR_GLOSSINESS:
			bufferWrite.emplace_back()
				.setDstSet(set)
				.setDstBinding(2)
				.setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
				.setImageInfo(mat.khrSpecularGlossinessTexture >= 0 ? materialTextureInfo[mat.khrSpecularGlossinessTexture] : defaultWhiteInfo);
			break;
		}
	}

	device.get().updateDescriptorSets(bufferWrite, {});
}

vk::UniqueRenderPass GBufferPass::_createPass(vk::Device device) {
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
		.setFormat(formats.materialProperties)
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

	std::array<vk::AttachmentReference, 3> colorAttachmentReferences{
		vk::AttachmentReference(0, vk::ImageLayout::eColorAttachmentOptimal),
		vk::AttachmentReference(1, vk::ImageLayout::eColorAttachmentOptimal),
		vk::AttachmentReference(2, vk::ImageLayout::eColorAttachmentOptimal)
	};

	vk::AttachmentReference depthAttachmentReference;
	depthAttachmentReference
		.setAttachment(3)
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

std::vector<Pass::PipelineCreationInfo> GBufferPass::_getPipelineCreationInfo() {
	std::vector<PipelineCreationInfo> result;

	GraphicsPipelineCreationInfo info;

	info.vertexInputBindingStorage.emplace_back(0, static_cast<uint32_t>(sizeof(Vertex)), vk::VertexInputRate::eVertex);
	info.vertexInputAttributeStorage.emplace_back(0, 0, vk::Format::eR32G32B32Sfloat, static_cast<uint32_t>(offsetof(Vertex, position)));
	info.vertexInputAttributeStorage.emplace_back(1, 0, vk::Format::eR32G32B32Sfloat, static_cast<uint32_t>(offsetof(Vertex, normal)));
	info.vertexInputAttributeStorage.emplace_back(2, 0, vk::Format::eR32G32B32A32Sfloat, static_cast<uint32_t>(offsetof(Vertex, tangent)));
	info.vertexInputAttributeStorage.emplace_back(3, 0, vk::Format::eR32G32B32A32Sfloat, static_cast<uint32_t>(offsetof(Vertex, color)));
	info.vertexInputAttributeStorage.emplace_back(4, 0, vk::Format::eR32G32Sfloat, static_cast<uint32_t>(offsetof(Vertex, uv)));
	info.vertexInputState
		.setVertexBindingDescriptions(info.vertexInputBindingStorage)
		.setVertexAttributeDescriptions(info.vertexInputAttributeStorage);

	info.inputAssemblyState = GraphicsPipelineCreationInfo::getTriangleListWithoutPrimitiveRestartInputAssembly();

	info.viewportStorage.emplace_back(
		0.0f, 0.0f, static_cast<float>(_bufferExtent.width), static_cast<float>(_bufferExtent.height), 0.0f, 1.0f
	);
	info.scissorStorage.emplace_back(vk::Offset2D(0, 0), _bufferExtent);
	info.viewportState
		.setViewports(info.viewportStorage)
		.setScissors(info.scissorStorage);

	info.rasterizationState = GraphicsPipelineCreationInfo::getDefaultRasterizationState();

	info.depthStencilState = GraphicsPipelineCreationInfo::getDefaultDepthTestState();

	info.multisampleState = GraphicsPipelineCreationInfo::getNoMultisampleState();

	info.attachmentColorBlendStorage.emplace_back(GraphicsPipelineCreationInfo::getNoBlendAttachment());
	info.attachmentColorBlendStorage.emplace_back(GraphicsPipelineCreationInfo::getNoBlendAttachment());
	info.attachmentColorBlendStorage.emplace_back(GraphicsPipelineCreationInfo::getNoBlendAttachment());
	info.colorBlendState.setAttachments(info.attachmentColorBlendStorage);

	info.shaderStages.emplace_back(_frag.getStageInfo());
	info.shaderStages.emplace_back(_vert.getStageInfo());

	info.pipelineLayout = _pipelineLayout.get();

	result.emplace_back(std::move(info));

	return result;
}

void GBufferPass::_initialize(vk::Device dev) {
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

	std::array<vk::DescriptorSetLayoutBinding, 1> materialDescriptorBindings{
		vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eUniformBufferDynamic, 1, vk::ShaderStageFlagBits::eFragment)
	};
	vk::DescriptorSetLayoutCreateInfo materialDescriptorSetInfo;
	materialDescriptorSetInfo.setBindings(materialDescriptorBindings);
	_materialDescriptorSetLayout = dev.createDescriptorSetLayoutUnique(materialDescriptorSetInfo);

	std::array<vk::DescriptorSetLayoutBinding, 3> textureDescriptorBindings{
		vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment),
		vk::DescriptorSetLayoutBinding(1, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment),
		vk::DescriptorSetLayoutBinding(2, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment)
	};
	vk::DescriptorSetLayoutCreateInfo textureDescriptorSetInfo;
	textureDescriptorSetInfo.setBindings(textureDescriptorBindings);
	_textureDescriptorSetLayout = dev.createDescriptorSetLayoutUnique(textureDescriptorSetInfo);

	std::array<vk::DescriptorSetLayout, 4> descriptorSetLayouts{
		_uniformsDescriptorSetLayout.get(),
		_matricesDescriptorSetLayout.get(),
		_materialDescriptorSetLayout.get(),
		_textureDescriptorSetLayout.get()
	};

	vk::PipelineLayoutCreateInfo pipelineInfo;
	pipelineInfo
		.setSetLayouts(descriptorSetLayouts);
	_pipelineLayout = dev.createPipelineLayoutUnique(pipelineInfo);

	Pass::_initialize(dev);
}
