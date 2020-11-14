#include<iostream>
#include "gltfscene.h"
#include "nvmath_glsltypes.h"

void GltfScene::processMesh(const tinygltf::Model& tmodel, const tinygltf::Primitive& tmesh, GltfAttributes attributes) {
	GltfPrimMesh resultMesh;
	resultMesh.materialIndex = std::max(0, tmesh.material);
	resultMesh.vertexOffset = static_cast<uint32_t>(m_positions.size());
	resultMesh.firstIndex = static_cast<uint32_t>(m_indices.size());

	// Only triangles are supported
    // 0:point, 1:lines, 2:line_loop, 3:line_strip, 4:triangles, 5:triangle_strip, 6:triangle_fan
	if (tmesh.mode != 4)
		return;

	// INDICES
	{
		const tinygltf::Accessor& indexAccessor = tmodel.accessors[tmesh.indices];
		const tinygltf::BufferView& bufferView = tmodel.bufferViews[indexAccessor.bufferView];
		const tinygltf::Buffer& buffer = tmodel.buffers[bufferView.buffer];

		resultMesh.indexCount = static_cast<uint32_t>(indexAccessor.count);

		switch (indexAccessor.componentType)
		{
		case TINYGLTF_PARAMETER_TYPE_UNSIGNED_INT: {
			primitiveIndices32u.resize(indexAccessor.count);
			memcpy(primitiveIndices32u.data(), &buffer.data[indexAccessor.byteOffset + bufferView.byteOffset],
				indexAccessor.count * sizeof(uint32_t));
			m_indices.insert(m_indices.end(), primitiveIndices32u.begin(), primitiveIndices32u.end());
			break;
		}
		case TINYGLTF_PARAMETER_TYPE_UNSIGNED_SHORT: {
			primitiveIndices16u.resize(indexAccessor.count);
			memcpy(primitiveIndices16u.data(), &buffer.data[indexAccessor.byteOffset + bufferView.byteOffset],
				indexAccessor.count * sizeof(uint16_t));
			m_indices.insert(m_indices.end(), primitiveIndices16u.begin(), primitiveIndices16u.end());
			break;
		}
		case TINYGLTF_PARAMETER_TYPE_UNSIGNED_BYTE: {
			primitiveIndices8u.resize(indexAccessor.count);
			memcpy(primitiveIndices8u.data(), &buffer.data[indexAccessor.byteOffset + bufferView.byteOffset],
				indexAccessor.count * sizeof(uint8_t));
			m_indices.insert(m_indices.end(), primitiveIndices8u.begin(), primitiveIndices8u.end());
			break;
		}
		default:
			std::cerr << "Index component type " << indexAccessor.componentType << " not supported!" << std::endl;
			return;
		}
	}

	// POSITION
	{
		bool result = getAttribute<nvmath::vec3f>(tmodel, tmesh, m_positions, "POSITION");

		// Keeping the size of this primitive (Spec says this is required information)
		const auto& accessor = tmodel.accessors[tmesh.attributes.find("POSITION")->second];
		resultMesh.vertexCount = static_cast<uint32_t>(accessor.count);
		if (!accessor.minValues.empty())
			resultMesh.posMin = nvmath::vec3f(accessor.minValues[0], accessor.minValues[1], accessor.minValues[2]);
		if (!accessor.maxValues.empty())
			resultMesh.posMax = nvmath::vec3f(accessor.maxValues[0], accessor.maxValues[1], accessor.maxValues[2]);
	}

	// NORMAL
	if ((attributes & GltfAttributes::Normal) == GltfAttributes::Normal)
	{
		if (!getAttribute<nvmath::vec3f>(tmodel, tmesh, m_normals, "NORMAL"))
		{
			// Need to compute the normals
			std::vector<nvmath::vec3> geonormal(resultMesh.vertexCount);
			for (size_t i = 0; i < resultMesh.indexCount; i += 3)
			{
				uint32_t    ind0 = m_indices[resultMesh.firstIndex + i + 0];
				uint32_t    ind1 = m_indices[resultMesh.firstIndex + i + 1];
				uint32_t    ind2 = m_indices[resultMesh.firstIndex + i + 2];
				const auto& pos0 = m_positions[ind0 + resultMesh.vertexOffset];
				const auto& pos1 = m_positions[ind1 + resultMesh.vertexOffset];
				const auto& pos2 = m_positions[ind2 + resultMesh.vertexOffset];
				const auto  v1 = nvmath::normalize(pos1 - pos0);  // Many normalize, but when objects are really small the
				const auto  v2 = nvmath::normalize(pos2 - pos0);  // cross will go below nv_eps and the normal will be (0,0,0)
				const auto  n = nvmath::cross(v2, v1);
				geonormal[ind0] += n;
				geonormal[ind1] += n;
				geonormal[ind2] += n;
			}
			for (auto& n : geonormal)
				n = nvmath::normalize(n);
			m_normals.insert(m_normals.end(), geonormal.begin(), geonormal.end());
		}
	}
	
	
	m_primMeshes.emplace_back(resultMesh);
}

//--------------------------------------------------------------------------------------------------
// Get the dimension of the scene
//
void GltfScene::computeSceneDimensions()
{
	auto valMin = nvmath::vec3f(FLT_MAX);
	auto valMax = nvmath::vec3f(-FLT_MAX);
	for (const auto& node : m_nodes)
	{
		const auto& mesh = m_primMeshes[node.primMesh];

		nvmath::vec4f locMin = node.worldMatrix * nvmath::vec4f(mesh.posMin, 1.0f);
		nvmath::vec4f locMax = node.worldMatrix * nvmath::vec4f(mesh.posMax, 1.0f);

		valMin = { std::min(locMin.x, valMin.x), std::min(locMin.y, valMin.y), std::min(locMin.z, valMin.z) };
		valMax = { std::max(locMax.x, valMax.x), std::max(locMax.y, valMax.y), std::max(locMax.z, valMax.z) };
	}
	if (valMin == valMax)
	{
		valMin = nvmath::vec3f(-1);
		valMin = nvmath::vec3f(1);
	}
	m_dimensions.min = valMin;
	m_dimensions.max = valMax;
	m_dimensions.size = valMax - valMin;
	m_dimensions.center = (valMin + valMax) / 2.0f;
	m_dimensions.radius = nvmath::length(valMax - valMin) / 2.0f;
}

//--------------------------------------------------------------------------------------------------
// Return the matrix of the node
//
nvmath::mat4f GltfScene::getLocalMatrix(const tinygltf::Node& tnode)
{
	nvmath::mat4f mtranslation{ 1 };
	nvmath::mat4f mscale{ 1 };
	nvmath::mat4f mrot{ 1 };
	nvmath::mat4f matrix{ 1 };
	nvmath::quatf mrotation;

	if (!tnode.translation.empty())
		mtranslation.as_translation(nvmath::vec3f(tnode.translation[0], tnode.translation[1], tnode.translation[2]));
	if (!tnode.scale.empty())
		mscale.as_scale(nvmath::vec3f(tnode.scale[0], tnode.scale[1], tnode.scale[2]));
	if (!tnode.rotation.empty())
	{
		mrotation[0] = static_cast<float>(tnode.rotation[0]);
		mrotation[1] = static_cast<float>(tnode.rotation[1]);
		mrotation[2] = static_cast<float>(tnode.rotation[2]);
		mrotation[3] = static_cast<float>(tnode.rotation[3]);
		mrotation.to_matrix(mrot);
	}
	if (!tnode.matrix.empty())
	{
		for (int i = 0; i < 16; ++i)
			matrix.mat_array[i] = static_cast<float>(tnode.matrix[i]);
	}
	return mtranslation * mrot * mscale * matrix;
}


//--------------------------------------------------------------------------------------------------
//
//
void GltfScene::processNode(const tinygltf::Model& tmodel, int& nodeIdx, const nvmath::mat4f& parentMatrix)
{
	const auto& tnode = tmodel.nodes[nodeIdx];

	nvmath::mat4f matrix = getLocalMatrix(tnode);
	nvmath::mat4f worldMatrix = parentMatrix * matrix;

	if (tnode.mesh > -1)
	{
		const auto& meshes = m_meshToPrimMeshes[tnode.mesh];  // A mesh could have many primitives
		for (const auto& mesh : meshes)
		{
			GltfNode node;
			node.primMesh = mesh;
			node.worldMatrix = worldMatrix;
			m_nodes.emplace_back(node);
		}
	}
	else if (tnode.camera > -1)
	{
		GltfCamera camera;
		camera.worldMatrix = worldMatrix;
		camera.cam = tmodel.cameras[tmodel.nodes[nodeIdx].camera];
		m_cameras.emplace_back(camera);
	}
	else if (tnode.extensions.find(EXTENSION_LIGHT) != tnode.extensions.end())
	{
		GltfLight   light;
		const auto& ext = tnode.extensions.find(EXTENSION_LIGHT)->second;
		auto        lightIdx = ext.Get("light").GetNumberAsInt();
		light.light = tmodel.lights[lightIdx];
		light.worldMatrix = worldMatrix;
		m_lights.emplace_back(light);
	}

	// Recursion for all children
	for (auto child : tnode.children)
	{
		processNode(tmodel, child, worldMatrix);
	}
}


void GltfScene::importDrawableNodes(const tinygltf::Model& tmodel, GltfAttributes attributes) {
	std::cout << "mesh number:" << tmodel.meshes.size() << std::endl;

	// Find the number of vertex(attributes), index, meshcount and primitives count
	uint32_t nbVert{ 0 };
	uint32_t nbIndex{ 0 };
	uint32_t meshCnt{ 0 }; 
	uint32_t primCnt{ 0 };
	for (const auto& mesh : tmodel.meshes) {
		std::vector<uint32_t> vprim;
		// std::cout << "prim number:" << mesh.primitives.size();
		for (const auto& primitive : mesh.primitives)
		{
			if (primitive.mode != 4)  // Triangle
				continue;
			const auto& posAccessor = tmodel.accessors[primitive.attributes.find("POSITION")->second];
			nbVert += static_cast<uint32_t>(posAccessor.count);
			const auto& indexAccessor = tmodel.accessors[primitive.indices];
			nbIndex += static_cast<uint32_t>(indexAccessor.count);
			vprim.emplace_back(primCnt++);
		}
		m_meshToPrimMeshes[meshCnt++] = std::move(vprim);  // mesh-id = { prim0, prim1, ... }
	}

	// Reserving memory
	m_positions.reserve(nbVert);
	m_indices.reserve(nbIndex);
	if ((attributes & GltfAttributes::Normal) == GltfAttributes::Normal)
	{
		m_normals.reserve(nbVert);
	}
	
	// Convert all mesh/primitives+ to a single primitive per mesh
	for (const auto& tmesh : tmodel.meshes)
	{
		for (const auto& tprimitive : tmesh.primitives)
		{
			processMesh(tmodel, tprimitive, attributes);
		}
	}

	// Transforming the scene hierarchy to a flat list
	int         defaultScene = tmodel.defaultScene > -1 ? tmodel.defaultScene : 0;
	const auto& tscene = tmodel.scenes[defaultScene];

	for (auto nodeIdx : tscene.nodes)
	{
		processNode(tmodel, nodeIdx, nvmath::mat4f(1));
	}

	computeSceneDimensions();

	m_meshToPrimMeshes.clear();
	primitiveIndices32u.clear();
	primitiveIndices16u.clear();
	primitiveIndices8u.clear();
}