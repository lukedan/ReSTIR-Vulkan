#pragma once

#include <cmath>
#include <vector>

#include <gltfscene.h>

#include "shaderIncludes.h"
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
		// align the array correctly
		result.nodeBufferSize = sizeof(shader::AabbTreeNode) * tree.nodes.size();
		result.nodeBuffer = allocator.createBuffer(
			static_cast<uint32_t>(result.nodeBufferSize),
			vk::BufferUsageFlagBits::eStorageBuffer, VMA_MEMORY_USAGE_CPU_TO_GPU
		);

		result.triangleBufferSize = sizeof(shader::Triangle) * tree.triangles.size();
		result.triangleBuffer = allocator.createBuffer(
			static_cast<uint32_t>(result.triangleBufferSize),
			vk::BufferUsageFlagBits::eStorageBuffer, VMA_MEMORY_USAGE_CPU_TO_GPU
		);

		auto *ptr = result.nodeBuffer.mapAs<shader::AabbTreeNode>();
		std::memcpy(ptr, tree.nodes.data(), sizeof(shader::AabbTreeNode) * tree.nodes.size());
		result.nodeBuffer.unmap();
		result.nodeBuffer.flush();

		auto *tris = result.triangleBuffer.mapAs<shader::Triangle>();
		std::memcpy(tris, tree.triangles.data(), sizeof(shader::Triangle) * tree.triangles.size());
		result.triangleBuffer.unmap();
		result.triangleBuffer.flush();

		return result;
	}
};
