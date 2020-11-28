#pragma once

#include <nvmath_glsltypes.h>

#include <gltfscene.h>

#include "vertex.h"
#include "vma.h"
#include "transientCommandBuffer.h"
#include "shaderIncludes.h"

class SceneBuffers {
public:
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
	[[nodiscard]] vk::Buffer getMaterials() const {
		return _materials.get();
	}
	[[nodiscard]] const std::vector<SceneTexture> &getTextures() const {
		return _textureImages;
	}
	[[nodiscard]] const SceneTexture &getDefaultNormal() const {
		return _defaultNormal;
	}
	[[nodiscard]] const SceneTexture &getDefaultWhite() const {
		return _defaultWhite;
	}

	[[nodiscard]] static SceneBuffers create(
		const nvh::GltfScene &scene,
		vma::Allocator &allocator,
		TransientCommandBufferPool &oneTimeBufferPool,
		vk::Device l_device,
		vk::Queue graphicsQueue
	) {
		SceneBuffers result;

		result._vertices = allocator.createTypedBuffer<Vertex>(
			scene.m_positions.size(), vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress, VMA_MEMORY_USAGE_CPU_TO_GPU
			);
		result._indices = allocator.createTypedBuffer<int32_t>(
			scene.m_indices.size(), vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress, VMA_MEMORY_USAGE_CPU_TO_GPU
			);
		result._matrices = allocator.createTypedBuffer<shader::ModelMatrices>(
			scene.m_nodes.size(), vk::BufferUsageFlagBits::eUniformBuffer, VMA_MEMORY_USAGE_CPU_TO_GPU
			);
		result._materials = allocator.createTypedBuffer<shader::MaterialUniforms>(
			scene.m_materials.size(), vk::BufferUsageFlagBits::eUniformBuffer, VMA_MEMORY_USAGE_CPU_TO_GPU
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

			unsigned char defaultWhite[4]{ 255, 255, 255, 255 };
			result._defaultWhite.image = loadTexture(
				defaultWhite, 1, 1, vk::Format::eR8G8B8A8Unorm, 1, allocator, oneTimeBufferPool, graphicsQueue
			);
			result._defaultWhite.sampler = createSampler(l_device);
			result._defaultWhite.imageView = createImageView2D(
				l_device, result._defaultWhite.image.get(), format, vk::ImageAspectFlagBits::eColor
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


		auto *mat_device = result._materials.mapAs<shader::MaterialUniforms>();
		for (std::size_t i = 0; i < scene.m_materials.size(); ++i) {
			const nvh::GltfMaterial &mat = scene.m_materials[i];
			shader::MaterialUniforms &outMat = mat_device[i];

			outMat.emissiveFactor = mat.emissiveFactor;
			outMat.shadingModel = mat.shadingModel;
			outMat.alphaMode = mat.alphaMode;
			outMat.alphaCutoff = mat.alphaCutoff;
			outMat.normalTextureScale = mat.normalTextureScale;

			switch (outMat.shadingModel) {
			case SHADING_MODEL_METALLIC_ROUGHNESS:
				outMat.colorParam = mat.pbrBaseColorFactor;
				outMat.materialParam.y = mat.pbrRoughnessFactor;
				outMat.materialParam.z = mat.pbrMetallicFactor;
				break;
			case SHADING_MODEL_SPECULAR_GLOSSINESS:
				outMat.colorParam = mat.khrDiffuseFactor;
				outMat.materialParam = mat.khrSpecularFactor;
				outMat.materialParam.w = mat.khrGlossinessFactor;
				break;
			}
		}
		result._materials.unmap();
		result._materials.flush();


		auto *matrices = result._matrices.mapAs<shader::ModelMatrices>();
		for (std::size_t i = 0; i < scene.m_nodes.size(); ++i) {
			matrices[i].transform = scene.m_nodes[i].worldMatrix;
			matrices[i].transformInverseTransposed = nvmath::transpose(nvmath::invert(matrices[i].transform));
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
	SceneTexture _defaultWhite;
};
