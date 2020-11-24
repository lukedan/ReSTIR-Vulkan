#pragma once

#include <cmath>
#include <vector>

#include "shaderIncludes.h"
#include "../gltf/gltfscene.h"
#include "vma.h"

struct AabbTree {
	std::vector<shader::AabbTreeNode> nodes;
	std::vector<shader::Triangle> triangles;
	int32_t root;

	[[nodiscard]] static AabbTree build(const nvh::GltfScene&);
};

struct AabbTreeBuffers {
	vma::UniqueBuffer nodeBuffer;
	vma::UniqueBuffer triangleBuffer;
	vk::DeviceSize nodeBufferSize;
	vk::DeviceSize triangleBufferSize;

	[[nodiscard]] static AabbTreeBuffers create(const AabbTree &tree, vma::Allocator &allocator) {
		AabbTreeBuffers result;
		// due to alignment there are 12 bytes padding after the root node index
		result.nodeBufferSize = 16 + sizeof(shader::AabbTreeNode) * tree.nodes.size();
		result.nodeBuffer = allocator.createBuffer(
			result.nodeBufferSize, vk::BufferUsageFlagBits::eStorageBuffer, VMA_MEMORY_USAGE_CPU_TO_GPU
		);

		result.triangleBufferSize = sizeof(shader::Triangle) * tree.triangles.size();
		result.triangleBuffer = allocator.createBuffer(
			result.triangleBufferSize, vk::BufferUsageFlagBits::eStorageBuffer, VMA_MEMORY_USAGE_CPU_TO_GPU
		);

		int32_t *ptr = result.nodeBuffer.mapAs<int32_t>();
		*ptr = tree.root;
		auto *nodes = reinterpret_cast<shader::AabbTreeNode*>(reinterpret_cast<uintptr_t>(ptr) + 16);
		std::memcpy(nodes, tree.nodes.data(), sizeof(shader::AabbTreeNode) * tree.nodes.size());
		result.nodeBuffer.unmap();
		result.nodeBuffer.flush();

		auto *tris = result.triangleBuffer.mapAs<shader::Triangle>();
		std::memcpy(tris, tree.triangles.data(), sizeof(shader::Triangle) * tree.triangles.size());
		result.triangleBuffer.unmap();
		result.triangleBuffer.flush();

		return result;
	}
};
