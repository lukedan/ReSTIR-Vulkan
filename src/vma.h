#pragma once

#include <cassert>

#include <vulkan/vulkan.hpp>

#include <vk_mem_alloc.h>

namespace vma {
	class Allocator;


	template <typename T, typename Derived> struct UniqueHandle {
	public:
		UniqueHandle() = default;
		UniqueHandle(Derived &&src) :
			_object(src._object), _allocation(src._allocation), _allocator(src._allocator) {

			assert(&src != this);
			src._object = T();
			src._allocation = nullptr;
			src._allocator = nullptr;
		}
		UniqueHandle(const UniqueHandle&) = delete;
		UniqueHandle &operator=(UniqueHandle &&src) {
			assert(&src != this);
			reset();

			_object = src._object;
			_allocation = src._allocation;
			_allocator = src._allocator;

			src._object = T();
			src._allocation = nullptr;
			src._allocator = nullptr;

			return *this;
		}
		UniqueHandle &operator=(const UniqueHandle&) = delete;
		~UniqueHandle() {
			reset();
		}

		T get() const {
			return _object;
		}

		void reset() {
			if (_object) {
				static_cast<Derived*>(this)->_release();
				_object = T();
				_allocation = nullptr;
				_allocator = nullptr;
			}
		}
	protected:
		T _object;
		VmaAllocation _allocation = nullptr;
		Allocator *_allocator = nullptr;

		[[nodiscard]] VmaAllocator _getAllocator() const {
			return _allocator->_allocator;
		}
	};

	struct UniqueBuffer : public UniqueHandle<vk::Buffer, UniqueBuffer> {
		friend Allocator;
	private:
		using Base = UniqueHandle<vk::Buffer, UniqueBuffer>;
		friend Base;
	public:
		UniqueBuffer() = default;
		UniqueBuffer(UniqueBuffer &&src) : Base(std::move(src)) {
		}
		UniqueBuffer &operator=(UniqueBuffer &&src) {
			Base::operator=(std::move(src));
			return *this;
		}

		void *map();
		template <typename T> T *mapAs() {
			return static_cast<T*>(map());
		}
		void unmap() {
			vmaUnmapMemory(_getAllocator(), _allocation);
		}
	private:
		void _release() {
			vmaDestroyBuffer(_getAllocator(), _object, _allocation);
		}
	};

	struct UniqueImage : public UniqueHandle<vk::Image, UniqueImage> {
		friend Allocator;
	private:
		using Base = UniqueHandle<vk::Image, UniqueImage>;
		friend Base;
	public:
		UniqueImage() = default;
		UniqueImage(UniqueImage &&src) : Base(std::move(src)) {
		}
		UniqueImage &operator=(UniqueImage &&src) {
			Base::operator=(std::move(src));
			return *this;
		}
	private:
		void _release() {
			vmaDestroyImage(_getAllocator(), _object, _allocation);
		}
	};


	class Allocator {
		template <typename, typename> friend struct UniqueHandle;
	public:
		Allocator() = default;
		Allocator(Allocator &&src) : _allocator(src._allocator) {
			assert(&src != this);
			src._allocator = nullptr;
		}
		Allocator(const Allocator&) = delete;
		Allocator &operator=(Allocator &&src) {
			assert(&src != this);
			reset();
			_allocator = src._allocator;
			src._allocator = nullptr;
			return *this;
		}
		Allocator &operator=(const Allocator&) = delete;
		~Allocator() {
			reset();
		}

		[[nodiscard]] UniqueBuffer createBuffer(const vk::BufferCreateInfo&, const VmaAllocationCreateInfo&);
		[[nodiscard]] UniqueImage createImage(const vk::ImageCreateInfo&, const VmaAllocationCreateInfo&);

		[[nodiscard]] UniqueImage createImage2D(
			vk::Extent2D size, vk::Format format, vk::ImageUsageFlags usage,
			VmaMemoryUsage memoryUsage = VMA_MEMORY_USAGE_GPU_ONLY,
			vk::ImageTiling tiling = vk::ImageTiling::eOptimal,
			uint32_t mipLevels = 1,
			vk::SampleCountFlagBits sampleCount = vk::SampleCountFlagBits::e1,
			const std::vector<uint32_t> *sharedQueues = nullptr,
			vk::ImageLayout initialLayout = vk::ImageLayout::eUndefined,
			uint32_t arrayLayers = 1
		);
		template <typename T> [[nodiscard]] UniqueBuffer createTypedBuffer(
			std::size_t numElements, vk::BufferUsageFlags usage,
			VmaMemoryUsage memoryUsage = VMA_MEMORY_USAGE_CPU_TO_GPU,
			const std::vector<uint32_t> *sharedQueues = nullptr
		) {
			vk::BufferCreateInfo bufferInfo;
			bufferInfo
				.setSize(static_cast<uint32_t>(sizeof(T) * numElements))
				.setUsage(usage);
			if (sharedQueues) {
				bufferInfo
					.setSharingMode(vk::SharingMode::eConcurrent)
					.setQueueFamilyIndices(*sharedQueues);
			} else {
				bufferInfo.setSharingMode(vk::SharingMode::eExclusive);
			}

			VmaAllocationCreateInfo allocationInfo{};
			allocationInfo.usage = memoryUsage;

			return createBuffer(bufferInfo, allocationInfo);
		}

		void reset() {
			if (_allocator) {
				vmaDestroyAllocator(_allocator);
				_allocator = nullptr;
			}
		}
		[[nodiscard]] bool empty() const {
			return _allocator == nullptr;
		}
		[[nodiscard]] explicit operator bool() const {
			return _allocator != nullptr;
		}

		[[nodiscard]] static Allocator create(
			uint32_t vulkanApiVersion, vk::Instance, vk::PhysicalDevice, vk::Device
		);
	private:
		VmaAllocator _allocator = nullptr;
	};
}
