#pragma once

#include <nvmath_glsltypes.h>

#include "shaderStructs/vertex.h"
#include "vma.h"
#include "../gltf/gltfscene.h"

class SceneBuffers {
public:
	struct ModelMatrices {
		nvmath::mat4 transform;
		nvmath::mat4 inverseTransposed;
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

	[[nodiscard]] inline static SceneBuffers create(const nvh::GltfScene &scene, vma::Allocator &allocator) {
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
		}
		result._vertices.unmap();
		result._vertices.flush();

		uint32_t *indices = result._indices.mapAs<uint32_t>();
		for (std::size_t i = 0; i < scene.m_indices.size(); ++i) {
			indices[i] = scene.m_indices[i];
		}
		result._indices.unmap();
		result._indices.flush();

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
};
