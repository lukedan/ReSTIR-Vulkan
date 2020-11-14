#pragma once
#include "tiny_gltf.h"
#include "nvmath.h"

#define EXTENSION_LIGHT "KHR_lights_punctual"

enum class GltfAttributes : uint8_t
{
	Position = 0,
	Normal = 1,
	Texcoord_0 = 2,
	Texcoord_1 = 4,
	Tangent = 8,
	Color_0 = 16,
	//Joints_0   = 32, // #TODO - Add support for skinning
	//Weights_0  = 64,
};
using GltfAttributes_t = std::underlying_type_t<GltfAttributes>;
inline GltfAttributes operator|(GltfAttributes lhs, GltfAttributes rhs)
{
	return static_cast<GltfAttributes>(static_cast<GltfAttributes_t>(lhs) | static_cast<GltfAttributes_t>(rhs));
}

inline GltfAttributes operator&(GltfAttributes lhs, GltfAttributes rhs)
{
	return static_cast<GltfAttributes>(static_cast<GltfAttributes_t>(lhs) & static_cast<GltfAttributes_t>(rhs));
}


struct GltfPrimMesh
{
	uint32_t firstIndex{ 0 };
	uint32_t indexCount{ 0 };
	uint32_t vertexOffset{ 0 };
	uint32_t vertexCount{ 0 };
	int      materialIndex{ 0 };

	nvmath::vec3f posMin{ 0, 0, 0 };
	nvmath::vec3f posMax{ 0, 0, 0 };
};

struct GltfNode
{
    nvmath::mat4f worldMatrix{ 1 };
    int           primMesh{ 0 };
};

struct GltfCamera
{
    nvmath::mat4f    worldMatrix{ 1 };
    tinygltf::Camera cam;
};

struct GltfLight
{
    nvmath::mat4f   worldMatrix{ 1 };
    tinygltf::Light light;
};

class GltfScene {
public:
	// Attributes, all same length if valid
	std::vector<nvmath::vec3f> m_positions;
	std::vector<uint32_t>      m_indices;
	std::vector<nvmath::vec3f> m_normals;

    // Scene data
    std::vector<GltfNode>     m_nodes;       // Drawable nodes, flat hierarchy
    std::vector<GltfPrimMesh> m_primMeshes;  // Primitive promoted to meshes
    std::vector<GltfCamera>   m_cameras;
    std::vector<GltfLight>    m_lights;

    // Size of the scene
    struct Dimensions
    {
        nvmath::vec3f min = nvmath::vec3f(std::numeric_limits<float>::max());
        nvmath::vec3f max = nvmath::vec3f(std::numeric_limits<float>::min());
        nvmath::vec3f size{ 0.f };
        nvmath::vec3f center{ 0.f };
        float         radius{ 0 };
    } m_dimensions;

	void importDrawableNodes(const tinygltf::Model& tmodel, GltfAttributes attributes);


private:
	// Temporary data
	std::unordered_map<int, std::vector<uint32_t>> m_meshToPrimMeshes;
	std::vector<uint32_t>                          primitiveIndices32u;
	std::vector<uint16_t>                          primitiveIndices16u;
	std::vector<uint8_t>                           primitiveIndices8u;

	void processMesh(const tinygltf::Model& tmodel, const tinygltf::Primitive& tmesh, GltfAttributes attributes);
    void computeSceneDimensions();
    nvmath::mat4f getLocalMatrix(const tinygltf::Node& tnode);
    void processNode(const tinygltf::Model& tmodel, int& nodeIdx, const nvmath::mat4f& parentMatrix);

    // Appending to \p attribVec, all the values of \p attribName
    // Return false if the attribute is missing
    template <typename T>
    bool getAttribute(const tinygltf::Model& tmodel, const tinygltf::Primitive& primitive, std::vector<T>& attribVec, const std::string& attribName)
    {
        if (primitive.attributes.find(attribName) == primitive.attributes.end())
            return false;

        // Retrieving the data of the attribute
        const auto& accessor = tmodel.accessors[primitive.attributes.find(attribName)->second];
        const auto& bufView = tmodel.bufferViews[accessor.bufferView];
        const auto& buffer = tmodel.buffers[bufView.buffer];
        const auto  bufData = reinterpret_cast<const T*>(&(buffer.data[accessor.byteOffset + bufView.byteOffset]));
        const auto  nbElems = accessor.count;

        assert(accessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT);

        // Copying the attributes
        if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT)
        {
            if (bufView.byteStride == 0)
            {
                attribVec.insert(attribVec.end(), bufData, bufData + nbElems);
            }
            else
            {
                // With stride, need to add one by one the element
                auto bufferByte = reinterpret_cast<const uint8_t*>(bufData);
                for (size_t i = 0; i < nbElems; i++)
                {
                    attribVec.push_back(*reinterpret_cast<const T*>(bufferByte));
                    bufferByte += bufView.byteStride;
                }
            }
        }
        else
        {
            // The component is smaller than float and need to be converted

            // VEC3 or VEC4
            int nbComponents = accessor.type == TINYGLTF_TYPE_VEC2 ? 2 : (accessor.type == TINYGLTF_TYPE_VEC3) ? 3 : 4;
            // UNSIGNED_BYTE or UNSIGNED_SHORT
            int strideComponent = accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE ? 1 : 2;

            size_t byteStride = bufView.byteStride > 0 ? bufView.byteStride : nbComponents * strideComponent;
            auto   bufferByte = reinterpret_cast<const uint8_t*>(bufData);
            for (size_t i = 0; i < nbElems; i++)
            {
                T vecValue;

                auto bufferByteData = bufferByte;
                for (int c = 0; c < nbComponents; c++)
                {
                    float value = *reinterpret_cast<const float*>(bufferByteData);
                    vecValue[c] = accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE ? value / 255.0f : value / 65535.0f;
                    bufferByteData += strideComponent;
                }
                bufferByte += byteStride;
                attribVec.push_back(vecValue);
            }
        }


        return true;
    }
};



