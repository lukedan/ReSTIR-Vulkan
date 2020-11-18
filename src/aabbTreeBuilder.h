#pragma once

#include <cmath>
#include <vector>

#include "shaderStructs/aabbTree.h"
#include "../gltf/gltfscene.h"
#include "vma.h"

struct AabbTree {
	std::vector<AabbNode> nodes;
	std::vector<Triangle> triangles;
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
		result.nodeBufferSize = 16 + sizeof(AabbNode) * tree.nodes.size();
		result.nodeBuffer = allocator.createBuffer(
			result.nodeBufferSize, vk::BufferUsageFlagBits::eStorageBuffer, VMA_MEMORY_USAGE_CPU_TO_GPU
		);

		result.triangleBufferSize = sizeof(Triangle) * tree.triangles.size();
		result.triangleBuffer = allocator.createBuffer(
			result.triangleBufferSize, vk::BufferUsageFlagBits::eStorageBuffer, VMA_MEMORY_USAGE_CPU_TO_GPU
		);

		int32_t *ptr = result.nodeBuffer.mapAs<int32_t>();
		*ptr = tree.root;
		AabbNode *nodes = reinterpret_cast<AabbNode*>(reinterpret_cast<uintptr_t>(ptr) + 16);
		std::memcpy(nodes, tree.nodes.data(), sizeof(AabbNode) * tree.nodes.size());
		result.nodeBuffer.unmap();
		result.nodeBuffer.flush();

		Triangle *tris = result.triangleBuffer.mapAs<Triangle>();
		std::memcpy(tris, tree.triangles.data(), sizeof(Triangle) * tree.triangles.size());
		result.triangleBuffer.unmap();
		result.triangleBuffer.flush();

		return result;
	}
};
