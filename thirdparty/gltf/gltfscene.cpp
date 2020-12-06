/* Copyright (c) 2014-2019, NVIDIA CORPORATION. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *  * Neither the name of NVIDIA CORPORATION nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "gltfscene.h"
#include "mikktWrapper.h"
#include <iostream>
#include <numeric>

namespace nvh {
    //--------------------------------------------------------------------------------------------------
    // Collect the value of all materials
    //
    void GltfScene::importMaterials(const tinygltf::Model& tmodel)
    {
        m_materials.reserve(tmodel.materials.size());

        for (auto& tmat : tmodel.materials)
        {
            GltfMaterial gmat;

            gmat.alphaCutoff = static_cast<float>(tmat.alphaCutoff);
            gmat.alphaMode = tmat.alphaMode == "MASK" ? 1 : (tmat.alphaMode == "BLEND" ? 2 : 0);
            gmat.doubleSided = tmat.doubleSided;
            gmat.emissiveFactor = nvmath::vec3f(tmat.emissiveFactor[0], tmat.emissiveFactor[1], tmat.emissiveFactor[2]);
            gmat.emissiveTexture = tmat.emissiveTexture.index;
            gmat.normalTexture = tmat.normalTexture.index;
            gmat.normalTextureScale = static_cast<float>(tmat.normalTexture.scale);
            gmat.occlusionTexture = tmat.occlusionTexture.index;
            gmat.occlusionTextureStrength = static_cast<float>(tmat.occlusionTexture.strength);

            // PbrMetallicRoughness
            auto& tpbr = tmat.pbrMetallicRoughness;
            gmat.pbrBaseColorFactor =
                nvmath::vec4f(tpbr.baseColorFactor[0], tpbr.baseColorFactor[1], tpbr.baseColorFactor[2], tpbr.baseColorFactor[3]);
            gmat.pbrBaseColorTexture = tpbr.baseColorTexture.index;
            gmat.pbrMetallicFactor = static_cast<float>(tpbr.metallicFactor);
            gmat.pbrMetallicRoughnessTexture = tpbr.metallicRoughnessTexture.index;
            gmat.pbrRoughnessFactor = static_cast<float>(tpbr.roughnessFactor);

            // KHR_materials_pbrSpecularGlossiness
            if (tmat.extensions.find("KHR_materials_pbrSpecularGlossiness") != tmat.extensions.end())
            {
                gmat.shadingModel = 1;

                const auto& khr = tmat.extensions.find("KHR_materials_pbrSpecularGlossiness")->second;
                if (khr.Has("diffuseFactor"))
                {
                    auto vec = getVector<float>(khr.Get("diffuseFactor"));
                    gmat.khrDiffuseFactor = nvmath::vec4f(vec[0], vec[1], vec[2], vec[3]);
                }
                if (khr.Has("glossinessFactor"))
                {
                    gmat.khrGlossinessFactor = static_cast<float>(khr.Get("glossinessFactor").GetNumberAsDouble());
                }
                if (khr.Has("specularFactor"))
                {
                    auto vec = getVector<float>(khr.Get("specularFactor"));
                    gmat.khrSpecularFactor = nvmath::vec3f(vec[0], vec[1], vec[2]);
                }
                if (khr.Has("diffuseTexture"))
                {
                    gmat.khrDiffuseTexture = khr.Get("diffuseTexture").Get("index").Get<int>();
                }
            }

            m_materials.emplace_back(gmat);
        }

        // Make default
        if (m_materials.empty())
        {
            GltfMaterial gmat;
            gmat.pbrMetallicFactor = 0;
            m_materials.emplace_back(gmat);
        }
    }

    //--------------------------------------------------------------------------------------------------
    // Linearize the scene graph to world space nodes.
    //
    void GltfScene::importDrawableNodes(const tinygltf::Model& tmodel, GltfAttributes attributes)
    {
        // Find the number of vertex(attributes) and index
        uint32_t nbVert{ 0 };
        uint32_t nbIndex{ 0 };
        uint32_t meshCnt{ 0 };  // use for mesh to new meshes
        uint32_t primCnt{ 0 };  //  "   "  "  "
        for (const auto& mesh : tmodel.meshes)
        {
            std::vector<uint32_t> vprim;
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
        // m_tangents.resize(nbVert, nvmath::vec4f(0.f, 0.f, 0.f, 0.f));
        m_positions.reserve(nbVert);
        m_indices.reserve(nbIndex);
        if ((attributes & GltfAttributes::Normal) == GltfAttributes::Normal)
            m_normals.reserve(nbVert);
        if ((attributes & GltfAttributes::Texcoord_0) == GltfAttributes::Texcoord_0)
            m_texcoords0.reserve(nbVert);
        if ((attributes & GltfAttributes::Tangent) == GltfAttributes::Tangent)
            m_tangents.reserve(nbVert);
        if ((attributes & GltfAttributes::Color_0) == GltfAttributes::Color_0)
            m_colors0.reserve(nbVert);

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

    //--------------------------------------------------------------------------------------------------
    // Extracting the values to a linear buffer
    //
    void GltfScene::processMesh(const tinygltf::Model& tmodel, const tinygltf::Primitive& tmesh, GltfAttributes attributes)
    {
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

        // TEXCOORD_0
        if ((attributes & GltfAttributes::Texcoord_0) == GltfAttributes::Texcoord_0)
        {
            if (!getAttribute<nvmath::vec2f>(tmodel, tmesh, m_texcoords0, "TEXCOORD_0"))
            {
                // Set them all to zero
                //      m_texcoords0.insert(m_texcoords0.end(), resultMesh.vertexCount, nvmath::vec2f(0, 0));

                // Cube map projection
                for (uint32_t i = 0; i < resultMesh.vertexCount; i++)
                {
                    const auto& pos = m_positions[resultMesh.vertexOffset + i];
                    float       absX = fabs(pos.x);
                    float       absY = fabs(pos.y);
                    float       absZ = fabs(pos.z);

                    int isXPositive = pos.x > 0 ? 1 : 0;
                    int isYPositive = pos.y > 0 ? 1 : 0;
                    int isZPositive = pos.z > 0 ? 1 : 0;

                    float maxAxis, uc, vc;

                    // POSITIVE X
                    if (isXPositive && absX >= absY && absX >= absZ)
                    {
                        // u (0 to 1) goes from +z to -z
                        // v (0 to 1) goes from -y to +y
                        maxAxis = absX;
                        uc = -pos.z;
                        vc = pos.y;
                    }
                    // NEGATIVE X
                    if (!isXPositive && absX >= absY && absX >= absZ)
                    {
                        // u (0 to 1) goes from -z to +z
                        // v (0 to 1) goes from -y to +y
                        maxAxis = absX;
                        uc = pos.z;
                        vc = pos.y;
                    }
                    // POSITIVE Y
                    if (isYPositive && absY >= absX && absY >= absZ)
                    {
                        // u (0 to 1) goes from -x to +x
                        // v (0 to 1) goes from +z to -z
                        maxAxis = absY;
                        uc = pos.x;
                        vc = -pos.z;
                    }
                    // NEGATIVE Y
                    if (!isYPositive && absY >= absX && absY >= absZ)
                    {
                        // u (0 to 1) goes from -x to +x
                        // v (0 to 1) goes from -z to +z
                        maxAxis = absY;
                        uc = pos.x;
                        vc = pos.z;
                    }
                    // POSITIVE Z
                    if (isZPositive && absZ >= absX && absZ >= absY)
                    {
                        // u (0 to 1) goes from -x to +x
                        // v (0 to 1) goes from -y to +y
                        maxAxis = absZ;
                        uc = pos.x;
                        vc = pos.y;
                    }
                    // NEGATIVE Z
                    if (!isZPositive && absZ >= absX && absZ >= absY)
                    {
                        // u (0 to 1) goes from +x to -x
                        // v (0 to 1) goes from -y to +y
                        maxAxis = absZ;
                        uc = -pos.x;
                        vc = pos.y;
                    }

                    // Convert range from -1 to 1 to 0 to 1
                    float u = 0.5f * (uc / maxAxis + 1.0f);
                    float v = 0.5f * (vc / maxAxis + 1.0f);

                    m_texcoords0.emplace_back(u, v);
                }
            }
        }


        // TANGENT
        if ((attributes & GltfAttributes::Tangent) == GltfAttributes::Tangent)
        {
            if (!getAttribute<nvmath::vec4f>(tmodel, tmesh, m_tangents, "TANGENT"))
            {
                // Default MikkTSpace algorithms
                // See: https://github.com/mmikk/MikkTSpace
                std::vector<nvmath::vec4f> localTangents(resultMesh.vertexCount);
                genTangents(&resultMesh, m_indices.data(), m_positions.data(), m_normals.data(), m_texcoords0.data(), localTangents.data());
                m_tangents.insert(m_tangents.end(), localTangents.begin(), localTangents.end());
            }
        }

        // COLOR_0
        if ((attributes & GltfAttributes::Color_0) == GltfAttributes::Color_0)
        {
            if (!getAttribute<nvmath::vec4f>(tmodel, tmesh, m_colors0, "COLOR_0"))
            {
                // Set them all to one
                m_colors0.insert(m_colors0.end(), resultMesh.vertexCount, nvmath::vec4f(1, 1, 1, 1));
            }
        }

        m_primMeshes.emplace_back(resultMesh);
    }  // namespace nvh

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


    void GltfScene::destroy()
    {
        m_materials.clear();
        m_nodes.clear();
        m_primMeshes.clear();
        m_cameras.clear();
        m_lights.clear();

        m_positions.clear();
        m_indices.clear();
        m_normals.clear();
        m_tangents.clear();
        m_texcoords0.clear();
        m_texcoords1.clear();
        m_colors0.clear();
        m_cameras.clear();
        //m_joints0.clear();
        //m_weights0.clear();
        m_dimensions = {};
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


    static uint32_t recursiveTriangleCount(const tinygltf::Model& model, int nodeIdx, const std::vector<uint32_t>& meshTriangle)
    {
        auto& node = model.nodes[nodeIdx];
        uint32_t nbTriangles{ 0 };
        for (const auto child : node.children)
        {
            nbTriangles += recursiveTriangleCount(model, child, meshTriangle);
        }

        if (node.mesh >= 0)
            nbTriangles += meshTriangle[node.mesh];

        return nbTriangles;
    }

    //--------------------------------------------------------------------------------------------------
    // Retrieving information about the scene
    //
    GltfStats GltfScene::getStatistics(const tinygltf::Model& tinyModel)
    {
        GltfStats stats;

        stats.nbCameras = static_cast<uint32_t>(tinyModel.cameras.size());
        stats.nbImages = static_cast<uint32_t>(tinyModel.images.size());
        stats.nbTextures = static_cast<uint32_t>(tinyModel.textures.size());
        stats.nbMaterials = static_cast<uint32_t>(tinyModel.materials.size());
        stats.nbSamplers = static_cast<uint32_t>(tinyModel.samplers.size());
        stats.nbNodes = static_cast<uint32_t>(tinyModel.nodes.size());
        stats.nbMeshes = static_cast<uint32_t>(tinyModel.meshes.size());
        stats.nbLights = static_cast<uint32_t>(tinyModel.lights.size());

        // Computing the memory usage for images
        for (const auto& image : tinyModel.images)
        {
            stats.imageMem += image.width * image.height * image.component * image.bits / 8;
        }

        // Computing the number of triangles
        std::vector<uint32_t> meshTriangle(tinyModel.meshes.size());
        uint32_t              meshIdx{ 0 };
        for (const auto& mesh : tinyModel.meshes)
        {
            for (const auto& primitive : mesh.primitives)
            {
                const tinygltf::Accessor& indexAccessor = tinyModel.accessors[primitive.indices];
                meshTriangle[meshIdx] += static_cast<uint32_t>(indexAccessor.count) / 3;
            }
            meshIdx++;
        }

        stats.nbUniqueTriangles = std::accumulate(meshTriangle.begin(), meshTriangle.end(), 0, std::plus<>());
        for (auto& node : tinyModel.scenes[0].nodes)
        {
            stats.nbTriangles += recursiveTriangleCount(tinyModel, node, meshTriangle);
        }

        return stats;
    }

    void GltfScene::importTexutureImages(tinygltf::Model& gltfModel)
    {
        if (!gltfModel.images.empty()) {
            m_textures.reserve(gltfModel.images.size());
            for (size_t i = 0; i < gltfModel.images.size(); i++) {
                m_textures.emplace_back(gltfModel.images[i]);
            }
        }        
    }

}  // namespace nvh
