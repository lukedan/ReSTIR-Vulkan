#version 450
#extension GL_ARB_separate_shader_objects: enable
#extension GL_EXT_scalar_block_layout : enable

#include "include/structs/sceneStructs.glsl"

layout (set = 2, binding = 0) uniform Material {
	MaterialUniforms material;
};

layout (set = 3, binding = 0) uniform sampler2D uniAlbedo;
layout (set = 3, binding = 1) uniform sampler2D uniNormal;
layout (set = 3, binding = 2) uniform sampler2D uniMaterial;

layout (location = 0) in vec3 inNormal;
layout (location = 1) in vec3 inTangent;
layout (location = 2) in vec4 inColor;
layout (location = 3) in vec2 inUv;

layout (location = 0) out vec3 outAlbedo;
layout (location = 1) out vec3 outNormal;
layout (location = 2) out vec3 outMaterialProperties;

void main() {
	// compute baseColor or diffuse
	vec4 albedo = texture(uniAlbedo, inUv) * material.colorParam;
	if (material.alphaMode == ALPHA_MODE_MASK) {
		if (albedo.a < material.alphaCutoff) {
			discard;
		}
	}
	// the deferred pipeline doesn't support transparency; set alpha to 1
	outAlbedo = albedo.rgb;

	// compute normal
	// this front facing flag may come in handy later when handling double-sided geometry
	vec3 bitangent = /*(gl_FrontFacing ? 1.0f : -1.0f) **/ cross(inNormal, inTangent);
	vec3 normalTex = texture(uniNormal, inUv * material.normalTextureScale).xyz * 2.0f - 1.0f;
	outNormal = normalize(normalTex.x * inTangent + normalTex.y * bitangent + normalTex.z * inNormal);

	// compute material properties
	vec4 materialProp = texture(uniMaterial, inUv) * material.materialParam;
	float roughness = 0.0f;
	float metallic = 0.0f;
	if (material.shadingModel == SHADING_MODEL_METALLIC_ROUGHNESS) {
		roughness = materialProp.y;
		metallic = materialProp.z;
	} else if (material.shadingModel == SHADING_MODEL_SPECULAR_GLOSSINESS) {
		roughness = 1.0f - materialProp.a;

		// TODO get metallic and adjust albedo
	}
}
