#pragma once

#include <vulkan/vulkan.hpp>

#include "pass.h"
#include "../vma.h"
#include "../misc.h"
#include "../shader.h"
#include "../sceneBuffers.h"
#include "../camera.h"

class GBufferPass;

class GBuffer {
public:
	[[nodiscard]] vk::Image getAlbedoBuffer() const {
		return _albedoBuffer.get();
	}
	[[nodiscard]] vk::Image getNormalBuffer() const {
		return _normalBuffer.get();
	}
	[[nodiscard]] vk::Image getMaterialPropertiesBuffer() const {
		return _materialPropertiesBuffer.get();
	}
	[[nodiscard]] vk::Image getWorldPositionBuffer() const {
		return _worldPosBuffer.get();
	}
	[[nodiscard]] vk::Image getDepthBuffer() const {
		return _depthBuffer.get();
	}

	[[nodiscard]] vk::ImageView getAlbedoView() const {
		return _albedoView.get();
	}
	[[nodiscard]] vk::ImageView getNormalView() const {
		return _normalView.get();
	}
	[[nodiscard]] vk::ImageView getMaterialPropertiesView() const {
		return _materialPropertiesView.get();
	}
	[[nodiscard]] vk::ImageView getWorldPositionView() const {
		return _worldPosView.get();
	}
	[[nodiscard]] vk::ImageView getDepthView() const {
		return _depthView.get();
	}

	[[nodiscard]] vk::Framebuffer getFramebuffer() const {
		return _framebuffer.get();
	}

	void resize(vma::Allocator&, vk::Device, vk::Extent2D, Pass&);

	[[nodiscard]] inline static GBuffer create(
		vma::Allocator &allocator, vk::Device device, vk::Extent2D bufferExtent, Pass &pass
	) {
		GBuffer result;
		result.resize(allocator, device, bufferExtent, pass);
		return result;
	}

	struct Formats {
		vk::Format albedo;
		vk::Format normal;
		vk::Format depth;
		vk::Format materialProperties;
		vk::Format worldPosition;
		vk::ImageAspectFlags depthAspect;

		[[nodiscard]] static void initialize(vk::PhysicalDevice);
		[[nodiscard]] static const Formats &get();
	};
private:
	vma::UniqueImage _albedoBuffer;
	vma::UniqueImage _normalBuffer;
	vma::UniqueImage _materialPropertiesBuffer;
	vma::UniqueImage _worldPosBuffer;
	vma::UniqueImage _depthBuffer;

	vk::UniqueImageView _albedoView;
	vk::UniqueImageView _normalView;
	vk::UniqueImageView _materialPropertiesView;
	vk::UniqueImageView _worldPosView;
	vk::UniqueImageView _depthView;

	vk::UniqueFramebuffer _framebuffer;
};

class GBufferPass : public Pass {
	friend Pass;
public:
	struct Uniforms {
		nvmath::mat4 projectionViewMatrix;
	};

	struct Resources {
		vma::UniqueBuffer uniformBuffer;
		vk::UniqueDescriptorSet uniformDescriptor;
		vk::UniqueDescriptorSet matrixDescriptor;
		vk::UniqueDescriptorSet materialDescriptor;
		std::vector<vk::UniqueDescriptorSet> materialTexturesDescriptors;
	};

	GBufferPass() = default;

	void issueCommands(vk::CommandBuffer, vk::Framebuffer) const override;

	void onResized(vk::Device dev, vk::Extent2D extent) {
		_bufferExtent = extent;
		_recreatePipelines(dev);
	}

	[[nodiscard]] vk::DescriptorSetLayout getMatricesDescriptorSetLayout() const {
		return _matricesDescriptorSetLayout.get();
	}
	[[nodiscard]] vk::DescriptorSetLayout getUniformsDescriptorSetLayout() const {
		return _uniformsDescriptorSetLayout.get();
	}
	[[nodiscard]] vk::DescriptorSetLayout getTexturesDescriptorSetLayout() const {
		return _textureDescriptorSetLayout.get();
	}
	[[nodiscard]] vk::DescriptorSetLayout getMaterialDescriptorSetLayout() const {
		return _materialDescriptorSetLayout.get();
	}

	void initializeResourcesFor(const nvh::GltfScene&, const SceneBuffers&, vk::UniqueDevice&, Resources&);

	const nvh::GltfScene *scene = nullptr;
	const SceneBuffers *sceneBuffers = nullptr;
	const Resources *descriptorSets;
protected:
	explicit GBufferPass(vk::Extent2D extent) : _bufferExtent(extent) {
	}

	vk::Extent2D _bufferExtent;
	Shader _vert, _frag;
	vk::UniqueDescriptorSetLayout _uniformsDescriptorSetLayout;
	vk::UniqueDescriptorSetLayout _matricesDescriptorSetLayout;
	vk::UniqueDescriptorSetLayout _materialDescriptorSetLayout;
	vk::UniqueDescriptorSetLayout _textureDescriptorSetLayout;
	vk::UniquePipelineLayout _pipelineLayout;

	vk::UniqueRenderPass _createPass(vk::Device) override;
	std::vector<PipelineCreationInfo> _getPipelineCreationInfo() override;


	void _initialize(vk::Device dev) override;
};
