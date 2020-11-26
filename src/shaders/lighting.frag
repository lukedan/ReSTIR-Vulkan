#version 450
#extension GL_ARB_separate_shader_objects: enable
#extension GL_ARB_shader_clock : enable


#include "include/structs/aabbTree.glsl"
#include "include/structs/lightingPassStructs.glsl"
#include "include/gBufferDebugConstants.glsl"
#include "include/structs/light.glsl"

layout (binding = 0) uniform sampler2D uniAlbedo;
layout (binding = 1) uniform sampler2D uniNormal;
layout (binding = 2) uniform sampler2D uniDepth;

layout (binding = 3) buffer AabbTreeNodes {
	int root;
	AabbTreeNode nodes[];
} aabbTree;
layout (binding = 4) buffer Triangles {
	Triangle triangles[];
};

layout (binding = 5) uniform Uniforms {
	LightingPassUniforms uniforms;
};

layout (location = 0) in vec2 inUv;

layout (location = 0) out vec4 outColor;


#define NODE_BUFFER aabbTree
#define TRIANGLE_BUFFER triangles
#define PI 3.1415926
#include "include/softwareRaytracing.glsl"

#include "include/frustumUtils.glsl"
#include "include/RIS.glsl"

void main() {
	vec3 albedo = texture(uniAlbedo, inUv).xyz;
	vec3 normal = texture(uniNormal, inUv).xyz;
	float depth = texture(uniDepth, inUv).x;

	float worldDepth = sceneDepthToWorldDepth(depth, uniforms.cameraNear, uniforms.cameraFar);
	vec3 viewPos = fragCoordDepthToViewPos(inUv, worldDepth, uniforms.tanHalfFovY, uniforms.aspectRatio);
	vec3 worldPos = (uniforms.inverseViewMatrix * vec4(viewPos, 1.0f)).xyz;

	vec3 tempColor = vec3(0.0, 0.0, 0.0);
	int sample_num = uniforms.sample_num;
	if (uniforms.debugMode == GBUFFER_DEBUG_NONE) {
		outColor = vec4(albedo, 1.0f); // TODO
	} else if (uniforms.debugMode == GBUFFER_DEBUG_ALBEDO) {
		outColor = vec4(albedo, 1.0f);
	} else if (uniforms.debugMode == GBUFFER_DEBUG_NORMAL) {
		outColor = vec4((vec3(normal) + 1.0f) * 0.5f, 1.0f);
	}

	/*
	for(int i = 0; i < sample_num; ++i){
		// Lights parameters and init
		int lightNum = uniforms.lightNum;
		uint seed = uint(int(clockARB()) + int(inUv.x * 1000.0) + int(inUv.y * 13300.0));
		float tempFloatRnd = rnd(seed);
		int selectedIdx = int(tempFloatRnd * lightNum * 100.0) % lightNum;
	
		vec3 tempLightPos = uniforms.lightsArray[selectedIdx].pos;
		vec3 tempLightIntensity = uniforms.lightsArray[selectedIdx].color * uniforms.lightsArray[selectedIdx].intensity;

		vec3 rayDir = tempLightPos - worldPos;
		bool visible = raytrace(worldPos + 0.01 * rayDir, rayDir * 0.98);
		
		if (visible) {
			// Direct light shading
			vec3 f = albedo / PI;
			float pdf = length(tempLightPos - worldPos);
			pdf = pdf * pdf;
			tempColor += f * tempLightIntensity * abs(dot(normalize(rayDir), normal)) / (pdf * lightNum);
		}
	}

	outColor = vec4(tempColor / float(sample_num), 0.0);
	*/
}
