// this file also hosts vma definitions
#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

#include "vma.h"

#include "misc.h"

namespace vma {
	UniqueBuffer Allocator::createBuffer(
		const vk::BufferCreateInfo &vkBufferInfo, const VmaAllocationCreateInfo &allocationInfo
	) {
		VkBufferCreateInfo bufferInfo = vkBufferInfo;
		UniqueBuffer result;
		VkBuffer buffer;
		vkCheck(vmaCreateBuffer(_allocator, &bufferInfo, &allocationInfo, &buffer, &result._allocation, nullptr));
		result._object = buffer;
		result._allocator = this;
		return result;
	}

	UniqueImage Allocator::createImage(
		const vk::ImageCreateInfo &vkImageInfo, const VmaAllocationCreateInfo &allocationInfo
	) {
		VkImageCreateInfo imageInfo = vkImageInfo;
		UniqueImage result;
		VkImage image;
		vkCheck(vmaCreateImage(_allocator, &imageInfo, &allocationInfo, &image, &result._allocation, nullptr));
		result._object = image;
		result._allocator = this;
		return result;
	}

	Allocator Allocator::create(
		uint32_t version, vk::Instance inst, vk::PhysicalDevice physDev, vk::Device dev
	) {
		Allocator result;
		VmaAllocatorCreateInfo allocatorInfo{};
		allocatorInfo.vulkanApiVersion = version;
		allocatorInfo.instance = inst;
		allocatorInfo.physicalDevice = physDev;
		allocatorInfo.device = dev;
		allocatorInfo.flags = 0;
		vkCheck(vmaCreateAllocator(&allocatorInfo, &result._allocator));
		return result;
	}
}
