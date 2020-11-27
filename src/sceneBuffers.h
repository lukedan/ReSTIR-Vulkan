#pragma once

#include <nvmath_glsltypes.h>

#include "vertex.h"
#include "vma.h"
#include "../gltf/gltfscene.h"
#include "transientCommandBuffer.h"

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
	[[nodiscard]] const vk::Buffer getPtLights() const {
		return _ptLightsBuffer.get();
	}
	[[nodiscard]] const vk::Buffer getTriLights() const {
		return _triLightsBuffer.get();
	}
	[[nodiscard]] const std::vector<SceneTexture> &getTextures() const {
		return _textureImages;
	}
	[[nodiscard]] const SceneTexture &getDefaultNormal() const {
		return _defaultNormal;
	}
	[[nodiscard]] const SceneTexture &getDefaultAlbedo() const {
		return _defaultAlbedo;
	}
	[[nodiscard]] const vk::DeviceSize getPtLightsBufferSize() const {
		return _ptLightsBufferSize;
	}
	[[nodiscard]] const vk::DeviceSize getTriLightsBufferSize() const {
		return _triLightsBufferSize;
	}
	

	[[nodiscard]] static SceneBuffers create(
		const nvh::GltfScene &scene,
		vma::Allocator &allocator,
		TransientCommandBufferPool &oneTimeBufferPool,
		vk::PhysicalDevice p_device,
		vk::Device l_device,
		vk::Queue graphicsQueue,
		vk::CommandPool commandPool
	) {
		SceneBuffers result;

		result._vertices = allocator.createTypedBuffer<Vertex>(
			scene.m_positions.size(), vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress, VMA_MEMORY_USAGE_CPU_TO_GPU
			);
		result._indices = allocator.createTypedBuffer<int32_t>(
			scene.m_indices.size(), vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress, VMA_MEMORY_USAGE_CPU_TO_GPU
			);
		result._matrices = allocator.createTypedBuffer<ModelMatrices>(
			scene.m_nodes.size(), vk::BufferUsageFlagBits::eUniformBuffer, VMA_MEMORY_USAGE_CPU_TO_GPU
			);
		result._materials = allocator.createTypedBuffer<nvh::GltfMaterial>(
			scene.m_materials.size(), vk::BufferUsageFlagBits::eStorageBuffer, VMA_MEMORY_USAGE_CPU_TO_GPU
			);
		// Lights
		// Point lights
		result._ptLightsBufferSize = 16 + sizeof(shader::pointLight) * scene.m_pointLights.size();
		result._ptLightsBuffer = allocator.createBuffer(
			result._ptLightsBufferSize, vk::BufferUsageFlagBits::eStorageBuffer, VMA_MEMORY_USAGE_CPU_TO_GPU
		);
		// Triangle lights
		result._triLightsBufferSize = 16 + sizeof(shader::triLight) * scene.m_triLights.size();
		result._triLightsBuffer = allocator.createBuffer(
			result._triLightsBufferSize, vk::BufferUsageFlagBits::eStorageBuffer, VMA_MEMORY_USAGE_CPU_TO_GPU
		);

		vk::Format format = vk::Format::eR8G8B8A8Unorm;

		result._textureImages.resize(scene.m_textures.size());

		// load textures
		for (int i = 0; i < scene.m_textures.size(); ++i) {
			auto& gltfimage = scene.m_textures[i];
			std::cout << "Loading Texture: " << gltfimage.uri << std::endl;

			// Create vma::Uniqueimage
			uint32_t numMipLevels = 1 + static_cast<uint32_t>(std::ceil(std::log2(
				std::max(gltfimage.width, gltfimage.height)
			)));
			result._textureImages[i].image = loadTexture(
				gltfimage, format, numMipLevels, allocator, oneTimeBufferPool, graphicsQueue
			);

			result._textureImages[i].sampler = createSampler(
				l_device, vk::Filter::eLinear, vk::Filter::eLinear, vk::SamplerMipmapMode::eLinear, 16.0f
			);
			result._textureImages[i].imageView = createImageView2D(
				l_device, result._textureImages[i].image.get(),
				format, vk::ImageAspectFlagBits::eColor, 0, numMipLevels
			);
		}

		// generate default textures
		{
			unsigned char defaultNormal[4]{ 127, 127, 255, 255 };
			result._defaultNormal.image = loadTexture(
				defaultNormal, 1, 1, vk::Format::eR8G8B8A8Unorm, 1, allocator, oneTimeBufferPool, graphicsQueue
			);
			result._defaultNormal.sampler = createSampler(l_device);
			result._defaultNormal.imageView = createImageView2D(
				l_device, result._defaultNormal.image.get(), format, vk::ImageAspectFlagBits::eColor
			);

			unsigned char defaultAlbedo[4]{ 255, 255, 255, 255 };
			result._defaultAlbedo.image = loadTexture(
				defaultAlbedo, 1, 1, vk::Format::eR8G8B8A8Unorm, 1, allocator, oneTimeBufferPool, graphicsQueue
			);
			result._defaultAlbedo.sampler = createSampler(l_device);
			result._defaultAlbedo.imageView = createImageView2D(
				l_device, result._defaultAlbedo.image.get(), format, vk::ImageAspectFlagBits::eColor
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

		// Lights
		// Point lights
		int32_t* pointLightPtr = result._ptLightsBuffer.mapAs<int32_t>();
		*pointLightPtr = scene.m_ptLightsNum;
		auto* ptLights = reinterpret_cast<shader::pointLight*>(reinterpret_cast<uintptr_t>(pointLightPtr) + 16);
		std::memcpy(ptLights, scene.m_pointLights.data(), sizeof(shader::pointLight) * scene.m_pointLights.size());
		result._ptLightsBuffer.unmap();
		result._ptLightsBuffer.flush();
		
		// Tri lights
		int32_t* triLightsPtr = result._triLightsBuffer.mapAs<int32_t>();
		*triLightsPtr = scene.m_triLightsNum;
		auto* triLights = reinterpret_cast<shader::triLight*>(reinterpret_cast<uintptr_t>(triLightsPtr) + 16);
		std::memcpy(triLights, scene.m_triLights.data(), sizeof(shader::triLight) * scene.m_triLights.size());
		result._triLightsBuffer.unmap();
		result._triLightsBuffer.flush();

		return result;
	}
private:
	vma::UniqueBuffer _vertices;
	vma::UniqueBuffer _indices;
	vma::UniqueBuffer _matrices;
	vma::UniqueBuffer _materials;
	vma::UniqueBuffer _ptLightsBuffer;
	vma::UniqueBuffer _triLightsBuffer;
	std::vector<SceneTexture> _textureImages;
	SceneTexture _defaultNormal;
	SceneTexture _defaultAlbedo;
	vk::DeviceSize _ptLightsBufferSize;
	vk::DeviceSize _triLightsBufferSize;
};
