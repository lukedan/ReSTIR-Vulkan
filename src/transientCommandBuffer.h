#pragma once

#include <vulkan/vulkan.hpp>

class TransientCommandBufferPool;

class TransientCommandBuffer {
	friend TransientCommandBufferPool;
public:
	TransientCommandBuffer() = default;
	TransientCommandBuffer(TransientCommandBuffer&&) = default;
	TransientCommandBuffer &operator=(TransientCommandBuffer&&) = default;
	~TransientCommandBuffer() {
		submitAndWait();
	}

	void submitAndWait() {
		if (!empty()) {
			_buffer->end();

			std::array<vk::CommandBuffer, 1> buffers{ _buffer.get() };
			vk::SubmitInfo submitInfo;
			submitInfo.setCommandBuffers(buffers);
			_queue.submit(submitInfo, _fence.get());
			_device.waitForFences(_fence.get(), true, std::numeric_limits<uint64_t>::max());

			_buffer.reset();
			_fence.reset();
			_device = nullptr;
			_queue = nullptr;
		}
	}

	[[nodiscard]] vk::CommandBuffer get() const {
		return _buffer.get();
	}
	[[nodiscard]] const vk::CommandBuffer *operator->() const {
		return &_buffer.get();
	}

	[[nodiscard]] bool empty() const {
		return !_buffer;
	}
	[[nodiscard]] explicit operator bool() const {
		return static_cast<bool>(_buffer);
	}
private:
	vk::UniqueCommandBuffer _buffer;
	vk::UniqueFence _fence;
	vk::Device _device;
	vk::Queue _queue;
};

class TransientCommandBufferPool {
public:
	TransientCommandBufferPool() = default;
	TransientCommandBufferPool(vk::Device device, uint32_t queueIndex) : _device(device) {
		vk::CommandPoolCreateInfo transientPoolInfo;
		transientPoolInfo
			.setQueueFamilyIndex(queueIndex)
			.setFlags(vk::CommandPoolCreateFlagBits::eTransient);
		_pool = _device.createCommandPoolUnique(transientPoolInfo);
	}

	TransientCommandBuffer begin(vk::Queue queue) {
		TransientCommandBuffer result;

		result._device = _device;
		result._queue = queue;

		vk::CommandBufferAllocateInfo bufferInfo;
		bufferInfo
			.setCommandPool(_pool.get())
			.setLevel(vk::CommandBufferLevel::ePrimary)
			.setCommandBufferCount(1);
		result._buffer = std::move(_device.allocateCommandBuffersUnique(bufferInfo)[0]);

		result._fence = _device.createFenceUnique(vk::FenceCreateInfo());

		vk::CommandBufferBeginInfo beginInfo;
		beginInfo.setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
		result->begin(beginInfo);

		return result;
	}
private:
	vk::UniqueCommandPool _pool;
	vk::Device _device;
};
