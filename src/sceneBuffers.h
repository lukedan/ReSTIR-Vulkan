#pragma once

#include <vulkan/vulkan.hpp>

#include <nvmath_glsltypes.h>

#include <gltfscene.h>

#include "vertex.h"
#include "vma.h"
#include "transientCommandBuffer.h"
#include "shaderIncludes.h"

#undef MemoryBarrier

class SceneBuffers {
public:
	struct SceneTexture {
		vk::UniqueImageView imageView;
		vk::UniqueSampler sampler;
		vma::UniqueImage image;

		[[nodiscard]] vk::DescriptorImageInfo getDescriptorInfo(
			vk::ImageLayout layout = vk::ImageLayout::eShaderReadOnlyOptimal
		) const {
			return vk::DescriptorImageInfo(sampler.get(), imageView.get(), layout);
		}
	};

	[[nodiscard]] vk::Buffer getVertices() const {
		return _vertices.get();
	}
	[[nodiscard]] vk::Buffer getIndices() const {
		return _indices.get();
	}
	[[nodiscard]] vk::Buffer getMatrices() const {
		return _matrices.get();
	}
	[[nodiscard]] const vk::Buffer getPtLights() const {
		return _ptLightsBuffer.get();
	}
	[[nodiscard]] const vk::Buffer getTriLights() const {
		return _triLightsBuffer.get();
	}
	[[nodiscard]] vk::Buffer getMaterials() const {
		return _materials.get();
	}
	[[nodiscard]] vk::Buffer getAliasTable() const {
		return _aliasTableBuffer.get();
	}
	[[nodiscard]] const std::vector<SceneTexture> &getTextures() const {
		return _textureImages;
	}
	[[nodiscard]] const SceneTexture &getDefaultNormal() const {
		return _defaultNormal;
	}
	[[nodiscard]] const SceneTexture &getDefaultWhite() const {
		return _defaultWhite;
	}
	[[nodiscard]] const vk::DeviceSize getPtLightsBufferSize() const {
		return _ptLightsBufferSize;
	}
	[[nodiscard]] const vk::DeviceSize getTriLightsBufferSize() const {
		return _triLightsBufferSize;
	}
	[[nodiscard]] const vk::DeviceSize getAliasTableBufferSize() const {
		return _aliasTableBufferSize;
	}
	

	[[nodiscard]] static SceneBuffers create(
		const nvh::GltfScene &scene,
		vma::Allocator &allocator,
		TransientCommandBufferPool &oneTimeBufferPool,
		vk::Device l_device,
		vk::Queue graphicsQueue
	) {
		std::vector<shader::pointLight> pointLights = collectPointLightsFromScene(scene);
		std::vector<shader::triLight> triangleLights = collectTriangleLightsFromScene(scene);
		if (pointLights.empty() && triangleLights.empty()) {
			pointLights = generateRandomPointLights(200, scene.m_dimensions.min, scene.m_dimensions.max);
		}

		std::vector<shader::aliasTableColumn> aliasTable = createAliasTable(pointLights, triangleLights);

		SceneBuffers result;

		result._vertices = allocator.createTypedBuffer<Vertex>(
			scene.m_positions.size(), vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress | vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR, VMA_MEMORY_USAGE_CPU_TO_GPU
			);
		result._indices = allocator.createTypedBuffer<int32_t>(
			scene.m_indices.size(), vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress | vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR, VMA_MEMORY_USAGE_CPU_TO_GPU
			);
		result._matrices = allocator.createTypedBuffer<shader::ModelMatrices>(
			scene.m_nodes.size(), vk::BufferUsageFlagBits::eUniformBuffer, VMA_MEMORY_USAGE_CPU_TO_GPU
			);
		result._materials = allocator.createTypedBuffer<shader::MaterialUniforms>(
			scene.m_materials.size(), vk::BufferUsageFlagBits::eUniformBuffer, VMA_MEMORY_USAGE_CPU_TO_GPU
			);
		// Lights
		// Point lights
		result._ptLightsBufferSize =
			alignPreArrayBlock<shader::pointLight, int32_t>() + 
			sizeof(shader::pointLight) * pointLights.size();
		result._ptLightsBuffer = allocator.createBuffer(
			static_cast<uint32_t>(result._ptLightsBufferSize),
			vk::BufferUsageFlagBits::eStorageBuffer, VMA_MEMORY_USAGE_CPU_TO_GPU
		);
		// Triangle lights
		result._triLightsBufferSize =
			alignPreArrayBlock<shader::triLight, int32_t>() +
			sizeof(shader::triLight) * triangleLights.size();
		result._triLightsBuffer = allocator.createBuffer(
			static_cast<uint32_t>(result._triLightsBufferSize),
			vk::BufferUsageFlagBits::eStorageBuffer, VMA_MEMORY_USAGE_CPU_TO_GPU
		);
		// Alias table
		result._aliasTableBufferSize =
			alignPreArrayBlock<shader::aliasTableColumn, int32_t[4]>() +
			sizeof(shader::aliasTableColumn) * aliasTable.size();
		result._aliasTableBuffer = allocator.createBuffer(
			static_cast<uint32_t>(result._aliasTableBufferSize),
			vk::BufferUsageFlagBits::eStorageBuffer, VMA_MEMORY_USAGE_CPU_TO_GPU
		);


		vk::Format format = vk::Format::eR8G8B8A8Unorm;

		result._textureImages.resize(scene.m_textures.size());

		// load textures
		for (int i = 0; i < scene.m_textures.size(); ++i) {
			auto& gltfimage = scene.m_textures[i];
			std::cout << "Loading Texture: " << gltfimage.uri << std::endl;

			// Create vma::Uniqueimage
			uint32_t numMipLevels = 1 + static_cast<uint32_t>(std::ceil(std::log2(
				std::max(gltfimage.width, gltfimage.height)
			)));
			result._textureImages[i].image = loadTexture(
				gltfimage, format, numMipLevels, allocator, oneTimeBufferPool, graphicsQueue
			);

			result._textureImages[i].sampler = createSampler(
				l_device, vk::Filter::eLinear, vk::Filter::eLinear, vk::SamplerMipmapMode::eLinear, 16.0f
			);
			result._textureImages[i].imageView = createImageView2D(
				l_device, result._textureImages[i].image.get(),
				format, vk::ImageAspectFlagBits::eColor, 0, numMipLevels
			);
		}

		// generate default textures
		{
			unsigned char defaultNormal[4]{ 127, 127, 255, 255 };
			result._defaultNormal.image = loadTexture(
				defaultNormal, 1, 1, vk::Format::eR8G8B8A8Unorm, 1, allocator, oneTimeBufferPool, graphicsQueue
			);
			result._defaultNormal.sampler = createSampler(l_device);
			result._defaultNormal.imageView = createImageView2D(
				l_device, result._defaultNormal.image.get(), format, vk::ImageAspectFlagBits::eColor
			);

			unsigned char defaultWhite[4]{ 255, 255, 255, 255 };
			result._defaultWhite.image = loadTexture(
				defaultWhite, 1, 1, vk::Format::eR8G8B8A8Unorm, 1, allocator, oneTimeBufferPool, graphicsQueue
			);
			result._defaultWhite.sampler = createSampler(l_device);
			result._defaultWhite.imageView = createImageView2D(
				l_device, result._defaultWhite.image.get(), format, vk::ImageAspectFlagBits::eColor
			);
		}

		// collect vertices
		Vertex *vertices = result._vertices.mapAs<Vertex>();
		for (std::size_t i = 0; i < scene.m_positions.size(); ++i) {
			Vertex &v = vertices[i];
			v.position = scene.m_positions[i];
			if (i < scene.m_normals.size()) {
				v.normal = scene.m_normals[i];
			}
			if (i < scene.m_colors0.size()) {
				v.color = scene.m_colors0[i];
			} else {
				v.color = nvmath::vec4(1.0f, 0.0f, 1.0f, 1.0f);
			}
			if (i < scene.m_texcoords0.size()) {
				v.uv = scene.m_texcoords0[i];
			}
			if (i < scene.m_tangents.size()) {
				v.tangent = scene.m_tangents[i];
			}
		}
		result._vertices.unmap();
		result._vertices.flush();

		uint32_t *indices = result._indices.mapAs<uint32_t>();
		for (std::size_t i = 0; i < scene.m_indices.size(); ++i) {
			indices[i] = scene.m_indices[i];
		}
		result._indices.unmap();
		result._indices.flush();


		auto *mat_device = result._materials.mapAs<shader::MaterialUniforms>();
		for (std::size_t i = 0; i < scene.m_materials.size(); ++i) {
			const nvh::GltfMaterial &mat = scene.m_materials[i];
			shader::MaterialUniforms &outMat = mat_device[i];

			outMat.emissiveFactor = mat.emissiveFactor;
			outMat.shadingModel = mat.shadingModel;
			outMat.alphaMode = mat.alphaMode;
			outMat.alphaCutoff = mat.alphaCutoff;
			outMat.normalTextureScale = mat.normalTextureScale;

			switch (outMat.shadingModel) {
			case SHADING_MODEL_METALLIC_ROUGHNESS:
				outMat.colorParam = mat.pbrBaseColorFactor;
				outMat.materialParam.y = mat.pbrRoughnessFactor;
				outMat.materialParam.z = mat.pbrMetallicFactor;
				break;
			case SHADING_MODEL_SPECULAR_GLOSSINESS:
				outMat.colorParam = mat.khrDiffuseFactor;
				outMat.materialParam = mat.khrSpecularFactor;
				outMat.materialParam.w = mat.khrGlossinessFactor;
				break;
			}
		}
		result._materials.unmap();
		result._materials.flush();


		auto *matrices = result._matrices.mapAs<shader::ModelMatrices>();
		for (std::size_t i = 0; i < scene.m_nodes.size(); ++i) {
			matrices[i].transform = scene.m_nodes[i].worldMatrix;
			matrices[i].transformInverseTransposed = nvmath::transpose(nvmath::invert(matrices[i].transform));
		}
		result._matrices.unmap();
		result._matrices.flush();

		// Lights
		// Point lights
		int32_t* pointLightPtr = result._ptLightsBuffer.mapAs<int32_t>();
		*pointLightPtr = static_cast<int32_t>(pointLights.size());
		auto* ptLights = reinterpret_cast<shader::pointLight*>(
			reinterpret_cast<uintptr_t>(pointLightPtr) + alignPreArrayBlock<shader::pointLight, int32_t>()
			);
		std::memcpy(ptLights, pointLights.data(), sizeof(shader::pointLight) * pointLights.size());
		result._ptLightsBuffer.unmap();
		result._ptLightsBuffer.flush();
		
		// Tri lights
		int32_t* triLightsPtr = result._triLightsBuffer.mapAs<int32_t>();
		*triLightsPtr = static_cast<int32_t>(triangleLights.size());
		auto* triLights = reinterpret_cast<shader::triLight*>(
			reinterpret_cast<uintptr_t>(triLightsPtr) + alignPreArrayBlock<shader::triLight, int32_t>()
			);
		std::memcpy(triLights, triangleLights.data(), sizeof(shader::triLight) * triangleLights.size());
		result._triLightsBuffer.unmap();
		result._triLightsBuffer.flush();

		// Alias table
		int32_t* aliasTablePtr = result._aliasTableBuffer.mapAs<int32_t>();
		*aliasTablePtr = static_cast<int32_t>(aliasTable.size());
		auto* aliasTableContentPtr = reinterpret_cast<shader::aliasTableColumn*>(
			reinterpret_cast<uintptr_t>(aliasTablePtr) + alignPreArrayBlock<shader::aliasTableColumn, int32_t[4]>()
			);
		std::memcpy(aliasTableContentPtr, aliasTable.data(), sizeof(shader::aliasTableColumn) * aliasTable.size());
		result._aliasTableBuffer.unmap();
		result._aliasTableBuffer.flush();

		return result;
	}
private:
	vma::UniqueBuffer _vertices;
	vma::UniqueBuffer _indices;
	vma::UniqueBuffer _matrices;
	vma::UniqueBuffer _materials;
	vma::UniqueBuffer _ptLightsBuffer;
	vma::UniqueBuffer _triLightsBuffer;
	vma::UniqueBuffer _aliasTableBuffer;
	std::vector<SceneTexture> _textureImages;
	SceneTexture _defaultNormal;
	SceneTexture _defaultWhite;
	vk::DeviceSize _ptLightsBufferSize;
	vk::DeviceSize _triLightsBufferSize;
	vk::DeviceSize _aliasTableBufferSize;
};

class SceneRaytraceBuffers {
public:
	SceneRaytraceBuffers() = default;
	SceneRaytraceBuffers(SceneRaytraceBuffers&&) = default;
	SceneRaytraceBuffers &operator=(SceneRaytraceBuffers&&) = default;

	[[nodiscard]] vk::AccelerationStructureKHR getTopLevelAccelerationStructure() const {
		return _topLevelAS.get();
	}

	[[nodiscard]] inline static SceneRaytraceBuffers create(
		vk::Device dev,
		vma::Allocator &allocator,
		TransientCommandBufferPool &cmdBufferPool,
		vk::Queue queue,
		const SceneBuffers &sceneBuffer,
		const nvh::GltfScene &gltfScene,
		const vk::DispatchLoaderDynamic &dynamicLoader
	) {
		SceneRaytraceBuffers result;

		result._allocator = &allocator;

		result._allBlas.resize(gltfScene.m_primMeshes.size());
		int index = 0;
		for (auto& primMesh : gltfScene.m_primMeshes) {
			vk::AccelerationStructureGeometryTrianglesDataKHR triangles;
			triangles.setMaxVertex(primMesh.vertexCount);
			triangles.setVertexFormat(vk::Format::eR32G32B32Sfloat);
			triangles.vertexData.setDeviceAddress(dev.getBufferAddress(sceneBuffer.getVertices()));
			triangles.setVertexStride(sizeof(Vertex));
			triangles.setIndexType(vk::IndexType::eUint32);
			triangles.indexData.setDeviceAddress(dev.getBufferAddress(sceneBuffer.getIndices()));

			vk::AccelerationStructureGeometryKHR blasAccelerationGeometry;
			blasAccelerationGeometry.setFlags(vk::GeometryFlagBitsKHR::eOpaque);
			blasAccelerationGeometry.setGeometryType(vk::GeometryTypeKHR::eTriangles);
			blasAccelerationGeometry.geometry.setTriangles(triangles);

			vk::AccelerationStructureBuildGeometryInfoKHR blasAccelerationBuildGeometryInfo;
			blasAccelerationBuildGeometryInfo.setType(vk::AccelerationStructureTypeKHR::eBottomLevel);
			blasAccelerationBuildGeometryInfo.setFlags(vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace);
			blasAccelerationBuildGeometryInfo.setDstAccelerationStructure(result._allBlas.at(index).get());
			blasAccelerationBuildGeometryInfo.setGeometries(blasAccelerationGeometry);

			vk::AccelerationStructureBuildSizesInfoKHR sizeInfo = dev.getAccelerationStructureBuildSizesKHR(
				vk::AccelerationStructureBuildTypeKHR::eDevice,
				blasAccelerationBuildGeometryInfo,
				primMesh.indexCount / 3,
				dynamicLoader
			);

			vma::UniqueBuffer blasScratchBuffer = _createScratchBuffer(sizeInfo.buildScratchSize, allocator);
			blasAccelerationBuildGeometryInfo.scratchData.setDeviceAddress(dev.getBufferAddress(blasScratchBuffer.get()));

			result._asAllocations.emplace_back(_createAccelerationStructureBuffer(
				sizeInfo.accelerationStructureSize, allocator
			));

			vk::AccelerationStructureCreateInfoKHR blasCreateInfo;
			blasCreateInfo.setBuffer(result._asAllocations.back().get());
			blasCreateInfo.setOffset(0);
			blasCreateInfo.setSize(sizeInfo.accelerationStructureSize);
			blasCreateInfo.setType(vk::AccelerationStructureTypeKHR::eBottomLevel);

			try {
				result._allBlas.at(index) = dev.createAccelerationStructureKHRUnique(blasCreateInfo, nullptr, dynamicLoader);
			} catch (std::system_error e) {
				std::cout << "Error in create bottom level AS" << std::endl;
				exit(-1);
			}
			blasAccelerationBuildGeometryInfo.setDstAccelerationStructure(result._allBlas[index].get());

			vk::AccelerationStructureBuildRangeInfoKHR blasAccelerationBuildOffsetInfo;
			blasAccelerationBuildOffsetInfo.primitiveCount = primMesh.indexCount / 3;
			blasAccelerationBuildOffsetInfo.primitiveOffset = primMesh.firstIndex * sizeof(uint32_t);
			blasAccelerationBuildOffsetInfo.firstVertex = primMesh.vertexOffset;
			blasAccelerationBuildOffsetInfo.transformOffset = 0;

			{ // Add acceleration command buffer
				TransientCommandBuffer cmdBuf = cmdBufferPool.begin(queue);
				cmdBuf->buildAccelerationStructuresKHR(blasAccelerationBuildGeometryInfo, { &blasAccelerationBuildOffsetInfo }, dynamicLoader);
			}

			index++;
		}

		// Top level acceleration structure
		std::vector<vk::AccelerationStructureInstanceKHR> tlas;
		tlas.reserve(gltfScene.m_nodes.size());
		for (auto& node : gltfScene.m_nodes) {
			vk::AccelerationStructureInstanceKHR inst;
			for (std::size_t y = 0; y < 3; ++y) {
				for (std::size_t x = 0; x < 4; ++x) {
					inst.transform.matrix[y][x] = node.worldMatrix.mat_array[x * 4 + y]; // transposed
				}
			}
			inst.instanceCustomIndex = node.primMesh;
			inst.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
			inst.mask = 0xFF;
			inst.instanceShaderBindingTableRecordOffset = 0;
			inst.accelerationStructureReference = dev.getAccelerationStructureAddressKHR(result._allBlas[node.primMesh].get(), dynamicLoader);
			tlas.emplace_back(inst);
		}

		result._instance = _createMappedBuffer(
			tlas.data(),
			static_cast<uint32_t>(sizeof(vk::AccelerationStructureInstanceKHR) * tlas.size()),
			allocator,
			vk::BufferUsageFlagBits::eShaderDeviceAddress | vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR
		);

		vk::AccelerationStructureGeometryInstancesDataKHR instances;
		instances.arrayOfPointers = VK_FALSE;
		instances.data.deviceAddress = dev.getBufferAddress(result._instance.get());

		vk::AccelerationStructureGeometryKHR tlasAccelerationGeometry;
		tlasAccelerationGeometry.flags = vk::GeometryFlagBitsKHR::eOpaque;
		tlasAccelerationGeometry.geometryType = vk::GeometryTypeKHR::eInstances;
		tlasAccelerationGeometry.geometry.setInstances(instances);

		vk::AccelerationStructureBuildGeometryInfoKHR tlasAccelerationBuildGeometryInfo;
		tlasAccelerationBuildGeometryInfo.type = vk::AccelerationStructureTypeKHR::eTopLevel;
		tlasAccelerationBuildGeometryInfo.flags = vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace;
		tlasAccelerationBuildGeometryInfo.dstAccelerationStructure = result._topLevelAS.get();
		tlasAccelerationBuildGeometryInfo.setGeometries(tlasAccelerationGeometry);

		auto buildSize = dev.getAccelerationStructureBuildSizesKHR(
			vk::AccelerationStructureBuildTypeKHR::eDevice,
			tlasAccelerationBuildGeometryInfo,
			{ static_cast<std::uint32_t>(tlas.size()) },
			dynamicLoader);

		result._asAllocations.emplace_back(_createAccelerationStructureBuffer(
			buildSize.accelerationStructureSize, allocator
		));

		vk::AccelerationStructureCreateInfoKHR tlasAccelerationInfo;
		tlasAccelerationInfo.type = vk::AccelerationStructureTypeKHR::eTopLevel;
		tlasAccelerationInfo.setBuffer(result._asAllocations.back().get());
		tlasAccelerationInfo.setSize(buildSize.accelerationStructureSize);

		result._topLevelAS = dev.createAccelerationStructureKHRUnique(tlasAccelerationInfo, nullptr, dynamicLoader);
		tlasAccelerationBuildGeometryInfo.setDstAccelerationStructure(result._topLevelAS.get());

		vma::UniqueBuffer tlasScratchBuffer = _createScratchBuffer(buildSize.buildScratchSize, allocator);
		tlasAccelerationBuildGeometryInfo.scratchData.deviceAddress = dev.getBufferAddress(tlasScratchBuffer.get());

		vk::AccelerationStructureBuildRangeInfoKHR tlasAccelerationBuildOffsetInfo;
		tlasAccelerationBuildOffsetInfo.primitiveCount = static_cast<uint32_t>(tlas.size());
		tlasAccelerationBuildOffsetInfo.primitiveOffset = 0x0;
		tlasAccelerationBuildOffsetInfo.firstVertex = 0;
		tlasAccelerationBuildOffsetInfo.transformOffset = 0x0;

		// Add acceleration command buffer
		{
			TransientCommandBuffer cmdBuf = cmdBufferPool.begin(queue);

			vk::MemoryBarrier barrier;
			barrier
				.setSrcAccessMask(vk::AccessFlagBits::eTransferWrite)
				.setDstAccessMask(vk::AccessFlagBits::eAccelerationStructureReadKHR);
			cmdBuf->pipelineBarrier(vk::PipelineStageFlagBits::eTransfer,
				vk::PipelineStageFlagBits::eAccelerationStructureBuildKHR, {}, barrier, {}, {}, dynamicLoader);

			cmdBuf->buildAccelerationStructuresKHR(tlasAccelerationBuildGeometryInfo, &tlasAccelerationBuildOffsetInfo, dynamicLoader);
		}

		return result;
	}
private:
	vma::UniqueBuffer _instance;
	std::vector<vk::UniqueHandle<vk::AccelerationStructureKHR, vk::DispatchLoaderDynamic>> _allBlas;
	vk::UniqueHandle<vk::AccelerationStructureKHR, vk::DispatchLoaderDynamic> _topLevelAS;
	std::vector<vma::UniqueBuffer> _asAllocations;
	vma::Allocator *_allocator = nullptr;

	[[nodiscard]] inline static vma::UniqueBuffer _createMappedBuffer(
		void* srcData,
		uint32_t byteLength,
		vma::Allocator& allocator,
		vk::BufferUsageFlags usage = vk::BufferUsageFlagBits::eShaderDeviceAddress
	) {
		vk::BufferCreateInfo bufferInfo;
		bufferInfo
			.setSize(byteLength)
			.setUsage(usage)
			.setSharingMode(vk::SharingMode::eExclusive);

		VmaAllocationCreateInfo allocationInfo{};
		allocationInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;

		vma::UniqueBuffer mappedBuffer = allocator.createBuffer(bufferInfo, allocationInfo);
		void* dstData = mappedBuffer.map();
		if (srcData != nullptr) {
			memcpy(dstData, srcData, byteLength);
		}
		mappedBuffer.unmap();

		return mappedBuffer;
	}

	[[nodiscard]] inline static vma::UniqueBuffer _createAccelerationStructureBuffer(
		vk::DeviceSize size,
		vma::Allocator &allocator
	) {
		vk::BufferCreateInfo asBufferInfo;
		asBufferInfo
			.setSize(size)
			.setUsage(vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR | vk::BufferUsageFlagBits::eShaderDeviceAddress)
			.setSharingMode(vk::SharingMode::eExclusive);
		VmaAllocationCreateInfo allocationInfo{};
		allocationInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
		return allocator.createBuffer(asBufferInfo, allocationInfo);
	}

	[[nodiscard]] inline static vma::UniqueBuffer _createScratchBuffer(vk::DeviceSize size, vma::Allocator& allocator) {
		vk::BufferCreateInfo bufferInfo;
		bufferInfo
			.setSize(size)
			.setUsage(vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR |
				vk::BufferUsageFlagBits::eStorageBuffer |
				vk::BufferUsageFlagBits::eShaderDeviceAddress |
				vk::BufferUsageFlagBits::eTransferDst);
		VmaAllocationCreateInfo allocInfo{};
		allocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
		return allocator.createBuffer(bufferInfo, allocInfo);
	}
};
