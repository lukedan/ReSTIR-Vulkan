#version 450
#extension GL_ARB_separate_shader_objects: enable

#include "include/structs/aabbTree.glsl"
#include "include/structs/lightingPassStructs.glsl"
#include "include/gBufferDebugConstants.glsl"
#include "include/disneyBRDF.glsl"

layout (binding = 0) uniform sampler2D uniAlbedo;
layout (binding = 1) uniform sampler2D uniNormal;
layout (binding = 2) uniform sampler2D uniMaterialProperties;
layout (binding = 3) uniform sampler2D uniDepth;

layout (binding = 4) buffer AabbTreeNodes {
	int root;
	AabbTreeNode nodes[];
} aabbTree;
layout (binding = 5) buffer Triangles {
	Triangle triangles[];
};

layout (binding = 6) uniform Uniforms {
	LightingPassUniforms uniforms;
};

layout (location = 0) in vec2 inUv;

layout (location = 0) out vec4 outColor;


#define NODE_BUFFER aabbTree
#define TRIANGLE_BUFFER triangles
#include "include/softwareRaytracing.glsl"

#include "include/frustumUtils.glsl"


void main() {
	vec3 albedo = texture(uniAlbedo, inUv).xyz;
	vec3 normal = texture(uniNormal, inUv).xyz;
	float depth = texture(uniDepth, inUv).x;
	vec2 materialProps = texture(uniMaterialProperties, inUv).xy;

	float worldDepth = sceneDepthToWorldDepth(depth, uniforms.cameraNear, uniforms.cameraFar);
	vec3 viewPos = fragCoordDepthToViewPos(inUv, worldDepth, uniforms.tanHalfFovY, uniforms.aspectRatio);
	vec3 worldPos = (uniforms.inverseViewMatrix * vec4(viewPos, 1.0f)).xyz;

	if (uniforms.debugMode == GBUFFER_DEBUG_NONE) {
		outColor = vec4(albedo, 1.0f); // TODO
	} else if (uniforms.debugMode == GBUFFER_DEBUG_ALBEDO) {
		outColor = vec4(albedo, 1.0f);
	} else if (uniforms.debugMode == GBUFFER_DEBUG_NORMAL) {
		outColor = vec4((vec3(normal) + 1.0f) * 0.5f, 1.0f);
	} else if (uniforms.debugMode == GBUFFER_DEBUG_MATERIAL_PROPERTIES) {
		outColor = vec4(materialProps, 1.0f, 1.0f);
	} else if (uniforms.debugMode == GBUFFER_DEBUG_DISNEY_BRDF) {
		float roughness = materialProps.r;
		float metallic = materialProps.g;

		vec3 wi = uniforms.tempLightPoint.xyz - worldPos;
		float sqrDist = dot(wi, wi);
		wi /= sqrt(sqrDist);
		vec3 wo = normalize(vec3(uniforms.cameraPos) - worldPos);

		float cosIn = dot(normal, wi);
		float cosOut = dot(normal, wo);
		vec3 halfVec = normalize(wi + wo);
		float cosHalf = dot(normal, halfVec);
		float cosInHalf = dot(wi, halfVec);

		outColor = vec4(disneyBrdfColor(cosIn, cosOut, cosHalf, cosInHalf, albedo, roughness, metallic), 1.0) * abs(cosIn) / sqrDist;
	}

	vec3 rayDir = uniforms.tempLightPoint.xyz - worldPos;
	bool visible = raytrace(worldPos + 0.01 * rayDir, rayDir * 0.98);
	if (!visible) {
		outColor.xyz *= 0.5f;
	}
}
