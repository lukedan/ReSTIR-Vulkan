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

layout (binding = 6) buffer PtLights {
	int lightsNum;
	pointLight lights[];
} ptLights;

/* Wait for pipeline layout */
layout (binding = 7) buffer TriLights {
	int lightsNum;
	triLight lights[];
} triLights;


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

	/* No Monte-Carlo rendering 
	if (uniforms.debugMode == GBUFFER_DEBUG_NONE) {
		outColor = vec4(albedo, 1.0f); // TODO
	} else if (uniforms.debugMode == GBUFFER_DEBUG_ALBEDO) {
		outColor = vec4(albedo, 1.0f);
	} else if (uniforms.debugMode == GBUFFER_DEBUG_NORMAL) {
		outColor = vec4((vec3(normal) + 1.0f) * 0.5f, 1.0f);
	}

	vec3 rayDir = uniforms.tempLightPoint.xyz - worldPos;
	bool visible = raytrace(worldPos + 0.01 * rayDir, rayDir * 0.98);
	if(!visible){
		outColor.xyz *= 0.5f;
	}
	*/

	vec3 tempColor = vec3(0.0, 0.0, 0.0);
	int sample_num = uniforms.sampleNum;

	/* Monte-Carlo rendering -- point lights 
	int lightNum = ptLights.lightsNum;
	if(lightNum != 0){
		for(int i = 0; i < sample_num; ++i){
			// Lights parameters and init
			uint seed = uint(int(clockARB()) + int(worldPos.x * 12.0) + int(worldPos.y * 133.0) + int(worldPos.z * 7.0));
			float tempFloatRnd = rnd(seed);
			int selectedIdx = int(tempFloatRnd * lightNum * 100.0) % lightNum;
	
			vec3 tempLightPos = ptLights.lights[selectedIdx].pos;
			vec3 tempLightIntensity = ptLights.lights[selectedIdx].color * ptLights.lights[selectedIdx].intensity;

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
	}
	*/

	/* Monte-Carlo rendering -- triangle lights */
	int lightNum = triLights.lightsNum;
	if(lightNum != 0){
		for(int i = 0; i < sample_num; ++i){
			uint seed = uint(int(clockARB()) + int(worldPos.x * 12.0) + int(worldPos.y * 133.0) + int(worldPos.z * 7.0));
			float tempFloatRnd = rnd(seed);
			int selectedIdx = int(tempFloatRnd * lightNum * 100.0) % lightNum;

			vec3 tempLightPos = (triLights.lights[selectedIdx].p1 + triLights.lights[selectedIdx].p2 + triLights.lights[selectedIdx].p3) / 3.0;
			vec3 tempLightIntensity = triLights.lights[selectedIdx].emissiveFactor * 10.0;

			vec3 rayDir = tempLightPos - worldPos;
			bool visible = raytrace(worldPos + 0.01 * rayDir, rayDir * 0.98);

			if (visible) {
				// Direct light shading
				vec3 f = vec3(1.0, 1.0, 1.0) / PI;
				float pdf = length(tempLightPos - worldPos);
				pdf = pdf * pdf;
				tempColor += f * tempLightIntensity * abs(dot(normalize(rayDir), normal)) / (pdf * lightNum);
			}
		}
		
	}

	outColor = vec4(tempColor / float(sample_num), 0.0);
	

	/* Debug
	// vec3 tempLightPos = (triLights.lights[8].p1 + triLights.lights[8].p2 + triLights.lights[8].p3) / 3.0;
	vec3 tempLightIntensity = triLights.lights[12].emissiveFactor;
	vec3 tempLightPos = vec3(0.0, 0.0, 0.0);
	// vec3 tempLightIntensity = vec3(10.0, 10.0, 10.0);

	vec3 rayDir = tempLightPos - worldPos;
	bool visible = raytrace(worldPos + 0.01 * rayDir, rayDir * 0.98);

	if (visible) {
		// Direct light shading
		vec3 f = vec3(1.0, 1.0, 1.0) / PI;
		float pdf = length(tempLightPos - worldPos);
		pdf = pdf * pdf;
		// tempColor += f * tempLightIntensity * abs(dot(normalize(rayDir), normal)) / (pdf * lightNum);
		tempColor += tempLightIntensity;
	}

	outColor = vec4(tempColor, 0.0);
	*/

	/* Debug 
	float checkNum = float(triLights.lightsNum) / 12.0;
	outColor.xyz = vec3(checkNum, checkNum, checkNum);
	*/
}
