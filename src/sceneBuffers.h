#pragma once

#include <nvmath_glsltypes.h>

#include "vertex.h"
#include "vma.h"
#include "../gltf/gltfscene.h"

class SceneBuffers {
public:
	struct ModelMatrices {
		nvmath::mat4 transform;
		nvmath::mat4 inverseTransposed;
	};

	struct SceneTexture {
		vk::UniqueImageView imageView;
		vk::UniqueSampler sampler;
		vma::UniqueImage image;

		[[nodiscard]] vk::DescriptorImageInfo getDescriptorInfo(
			vk::ImageLayout layout = vk::ImageLayout::eShaderReadOnlyOptimal
		) const {
			return vk::DescriptorImageInfo(sampler.get(), imageView.get(), layout);
		}
	};

	[[nodiscard]] vk::Buffer getVertices() const {
		return _vertices.get();
	}
	[[nodiscard]] vk::Buffer getIndices() const {
		return _indices.get();
	}
	[[nodiscard]] vk::Buffer getMatrices() const {
		return _matrices.get();
	}
	[[nodiscard]] const std::vector<SceneTexture> &getTextures() const {
		return _textureImages;
	}
	[[nodiscard]] const SceneTexture &getDefaultNormal() const {
		return _defaultNormal;
	}

	[[nodiscard]] static SceneBuffers create(const nvh::GltfScene &scene, 
		vma::Allocator &allocator, 
		const vk::PhysicalDevice& p_device, 
		vk::Device l_device,
		vk::Queue& graphicsQueue,
		vk::CommandPool commandPool
		) {
		SceneBuffers result;

		result._vertices = allocator.createTypedBuffer<Vertex>(
			scene.m_positions.size(), vk::BufferUsageFlagBits::eVertexBuffer, VMA_MEMORY_USAGE_CPU_TO_GPU
			);
		result._indices = allocator.createTypedBuffer<int32_t>(
			scene.m_indices.size(), vk::BufferUsageFlagBits::eIndexBuffer, VMA_MEMORY_USAGE_CPU_TO_GPU
			);
		result._matrices = allocator.createTypedBuffer<ModelMatrices>(
			scene.m_nodes.size(), vk::BufferUsageFlagBits::eUniformBuffer, VMA_MEMORY_USAGE_CPU_TO_GPU
			);
		result._materials = allocator.createTypedBuffer<nvh::GltfMaterial>(
			scene.m_materials.size(), vk::BufferUsageFlagBits::eStorageBuffer, VMA_MEMORY_USAGE_CPU_TO_GPU
			);

		// Create vma::imageunique
		vk::UniqueCommandBuffer commandBuffer = std::move(l_device.
			allocateCommandBuffersUnique(vk::CommandBufferAllocateInfo(
				commandPool, vk::CommandBufferLevel::ePrimary, 1))
			.front());

		vk::Format format = vk::Format::eR8G8B8A8Unorm;
		
		result._textureImages.resize(scene.m_textures.size());
		vk::CommandBufferBeginInfo cmdBeginInfo;
		cmdBeginInfo.setFlags({ vk::CommandBufferUsageFlagBits::eOneTimeSubmit });

		for (int i = 0; i < scene.m_textures.size(); ++i) {
			auto& gltfimage = scene.m_textures[i];
			std::cout << "Loaded Texture: " << gltfimage.uri << std::endl;

			// Create vma::Uniqueimage
			result._textureImages[i].image = allocator.createImage2D(
				vk::Extent2D(gltfimage.width, gltfimage.height),
				format,
				vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst,
				VMA_MEMORY_USAGE_CPU_TO_GPU,
				vk::ImageTiling::eLinear,
				vk::ImageLayout::ePreinitialized
			);

			// Map GPU memory to RAM and put data from RAM to GPU
			void* image_device = result._textureImages[i].image.map();
			memcpy(image_device, gltfimage.image.data(), gltfimage.image.size() * sizeof(unsigned char));
			// Unmap and flush
			result._textureImages[i].image.unmap();
			result._textureImages[i].image.flush();

			commandBuffer->begin(cmdBeginInfo);
			setImageLayout(commandBuffer, result._textureImages[i].image.get(), format, vk::ImageLayout::ePreinitialized, vk::ImageLayout::eShaderReadOnlyOptimal);
			commandBuffer->end();
			submitAndWait(l_device, graphicsQueue, commandBuffer);
				
			result._textureImages[i].sampler = createSampler(l_device);
			result._textureImages[i].imageView = createImageView2D(
				l_device, result._textureImages[i].image.get(), format, vk::ImageAspectFlagBits::eColor
			);
		}

		{// Generate default textures
			result._defaultNormal.image = allocator.createImage2D(
				vk::Extent2D(1, 1),
				format,
				vk::ImageUsageFlagBits::eSampled,
				VMA_MEMORY_USAGE_CPU_TO_GPU,
				vk::ImageTiling::eLinear,
				vk::ImageLayout::ePreinitialized
			);
			// Map GPU memory to RAM and put data from RAM to GPU
			unsigned char* image_device = result._defaultNormal.image.mapAs<unsigned char>();
			image_device[0] = 127;
			image_device[1] = 127;
			image_device[2] = 255;
			image_device[3] = 255;
			// Unmap and flush
			result._defaultNormal.image.unmap();
			result._defaultNormal.image.flush();

			commandBuffer->begin(vk::CommandBufferBeginInfo());
			setImageLayout(commandBuffer, result._defaultNormal.image.get(), format, vk::ImageLayout::ePreinitialized, vk::ImageLayout::eShaderReadOnlyOptimal);
			commandBuffer->end();
			submitAndWait(l_device, graphicsQueue, commandBuffer);

			result._defaultNormal.sampler = createSampler(l_device);
			result._defaultNormal.imageView = createImageView2D(
				l_device, result._defaultNormal.image.get(), format, vk::ImageAspectFlagBits::eColor
			);
		}


		// collect vertices
		Vertex *vertices = result._vertices.mapAs<Vertex>();
		for (std::size_t i = 0; i < scene.m_positions.size(); ++i) {
			Vertex &v = vertices[i];
			v.position = scene.m_positions[i];
			if (i < scene.m_normals.size()) {
				v.normal = scene.m_normals[i];
			}
			if (i < scene.m_colors0.size()) {
				v.color = scene.m_colors0[i];
			} else {
				v.color = nvmath::vec4(1.0f, 0.0f, 1.0f, 1.0f);
			}
			if (i < scene.m_texcoords0.size()) {
				v.uv = scene.m_texcoords0[i];
			}
			if (i < scene.m_tangents.size()) {
				v.tangent = scene.m_tangents[i];
			}
		}
		result._vertices.unmap();
		result._vertices.flush();

		uint32_t *indices = result._indices.mapAs<uint32_t>();
		for (std::size_t i = 0; i < scene.m_indices.size(); ++i) {
			indices[i] = scene.m_indices[i];
		}
		result._indices.unmap();
		result._indices.flush();


		nvh::GltfMaterial* mat_device = result._materials.mapAs<nvh::GltfMaterial>();
		for (std::size_t i = 0; i < scene.m_materials.size(); ++i) {
			mat_device[i] = scene.m_materials[i];
		}
		result._materials.unmap();
		result._materials.flush();
		

		ModelMatrices *matrices = result._matrices.mapAs<ModelMatrices>();
		for (std::size_t i = 0; i < scene.m_nodes.size(); ++i) {
			matrices[i].transform = scene.m_nodes[i].worldMatrix;
			matrices[i].inverseTransposed = nvmath::transpose(nvmath::invert(matrices[i].transform));
		}
		result._matrices.unmap();
		result._matrices.flush();


		return result;
	}
private:
	vma::UniqueBuffer _vertices;
	vma::UniqueBuffer _indices;
	vma::UniqueBuffer _matrices;
	vma::UniqueBuffer _materials;
	std::vector<SceneTexture> _textureImages;
	SceneTexture _defaultNormal;

	void static setImageLayout(vk::UniqueCommandBuffer const& commandBuffer,
		vk::Image                       image,
		vk::Format                      format,
		vk::ImageLayout                 oldImageLayout,
		vk::ImageLayout                 newImageLayout)
	{
		vk::AccessFlags sourceAccessMask;
		switch (oldImageLayout)
		{
		case vk::ImageLayout::eTransferDstOptimal: sourceAccessMask = vk::AccessFlagBits::eTransferWrite; break;
		case vk::ImageLayout::ePreinitialized: sourceAccessMask = vk::AccessFlagBits::eHostWrite; break;
		case vk::ImageLayout::eGeneral:  // sourceAccessMask is empty
		case vk::ImageLayout::eUndefined: break;
		default: assert(false); break;
		}

		vk::PipelineStageFlags sourceStage;
		switch (oldImageLayout)
		{
		case vk::ImageLayout::eGeneral:
		case vk::ImageLayout::ePreinitialized: sourceStage = vk::PipelineStageFlagBits::eHost; break;
		case vk::ImageLayout::eTransferDstOptimal: sourceStage = vk::PipelineStageFlagBits::eTransfer; break;
		case vk::ImageLayout::eUndefined: sourceStage = vk::PipelineStageFlagBits::eTopOfPipe; break;
		default: assert(false); break;
		}

		vk::AccessFlags destinationAccessMask;
		switch (newImageLayout)
		{
		case vk::ImageLayout::eColorAttachmentOptimal:
			destinationAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
			break;
		case vk::ImageLayout::eDepthStencilAttachmentOptimal:
			destinationAccessMask =
				vk::AccessFlagBits::eDepthStencilAttachmentRead | vk::AccessFlagBits::eDepthStencilAttachmentWrite;
			break;
		case vk::ImageLayout::eGeneral:  // empty destinationAccessMask
		case vk::ImageLayout::ePresentSrcKHR: break;
		case vk::ImageLayout::eShaderReadOnlyOptimal: destinationAccessMask = vk::AccessFlagBits::eShaderRead; break;
		case vk::ImageLayout::eTransferSrcOptimal: destinationAccessMask = vk::AccessFlagBits::eTransferRead; break;
		case vk::ImageLayout::eTransferDstOptimal: destinationAccessMask = vk::AccessFlagBits::eTransferWrite; break;
		default: assert(false); break;
		}

		vk::PipelineStageFlags destinationStage;
		switch (newImageLayout)
		{
		case vk::ImageLayout::eColorAttachmentOptimal:
			destinationStage = vk::PipelineStageFlagBits::eColorAttachmentOutput;
			break;
		case vk::ImageLayout::eDepthStencilAttachmentOptimal:
			destinationStage = vk::PipelineStageFlagBits::eEarlyFragmentTests;
			break;
		case vk::ImageLayout::eGeneral: destinationStage = vk::PipelineStageFlagBits::eHost; break;
		case vk::ImageLayout::ePresentSrcKHR: destinationStage = vk::PipelineStageFlagBits::eBottomOfPipe; break;
		case vk::ImageLayout::eShaderReadOnlyOptimal:
			destinationStage = vk::PipelineStageFlagBits::eFragmentShader;
			break;
		case vk::ImageLayout::eTransferDstOptimal:
		case vk::ImageLayout::eTransferSrcOptimal: destinationStage = vk::PipelineStageFlagBits::eTransfer; break;
		default: assert(false); break;
		}

		vk::ImageAspectFlags aspectMask;
		if (newImageLayout == vk::ImageLayout::eDepthStencilAttachmentOptimal)
		{
			aspectMask = vk::ImageAspectFlagBits::eDepth;
			if (format == vk::Format::eD32SfloatS8Uint || format == vk::Format::eD24UnormS8Uint)
			{
				aspectMask |= vk::ImageAspectFlagBits::eStencil;
			}
		}
		else
		{
			aspectMask = vk::ImageAspectFlagBits::eColor;
		}

		vk::ImageSubresourceRange imageSubresourceRange(aspectMask, 0, 1, 0, 1);
		vk::ImageMemoryBarrier    imageMemoryBarrier(sourceAccessMask,
			destinationAccessMask,
			oldImageLayout,
			newImageLayout,
			VK_QUEUE_FAMILY_IGNORED,
			VK_QUEUE_FAMILY_IGNORED,
			image,
			imageSubresourceRange);
		return commandBuffer->pipelineBarrier(sourceStage, destinationStage, {}, nullptr, nullptr, imageMemoryBarrier);
	}

	void static submitAndWait(vk::Device& device, vk::Queue queue, vk::UniqueCommandBuffer& commandBuffer)
	{
		vk::UniqueFence fence = device.createFenceUnique(vk::FenceCreateInfo());
		queue.submit(vk::SubmitInfo({}, {}, *commandBuffer), fence.get());
		while (vk::Result::eTimeout == device.waitForFences(fence.get(), VK_TRUE, std::numeric_limits<uint64_t>::max())) {
		}
	}
};
