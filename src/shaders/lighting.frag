#version 450

#include "include/structs/aabbTree.glsl"
#include "include/structs/lightingPassStructs.glsl"
#include "include/gBufferDebugConstants.glsl"
#include "include/rand.glsl"
#include "include/structs/light.glsl"
#include "include/structs/restirStructs.glsl"
#include "include/restirUtils.glsl"

layout (binding = 0) uniform sampler2D uniAlbedo;
layout (binding = 1) uniform sampler2D uniNormal;
layout (binding = 2) uniform sampler2D uniMaterialProperties;
layout (binding = 3) uniform sampler2D uniWorldPosition;

layout (binding = 4) uniform Uniforms {
	LightingPassUniforms uniforms;
};
layout (binding = 5) buffer Reservoirs {
	Reservoir reservoirs[];
};
layout (binding = 6) buffer PointLights {
	int count;
	pointLight lights[];
} pointLights;
layout (binding = 7) buffer TriangleLights {
	int count;
	triLight lights[];
} triangleLights;

layout (location = 0) in vec2 inUv;

layout (location = 0) out vec3 outColor;

#define PI 3.1415926

#include "include/frustumUtils.glsl"

float rnd(uint seed) {
	return 0.0f;
}

void main() {
	vec4 albedo = texture(uniAlbedo, inUv);
	vec3 normal = texture(uniNormal, inUv).xyz;
	vec2 materialProps = texture(uniMaterialProperties, inUv).xy;
	vec3 worldPos = texture(uniWorldPosition, inUv).xyz;

	if (uniforms.debugMode == GBUFFER_DEBUG_NONE) {
		uvec2 pixelCoord = uvec2(gl_FragCoord.xy);
		Reservoir reservoir = reservoirs[pixelCoord.y * uniforms.bufferSize.x + pixelCoord.x];
		outColor = vec3(0.0f);
		for (int i = 0; i < RESERVOIR_SIZE; ++i) {
			vec3 emission;
			int lightIndex = reservoir.samples[i].lightIndex;
			if (lightIndex < 0) {
				emission = triangleLights.lights[-1 - lightIndex].emission_luminance.rgb;
			} else {
				emission = pointLights.lights[lightIndex].color_luminance.rgb;
			}
			vec3 pHat = evaluatePHatFull(
				worldPos, reservoir.samples[i].position_emissionLum.xyz, uniforms.cameraPos.xyz,
				normal, reservoir.samples[i].normal.xyz, reservoir.samples[i].normal.w > 0.5f,
				albedo.rgb, emission, materialProps.x, materialProps.y
			);
			outColor += pHat * reservoir.samples[i].w;
		}
		outColor /= RESERVOIR_SIZE;
		if (albedo.w > 0.5f) {
			outColor = albedo.xyz;
		}
	} else if (uniforms.debugMode == GBUFFER_DEBUG_ALBEDO) {
		if (albedo.a < 0.5f) {
			outColor = albedo.rgb;
		} else {
			outColor = vec3(0.0f);
		}
	} else if (uniforms.debugMode == GBUFFER_DEBUG_EMISSION) {
		if (albedo.a > 0.5f) {
			outColor = albedo.rgb;
		} else {
			outColor = vec3(0.0f);
		}
	} else if (uniforms.debugMode == GBUFFER_DEBUG_NORMAL) {
		outColor = (vec3(normal) + 1.0f) * 0.5f;
	} else if (uniforms.debugMode == GBUFFER_DEBUG_MATERIAL_PROPERTIES) {
		outColor = vec3(materialProps, 1.0f);
	} else if (uniforms.debugMode == GBUFFER_DEBUG_WORLD_POSITION) {
		outColor = worldPos / 10.0f + 0.5f;
	} else if (uniforms.debugMode == GBUFFER_DEBUG_NAIVE_POINT_LIGHT_NO_SHADOW) {
		float roughness = materialProps.r;
		float metallic = materialProps.g;

		outColor = vec3(0.0f);
		for (int i = 0; i < pointLights.count; ++i) {
			outColor += evaluatePHatFull(
				worldPos, pointLights.lights[i].pos.xyz, uniforms.cameraPos.xyz, normal, vec3(0.0f), false,
				albedo.rgb, pointLights.lights[i].color_luminance.rgb, roughness, metallic
			);
		}
	}

	outColor = pow(outColor, vec3(1.0f / uniforms.gamma));
}
