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

	UniqueImage Allocator::createImage2D(
		vk::Extent2D size, vk::Format format, vk::ImageUsageFlags usage,
		VmaMemoryUsage memoryUsage, vk::ImageTiling tiling,
		uint32_t mipLevels, vk::SampleCountFlagBits sampleCount,
		const std::vector<uint32_t> *sharedQueues,
		vk::ImageLayout initialLayout, uint32_t arrayLayers
	) {
		vk::ImageCreateInfo imageInfo;
		imageInfo
			.setImageType(vk::ImageType::e2D)
			.setExtent(vk::Extent3D(size, 1))
			.setFormat(format)
			.setSamples(sampleCount)
			.setMipLevels(mipLevels)
			.setArrayLayers(arrayLayers)
			.setTiling(tiling)
			.setInitialLayout(initialLayout)
			.setUsage(usage);
		if (sharedQueues) {
			imageInfo
				.setSharingMode(vk::SharingMode::eConcurrent)
				.setQueueFamilyIndices(*sharedQueues);
		} else {
			imageInfo.setSharingMode(vk::SharingMode::eExclusive);
		}

		VmaAllocationCreateInfo allocationInfo{};
		allocationInfo.usage = memoryUsage;

		return createImage(imageInfo, allocationInfo);
	}

	Allocator Allocator::create(
		uint32_t version, vk::Instance inst, vk::PhysicalDevice physDev, vk::Device dev
	) {
		Allocator result;
		VmaAllocatorCreateInfo allocatorInfo{};
		allocatorInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
		allocatorInfo.vulkanApiVersion = version;
		allocatorInfo.instance = inst;
		allocatorInfo.physicalDevice = physDev;
		allocatorInfo.device = dev;
		vkCheck(vmaCreateAllocator(&allocatorInfo, &result._allocator));
		return result;
	}
}
