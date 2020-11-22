#version 450
#extension GL_ARB_separate_shader_objects: enable

#include "include/structs/aabbTree.glsl"

layout (binding = 0) uniform sampler2D uniAlbedo;
layout (binding = 1) uniform sampler2D uniNormal;
layout (binding = 2) uniform sampler2D uniDepth;

layout (binding = 3) buffer AabbTreeNodes {
	int root;
	AabbTreeNode data[];
} nodes;
layout (binding = 4) buffer Triangles {
	Triangle data[];
} triangles;

layout (binding = 5) uniform Uniforms {
	mat4 inverseViewMatrix;
	vec4 tempLightPoint;
	float cameraNear;
	float cameraFar;
	float tanHalfFovY;
	float aspectRatio;
} uniforms;

layout (location = 0) in vec2 inUv;

layout (location = 0) out vec4 outColor;


#define NODE_BUFFER nodes
#define TRIANGLE_BUFFER triangles
#include "include/softwareRaytracing.glsl"

#include "include/frustumUtils.glsl"


void main() {
	vec3 albedo = texture(uniAlbedo, inUv).xyz;
	vec3 normal = texture(uniNormal, inUv).xyz;
	float depth = texture(uniDepth, inUv).x;

	float worldDepth = sceneDepthToWorldDepth(depth, uniforms.cameraNear, uniforms.cameraFar);
	vec3 viewPos = fragCoordDepthToViewPos(inUv, worldDepth, uniforms.tanHalfFovY, uniforms.aspectRatio);
	vec3 worldPos = (uniforms.inverseViewMatrix * vec4(viewPos, 1.0f)).xyz;

	outColor = vec4((vec3(normal) + 1.0f) * 0.5f, 1.0f);
	/*outColor = vec4(depth, depth, depth, 1.0f);*/
	/*outColor = vec4(albedo, 1.0f);*/
	/*outColor = vec4(vec3(worldDepth / 10.0f), 1.0f);*/
	/*outColor = vec4(worldPos / 10.0f + 0.5f, 1.0f);*/

	vec3 rayDir = uniforms.tempLightPoint.xyz - worldPos;
	bool visible = raytrace(worldPos + 0.01 * rayDir, rayDir * 0.98);
	if (!visible) {
		outColor.xyz *= 0.5f;
	}
}
