#include "gBufferPass.h"

#include <cassert>

bool _formatsInitialized = false;
GBuffer::Formats _gBufferFormats;

void GBuffer::resize(vma::Allocator &allocator, vk::Device device, vk::Extent2D extent, Pass &pass) {
	_framebuffer.reset();

	_albedoView.reset();
	_normalView.reset();
	_depthView.reset();

	_albedoBuffer.reset();
	_normalBuffer.reset();
	_depthBuffer.reset();

	const Formats &formats = Formats::get();

	_albedoBuffer = allocator.createImage2D(
		extent, formats.albedo,
		vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eInputAttachment
	);
	_normalBuffer = allocator.createImage2D(
		extent, formats.normal,
		vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eInputAttachment
	);
	_depthBuffer = allocator.createImage2D(
		extent, formats.depth,
		vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eInputAttachment
	);

	_albedoView = createImageView2D(
		device, _albedoBuffer.get(), formats.albedo, vk::ImageAspectFlagBits::eColor
	);
	_normalView = createImageView2D(
		device, _normalBuffer.get(), formats.normal, vk::ImageAspectFlagBits::eColor
	);
	_depthView = createImageView2D(
		device, _depthBuffer.get(), formats.depth, formats.depthAspect
	);

	std::array<vk::ImageView, 3> attachments{
		_albedoView.get(), _normalView.get(), _depthView.get()
	};

	vk::FramebufferCreateInfo framebufferInfo;
	framebufferInfo
		.setRenderPass(pass.getPass())
		.setAttachments(attachments)
		.setWidth(extent.width)
		.setHeight(extent.height)
		.setLayers(1);

	_framebuffer = device.createFramebufferUnique(framebufferInfo);
}

void GBuffer::Formats::initialize(vk::PhysicalDevice physicalDevice) {
	assert(!_formatsInitialized);
	_gBufferFormats.albedo = findSupportedFormat(
		{ vk::Format::eR8G8B8A8Srgb }, physicalDevice, vk::ImageTiling::eOptimal,
		vk::FormatFeatureFlagBits::eColorAttachment
	);
	_gBufferFormats.normal = findSupportedFormat(
		{ vk::Format::eR16G16Snorm, vk::Format::eR8G8Snorm }, physicalDevice, vk::ImageTiling::eOptimal,
		vk::FormatFeatureFlagBits::eColorAttachment
	);
	_gBufferFormats.depth = findSupportedFormat(
		{ vk::Format::eD32Sfloat, vk::Format::eD32SfloatS8Uint, vk::Format::eD24UnormS8Uint },
		physicalDevice, vk::ImageTiling::eOptimal, vk::FormatFeatureFlagBits::eDepthStencilAttachment
	);
	_gBufferFormats.depthAspect = vk::ImageAspectFlagBits::eDepth;
	if (_gBufferFormats.depth != vk::Format::eD32Sfloat) {
		_gBufferFormats.depthAspect |= vk::ImageAspectFlagBits::eStencil;
	}
	_formatsInitialized = true;
}

const GBuffer::Formats &GBuffer::Formats::get() {
	assert(_formatsInitialized);
	return _gBufferFormats;
}
