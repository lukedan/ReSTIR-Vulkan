#include "misc.h"

#include <fstream>

#include "vma.h"

std::vector<char> readFile(const std::filesystem::path &path) {
	std::ifstream fin(path, std::ios::ate | std::ios::binary);
	std::streampos size = fin.tellg();
	std::vector<char> result(static_cast<std::size_t>(size));
	fin.seekg(0);
	fin.read(result.data(), size);
	return result;
}

void vkCheck(vk::Result res) {
	if (static_cast<int>(res) < 0) {
		std::cout << "Vulkan error: " << vk::to_string(res) << "\n";
		std::abort();
	}
}

[[nodiscard]] vk::Format findSupportedFormat(
	const std::vector<vk::Format> &candidates, vk::PhysicalDevice physicalDevice,
	vk::ImageTiling requiredTiling, vk::FormatFeatureFlags requiredFeatures
) {
	for (vk::Format fmt : candidates) {
		vk::FormatProperties properties = physicalDevice.getFormatProperties(fmt);
		vk::FormatFeatureFlags features =
			requiredTiling == vk::ImageTiling::eLinear ?
			properties.linearTilingFeatures :
			properties.optimalTilingFeatures;
		if ((features & requiredFeatures) == requiredFeatures) {
			return fmt;
		}
	}
	std::cout <<
		"Failed to find format with tiling " << vk::to_string(requiredTiling) <<
		" and features " << vk::to_string(requiredFeatures) << "\n";
	std::abort();
}

vk::UniqueImageView createImageView2D(
	vk::Device device, vk::Image image, vk::Format format, vk::ImageAspectFlags aspect,
	uint32_t baseMipLevel, uint32_t mipLevelCount, uint32_t baseArrayLayer, uint32_t arrayLayerCount
) {
	vk::ImageSubresourceRange range;
	range
		.setAspectMask(aspect)
		.setBaseMipLevel(baseMipLevel)
		.setLevelCount(mipLevelCount)
		.setBaseArrayLayer(baseArrayLayer)
		.setLayerCount(arrayLayerCount);

	vk::ImageViewCreateInfo imageViewInfo;
	imageViewInfo
		.setImage(image)
		.setViewType(vk::ImageViewType::e2D)
		.setFormat(format)
		.setSubresourceRange(range);

	return device.createImageViewUnique(imageViewInfo);
}

void transitionImageLayout(
	vk::CommandBuffer commandBuffer,
	vk::Image image, vk::Format format,
	vk::ImageLayout oldImageLayout, vk::ImageLayout newImageLayout,
	uint32_t baseMipLevel, uint32_t numMipLevels
) {
	vk::AccessFlags sourceAccessMask;
	switch (oldImageLayout) {
	case vk::ImageLayout::eTransferSrcOptimal:
		sourceAccessMask = vk::AccessFlagBits::eTransferRead;
		break;
	case vk::ImageLayout::eTransferDstOptimal:
		sourceAccessMask = vk::AccessFlagBits::eTransferWrite;
		break;
	case vk::ImageLayout::ePreinitialized:
		sourceAccessMask = vk::AccessFlagBits::eHostWrite;
		break;
	case vk::ImageLayout::eColorAttachmentOptimal:
		sourceAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
		break;
	case vk::ImageLayout::eGeneral:
		[[fallthrough]];
	case vk::ImageLayout::eUndefined:
		// sourceAccessMask is empty
		break;
	default:
		assert(false);
		break;
	}

	vk::PipelineStageFlags sourceStage;
	switch (oldImageLayout) {
	case vk::ImageLayout::eGeneral:
		[[fallthrough]];
	case vk::ImageLayout::ePreinitialized:
		sourceStage = vk::PipelineStageFlagBits::eHost;
		break;
	case vk::ImageLayout::eTransferSrcOptimal:
		[[fallthrough]];
	case vk::ImageLayout::eTransferDstOptimal:
		sourceStage = vk::PipelineStageFlagBits::eTransfer;
		break;
	case vk::ImageLayout::eColorAttachmentOptimal:
		sourceStage = vk::PipelineStageFlagBits::eColorAttachmentOutput;
		break;
	case vk::ImageLayout::eUndefined:
		sourceStage = vk::PipelineStageFlagBits::eTopOfPipe;
		break;
	default:
		assert(false);
		break;
	}

	vk::AccessFlags destinationAccessMask;
	switch (newImageLayout) {
	case vk::ImageLayout::eColorAttachmentOptimal:
		destinationAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
		break;
	case vk::ImageLayout::eDepthStencilAttachmentOptimal:
		destinationAccessMask =
			vk::AccessFlagBits::eDepthStencilAttachmentRead | vk::AccessFlagBits::eDepthStencilAttachmentWrite;
		break;
	case vk::ImageLayout::eShaderReadOnlyOptimal:
		destinationAccessMask = vk::AccessFlagBits::eShaderRead;
		break;
	case vk::ImageLayout::eTransferSrcOptimal:
		destinationAccessMask = vk::AccessFlagBits::eTransferRead;
		break;
	case vk::ImageLayout::eTransferDstOptimal:
		destinationAccessMask = vk::AccessFlagBits::eTransferWrite;
		break;
	case vk::ImageLayout::eGeneral:
		[[fallthrough]];
	case vk::ImageLayout::ePresentSrcKHR:
		// empty destinationAccessMask
		break;
	default:
		assert(false);
		break;
	}

	vk::PipelineStageFlags destinationStage;
	switch (newImageLayout) {
	case vk::ImageLayout::eColorAttachmentOptimal:
		destinationStage = vk::PipelineStageFlagBits::eColorAttachmentOutput;
		break;
	case vk::ImageLayout::eDepthStencilAttachmentOptimal:
		destinationStage = vk::PipelineStageFlagBits::eEarlyFragmentTests;
		break;
	case vk::ImageLayout::eGeneral:
		destinationStage = vk::PipelineStageFlagBits::eHost;
		break;
	case vk::ImageLayout::ePresentSrcKHR:
		destinationStage = vk::PipelineStageFlagBits::eBottomOfPipe;
		break;
	case vk::ImageLayout::eShaderReadOnlyOptimal:
		destinationStage = vk::PipelineStageFlagBits::eFragmentShader;
		break;
	case vk::ImageLayout::eTransferDstOptimal:
		[[fallthrough]];
	case vk::ImageLayout::eTransferSrcOptimal:
		destinationStage = vk::PipelineStageFlagBits::eTransfer;
		break;
	default:
		assert(false);
		break;
	}

	vk::ImageAspectFlags aspectMask;
	if (newImageLayout == vk::ImageLayout::eDepthStencilAttachmentOptimal) {
		aspectMask = vk::ImageAspectFlagBits::eDepth;
		if (format == vk::Format::eD32SfloatS8Uint || format == vk::Format::eD24UnormS8Uint) {
			aspectMask |= vk::ImageAspectFlagBits::eStencil;
		}
	} else {
		aspectMask = vk::ImageAspectFlagBits::eColor;
	}

	vk::ImageSubresourceRange imageSubresourceRange(aspectMask, baseMipLevel, numMipLevels, 0, 1);
	vk::ImageMemoryBarrier imageMemoryBarrier;
	imageMemoryBarrier
		.setSrcAccessMask(sourceAccessMask)
		.setDstAccessMask(destinationAccessMask)
		.setOldLayout(oldImageLayout)
		.setNewLayout(newImageLayout)
		.setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
		.setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
		.setImage(image)
		.setSubresourceRange(imageSubresourceRange);
	commandBuffer.pipelineBarrier(sourceStage, destinationStage, {}, nullptr, nullptr, imageMemoryBarrier);
}

vma::UniqueImage loadTexture(
	const unsigned char *data, uint32_t width, uint32_t height, vk::Format format, uint32_t mipLevels,
	vma::Allocator &allocator, TransientCommandBufferPool &cmdBufferPool, vk::Queue queue
) {
	vma::UniqueBuffer buffer = allocator.createTypedBuffer<unsigned char>(
		width * height * 4, vk::BufferUsageFlagBits::eTransferSrc, VMA_MEMORY_USAGE_CPU_TO_GPU
		);

	void *bufferData = buffer.map();
	std::memcpy(bufferData, data, sizeof(unsigned char) * width * height * 4);
	buffer.unmap();
	buffer.flush();

	vk::ImageUsageFlags usageFlags = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst;
	if (mipLevels > 1) {
		usageFlags |= vk::ImageUsageFlagBits::eTransferSrc;
	}
	vma::UniqueImage image = allocator.createImage2D(
		vk::Extent2D(width, height),
		format, usageFlags,
		VMA_MEMORY_USAGE_GPU_ONLY,
		vk::ImageTiling::eOptimal,
		vk::ImageLayout::eUndefined,
		mipLevels
	);

	{
		TransientCommandBuffer cmdBuffer = cmdBufferPool.begin(queue);

		transitionImageLayout(
			cmdBuffer.get(), image.get(), format,
			vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal, 0, mipLevels
		);

		// copy buffer to image
		vk::BufferImageCopy bufImgCopy;
		bufImgCopy
			.setImageExtent(vk::Extent3D(width, height, 1))
			.setImageSubresource(vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1));
		cmdBuffer->copyBufferToImage(buffer.get(), image.get(), vk::ImageLayout::eTransferDstOptimal, bufImgCopy);

		// generate mipmaps
		if (mipLevels > 1) {
			uint32_t mipWidth = width, mipHeight = height;
			for (uint32_t i = 1; i < mipLevels; ++i) {
				uint32_t nextWidth = std::max<uint32_t>(mipWidth / 2, 1), nextHeight = std::max<uint32_t>(mipHeight / 2, 1);

				transitionImageLayout(
					cmdBuffer.get(), image.get(), format,
					vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eTransferSrcOptimal, i - 1, 1
				);

				vk::ImageBlit blit;
				blit
					.setSrcOffsets({ vk::Offset3D(0, 0, 0), vk::Offset3D(mipWidth, mipHeight, 1) })
					.setSrcSubresource(vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, i - 1, 0, 1))
					.setDstOffsets({ vk::Offset3D(0, 0, 0), vk::Offset3D(nextWidth, nextHeight, 1) })
					.setDstSubresource(vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, i, 0, 1));
				cmdBuffer->blitImage(
					image.get(), vk::ImageLayout::eTransferSrcOptimal,
					image.get(), vk::ImageLayout::eTransferDstOptimal,
					blit, vk::Filter::eLinear
				);

				mipWidth = nextWidth;
				mipHeight = nextHeight;
			}

			transitionImageLayout(
				cmdBuffer.get(), image.get(), format,
				vk::ImageLayout::eTransferSrcOptimal, vk::ImageLayout::eShaderReadOnlyOptimal, 0, mipLevels - 1
			);
			transitionImageLayout(
				cmdBuffer.get(), image.get(), format,
				vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal, mipLevels - 1, 1
			);
		} else {
			transitionImageLayout(
				cmdBuffer.get(), image.get(), format,
				vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal, 0, mipLevels
			);
		}
	}

	return image;
}

vma::UniqueImage loadTexture(
	const tinygltf::Image &imageBinary, vk::Format format, uint32_t mipLevels,
	vma::Allocator &allocator, TransientCommandBufferPool &cmdBufferPool, vk::Queue queue
) {
	return loadTexture(
		imageBinary.image.data(), imageBinary.width, imageBinary.height, format, mipLevels,
		allocator, cmdBufferPool, queue
	);
}


void loadScene(const std::string& filename, nvh::GltfScene& m_gltfScene) {
	tinygltf::Model    tmodel;
	tinygltf::TinyGLTF tcontext;
	std::string        warn, error;
	if (!tcontext.LoadASCIIFromFile(&tmodel, &error, &warn, filename)) {
		assert(!"Error while loading scene");
	}
	m_gltfScene.importDrawableNodes(tmodel, nvh::GltfAttributes::Normal | nvh::GltfAttributes::Texcoord_0 | nvh::GltfAttributes::Color_0 | nvh::GltfAttributes::Tangent);
	m_gltfScene.importMaterials(tmodel);
	m_gltfScene.importTexutureImages(tmodel);


	// Show gltf scene info
	std::cout << "Show gltf scene info" << std::endl;
	std::cout << "scene center:[" << m_gltfScene.m_dimensions.center.x << ", "
		<< m_gltfScene.m_dimensions.center.y << ", "
		<< m_gltfScene.m_dimensions.center.z << "]" << std::endl;

	std::cout << "max:[" << m_gltfScene.m_dimensions.max.x << ", "
		<< m_gltfScene.m_dimensions.max.y << ", "
		<< m_gltfScene.m_dimensions.max.z << "]" << std::endl;

	std::cout << "min:[" << m_gltfScene.m_dimensions.min.x << ", "
		<< m_gltfScene.m_dimensions.min.y << ", "
		<< m_gltfScene.m_dimensions.min.z << "]" << std::endl;

	std::cout << "radius:" << m_gltfScene.m_dimensions.radius << std::endl;

	std::cout << "size:[" << m_gltfScene.m_dimensions.size.x << ", "
		<< m_gltfScene.m_dimensions.size.y << ", "
		<< m_gltfScene.m_dimensions.size.z << "]" << std::endl;

	std::cout << "vertex num:" << m_gltfScene.m_positions.size() << std::endl;
}
