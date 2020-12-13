#ifdef HARDWARE_RAY_TRACING
#	extension GL_EXT_ray_tracing : enable
#endif

#include "include/structs/lightingPassStructs.glsl"
#include "include/reservoir.glsl"
#include "include/restirUtils.glsl"
#include "include/structs/light.glsl"


layout (binding = 0, set = 0) buffer PointLights {
	int count;
	pointLight lights[];
} pointLights;
layout (binding = 1, set = 0) buffer TriangleLights {
	int count;
	triLight lights[];
} triangleLights;
layout (binding = 2, set = 0) buffer AliasTable{
	int count;
	int padding[3];
	aliasTableColumn aliasCol[];
} aliasTable;

layout (binding = 3, set = 0) uniform Restiruniforms {
	RestirUniforms uniforms;
};

layout (binding = 0, set = 1) uniform sampler2D uniWorldPosition;
layout (binding = 1, set = 1) uniform sampler2D uniAlbedo;
layout (binding = 2, set = 1) uniform sampler2D uniNormal;
layout (binding = 3, set = 1) uniform sampler2D uniMaterialProperties;

layout (binding = 4, set = 1) uniform sampler2D uniPrevFrameWorldPosition;
layout (binding = 5, set = 1) uniform sampler2D uniPrevFrameAlbedo;
layout (binding = 6, set = 1) uniform sampler2D uniPrevFrameNormal;
layout (binding = 7, set = 1) uniform sampler2D uniPrevDepth;

layout (binding = 8, set = 1) buffer Reservoirs {
	Reservoir reservoirs[];
};
layout (binding = 9, set = 1) buffer PrevFrameReservoirs {
	Reservoir prevFrameReservoirs[];
};

#ifdef HARDWARE_RAY_TRACING
layout (location = 0) rayPayloadEXT bool isShadowed;
layout (binding = 0, set = 2) uniform accelerationStructureEXT acc;
#else
#	include "include/structs/aabbTree.glsl"
layout (binding = 0, set = 2) buffer AabbTree {
	AabbTreeNode nodes[];
} aabbTree;
layout (binding = 1, set = 2) buffer Triangles {
	Triangle triangles[];
};

layout (local_size_x = OMNI_GROUP_SIZE_X, local_size_y = OMNI_GROUP_SIZE_Y, local_size_z = 1) in;

#	define NODE_BUFFER aabbTree
#	define TRIANGLE_BUFFER triangles
#	include "include/softwareRaytracing.glsl"
#endif

#include "include/visibilityTest.glsl"


vec3 pickPointOnTriangle(float r1, float r2, vec3 p1, vec3 p2, vec3 p3) {
	float sqrt_r1 = sqrt(r1);
	return (1.0 - sqrt_r1) * p1 + (sqrt_r1 * (1.0 - r2)) * p2 + (r2 * sqrt_r1) * p3;
}

void aliasTableSample(float r1, float r2, out int index, out float probability) {
	int selected_column = min(int(aliasTable.count * r1), aliasTable.count - 1);
	aliasTableColumn col = aliasTable.aliasCol[selected_column];
	if (col.prob > r2) {
		index = selected_column;
		probability = col.oriProb;
	} else {
		index = col.alias;
		probability = col.aliasOriProb;
	}
}


void main() {
	uvec2 pixelCoord =
#ifdef HARDWARE_RAY_TRACING
		gl_LaunchIDEXT.xy;
#else
		gl_GlobalInvocationID.xy;
#endif
	if (any(greaterThanEqual(pixelCoord, uniforms.screenSize))) {
		return;
	}

	// Light Sampling
	vec3 albedo = texelFetch(uniAlbedo, ivec2(pixelCoord), 0).xyz;
	vec3 normal = texelFetch(uniNormal, ivec2(pixelCoord), 0).xyz;
	vec2 roughnessMetallic = texelFetch(uniMaterialProperties, ivec2(pixelCoord), 0).xy;
	vec3 worldPos = texelFetch(uniWorldPosition, ivec2(pixelCoord), 0).xyz;

	float albedoLum = luminance(albedo.r, albedo.g, albedo.b);

	Reservoir res = newReservoir();
	Rand rand = seedRand(uniforms.frame, pixelCoord.y * 10007 + pixelCoord.x);
	for (int i = 0; i < uniforms.initialLightSampleCount; ++i) {			
		int selected_idx;
		float lightSampleProb;
		aliasTableSample(randFloat(rand), randFloat(rand), selected_idx, lightSampleProb);

		vec3 lightSamplePos;
		vec4 lightNormal;
		float lightSampleLum;
		int lightSampleIndex;
		if (pointLights.count != 0) {
			pointLight light = pointLights.lights[selected_idx];
			lightSamplePos = light.pos.xyz;
			lightSampleLum = light.color_luminance.w;
			lightSampleIndex = selected_idx;
			lightNormal = vec4(0.0f);
		} else {
			triLight light = triangleLights.lights[selected_idx];
			lightSamplePos = pickPointOnTriangle(randFloat(rand), randFloat(rand), light.p1.xyz, light.p2.xyz, light.p3.xyz);
			lightSampleLum = light.emission_luminance.w;
			lightSampleIndex = -1 - selected_idx;

			vec3 wi = normalize(worldPos - lightSamplePos);
			vec3 normal = light.normalArea.xyz;
			lightSampleProb /= abs(dot(wi, normal)) * light.normalArea.w;
			lightNormal = vec4(normal, 1.0f);
		}
		
		float pHat = evaluatePHat(
			worldPos, lightSamplePos, uniforms.cameraPos.xyz,
			normal, lightNormal.xyz, lightNormal.w > 0.5f,
			albedoLum, lightSampleLum, roughnessMetallic.x, roughnessMetallic.y
		);

		addSampleToReservoir(res, lightSamplePos, lightNormal, lightSampleLum, lightSampleIndex, pHat, lightSampleProb, rand);
	}
	
	uint reservoirIndex = pixelCoord.y * uniforms.screenSize.x + pixelCoord.x;
	
	// Visibility Reuse
	if ((uniforms.flags & RESTIR_VISIBILITY_REUSE_FLAG) != 0) {
		for (int i = 0; i < RESERVOIR_SIZE; i++) {
			bool shadowed = testVisibility(worldPos, res.samples[i].position_emissionLum.xyz);

			if (shadowed) {
				res.samples[i].w = 0.0f;
				res.samples[i].sumWeights = 0.0f;
			}
		}
	}

	// Temporal reuse
	if ((uniforms.flags & RESTIR_TEMPORAL_REUSE_FLAG) != 0) {
		vec4 prevFramePos = uniforms.prevFrameProjectionViewMatrix * vec4(worldPos, 1.0f);
		prevFramePos.xyz /= prevFramePos.w;
		prevFramePos.xy = (prevFramePos.xy + 1.0f) * 0.5f * vec2(uniforms.screenSize);
		if (
			all(greaterThan(prevFramePos.xy, vec2(0.0f))) &&
			all(lessThan(prevFramePos.xy, vec2(uniforms.screenSize)))
		) {
			ivec2 prevFrag = ivec2(prevFramePos.xy);

#ifdef COMPARE_DEPTH
			float depthDiff = prevFramePos.z - texelFetch(uniPrevDepth, prevFrag, 0).x;
			if (depthDiff < 0.001f * prevFramePos.z) {
#else
			// highest quality results can be obtained by directly comparing the world positions
			// the performance impact of this is unclear
			vec3 positionDiff = worldPos - texelFetch(uniPrevFrameWorldPosition, prevFrag, 0).xyz;
			if (dot(positionDiff, positionDiff) < 0.01f) {
#endif
				vec3 albedoDiff = albedo - texelFetch(uniPrevFrameAlbedo, prevFrag, 0).rgb;
				if (dot(albedoDiff, albedoDiff) < 0.01f) {
					float normalDot = dot(normal, texelFetch(uniPrevFrameNormal, prevFrag, 0).xyz);
					if (normalDot > 0.5f) {
						Reservoir prevRes = prevFrameReservoirs[prevFrag.y * uniforms.screenSize.x + prevFrag.x];

						// clamp the number of samples
						prevRes.numStreamSamples = min(
							prevRes.numStreamSamples, uniforms.temporalSampleCountMultiplier * res.numStreamSamples
						);

						vec2 metallicRoughness = texelFetch(uniMaterialProperties, ivec2(pixelCoord), 0).xy;

						float pHat[RESERVOIR_SIZE];
						for (int i = 0; i < RESERVOIR_SIZE; ++i) {
							pHat[i] = evaluatePHat(
								worldPos, prevRes.samples[i].position_emissionLum.xyz, uniforms.cameraPos.xyz,
								normal, prevRes.samples[i].normal.xyz, prevRes.samples[i].normal.w > 0.5f,
								albedoLum, prevRes.samples[i].position_emissionLum.w, metallicRoughness.x, metallicRoughness.y
							);
						}

						combineReservoirs(res, prevRes, pHat, rand);
					}
				}
			}
		}
	}

	reservoirs[reservoirIndex] = res;
}
