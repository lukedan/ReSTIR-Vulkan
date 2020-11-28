#version 450
#extension GL_ARB_separate_shader_objects: enable
#extension GL_ARB_shader_clock : enable


#include "include/structs/aabbTree.glsl"
#include "include/structs/lightingPassStructs.glsl"
#include "include/gBufferDebugConstants.glsl"
#include "include/disneyBRDF.glsl"
#include "include/rand.glsl"
#include "include/structs/light.glsl"

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

layout (binding = 7) buffer PtLights {
	int lightsNum;
	pointLight lights[];
} ptLights;

/* Wait for pipeline layout */
layout (binding = 8) buffer TriLights {
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
	vec4 albedo = texture(uniAlbedo, inUv);
	vec3 normal = texture(uniNormal, inUv).xyz;
	float depth = texture(uniDepth, inUv).x;
	vec2 materialProps = texture(uniMaterialProperties, inUv).xy;

	float worldDepth = sceneDepthToWorldDepth(depth, uniforms.cameraNear, uniforms.cameraFar);
	vec3 viewPos = fragCoordDepthToViewPos(inUv, worldDepth, uniforms.tanHalfFovY, uniforms.aspectRatio);
	vec3 worldPos = (uniforms.inverseViewMatrix * vec4(viewPos, 1.0f)).xyz;

	/* No Monte-Carlo rendering 
	if (uniforms.debugMode == GBUFFER_DEBUG_NONE) {
		outColor = vec4(albedo.rgb, 1.0f); // TODO
	} else if (uniforms.debugMode == GBUFFER_DEBUG_ALBEDO) {
		outColor = vec4(albedo.rgb, 1.0f);
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
	if(!visible){
		outColor.xyz *= 0.5f;
	}
	*/

	vec3 tempColor = vec3(0.0, 0.0, 0.0);
	int sample_num = uniforms.sampleNum;

	/* Monte-Carlo rendering -- point lights */
	int ptLightNum = ptLights.lightsNum;
	if(ptLightNum != 0){
		for(int i = 0; i < sample_num; ++i){
			// Lights parameters and init
			uint seed = uint(uniforms.sysTime + int(worldPos.x * 12.0) + int(worldPos.y * 133.0 * inUv.x * inUv.y) + int(worldPos.z * 7.0 * inUv.y));
			float tempFloatRnd = rnd(seed);
			int selectedIdx = int(tempFloatRnd * ptLightNum * 100.0) % ptLightNum;
	
			vec3 tempLightPos = ptLights.lights[selectedIdx].pos.xyz;
			vec3 tempLightIntensity = ptLights.lights[selectedIdx].color.rgb * ptLights.lights[selectedIdx].intensity;

			vec3 rayDir = tempLightPos - worldPos;
			bool visible = raytrace(worldPos + 0.01 * rayDir, rayDir * 0.98);
		
			if (visible) {
				// Direct light shading
				vec3 f = albedo.rgb / PI;
				float pdf = length(tempLightPos - worldPos);
				pdf = pdf * pdf;
				tempColor += f * tempLightIntensity * abs(dot(normalize(rayDir), normal)) / (pdf * ptLightNum);
			}
		}
	}else{
		/* Monte-Carlo rendering -- triangle lights */
		if(albedo.w == 1.0){
			tempColor = albedo.xyz * sample_num;
		}else{
			int lightNum = triLights.lightsNum;
			if(lightNum != 0){
				for(int i = 0; i < sample_num; ++i){
					uint seed = floatBitsToUint(uniforms.sysTime / i + inUv.x * 1333 + inUv.y * 1213 + worldPos.x * worldPos.y * worldPos.z);
					Rand randomNum;
					randomNum.context = seed;
					float tempFloatRnd = randFloat(randomNum);
					int selectedIdx = int(tempFloatRnd * lightNum * 100.0) % lightNum;
					triLight pickedLight = triLights.lights[selectedIdx];

					// Pick a triangle light
					uint seed1 = floatBitsToUint(uniforms.sysTime + i * 123 + worldPos.x * 10001399.0 + worldPos.y * 33.0 + worldPos.z * 17.0);
					uint seed2 = floatBitsToUint(uniforms.sysTime + i * 10001227 + worldPos.x * 7.0 + worldPos.y * 3.0 + worldPos.z * 121.0);
					Rand randomNum2;
					randomNum2.context = seed1;
					Rand randomNum3;
					randomNum3.context = seed2;
					vec3 pickedLightPos = pickPointOnTriangle(randFloat(randomNum2), randFloat(randomNum3), pickedLight.p1.xyz, pickedLight.p2.xyz, pickedLight.p3.xyz);
					vec3 pickedLightIntensity = pickedLight.emissiveFactor.xyz * 10.0;

					vec3 rayDir = pickedLightPos - worldPos;
					bool visible = raytrace(worldPos + 0.01 * rayDir, rayDir * 0.98);
	
					if (visible) {
						// Direct light shading
						vec3 f = albedo.xyz / PI;
						float pdf = triLightPDF(pickedLight, normalize(rayDir), pickedLightPos, worldPos);
						tempColor += f * pickedLightIntensity * abs(dot(normalize(rayDir), normal)) / (pdf * lightNum);
					}
				}
			}
		}
	}
	outColor = vec4(tempColor / float(sample_num), 0.0);
}
