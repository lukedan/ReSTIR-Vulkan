#ifdef HARDWARE_RAY_TRACING
#   extension GL_EXT_ray_tracing : enable
#endif

#include "include/structs/lightingPassStructs.glsl"
#include "include/reservoir.glsl"
#include "include/restirUtils.glsl"

layout (set = 0, binding = 0) uniform sampler2D uniWorldPosition;
layout (set = 0, binding = 1) uniform sampler2D uniAlbedo;
layout (set = 0, binding = 2) uniform sampler2D uniNormal;
layout (set = 0, binding = 3) uniform sampler2D uniMaterialProperties;
layout (set = 0, binding = 4) uniform sampler2D uniDepth;

layout (set = 0, binding = 5) buffer Reservoirs {
    Reservoir reservoirs[];
};
layout (set = 0, binding = 6) buffer ResultReservoirs {
	Reservoir resultReservoirs[];
};

layout (set = 0, binding = 7) uniform Restiruniforms {
	RestirUniforms uniforms;
};

#ifdef HARDWARE_RAY_TRACING
layout (location = 0) rayPayloadEXT bool isShadowed;
layout (set = 1, binding = 0) uniform accelerationStructureEXT acc;
#else
#	include "include/structs/aabbTree.glsl"
layout (set = 1, binding = 0) buffer AabbTree {
	AabbTreeNode nodes[];
} aabbTree;
layout (set = 1, binding = 1) buffer Triangles {
	Triangle triangles[];
};

layout (local_size_x = UNBIASED_REUSE_GROUP_SIZE_X, local_size_y = UNBIASED_REUSE_GROUP_SIZE_Y, local_size_z = 1) in;

#	define NODE_BUFFER aabbTree
#	define TRIANGLE_BUFFER triangles
#	include "include/softwareRaytracing.glsl"
#endif

#include "include/visibilityTest.glsl"


#define NUM_NEIGHBORS 3

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

	vec3 albedo = texelFetch(uniAlbedo, ivec2(pixelCoord), 0).xyz;
	vec3 normal = texelFetch(uniNormal, ivec2(pixelCoord), 0).xyz;
	vec2 roughnessMetallic = texelFetch(uniMaterialProperties, ivec2(pixelCoord), 0).xy;
    vec3 worldPos = texelFetch(uniWorldPosition, ivec2(pixelCoord), 0).xyz;
    float worldDepth = texelFetch(uniDepth, ivec2(pixelCoord), 0).x;
    
    float albedoLum = luminance(albedo.r, albedo.g, albedo.b);

	uint reservoirIndex = pixelCoord.y * uniforms.screenSize.x + pixelCoord.x;
    Reservoir res = reservoirs[reservoirIndex];

    Rand rand = seedRand(uniforms.frame * 17, pixelCoord.y * 10007 + pixelCoord.x);
	ivec2 neighborPositions[NUM_NEIGHBORS];
	uint neighborNumSamples[NUM_NEIGHBORS];
	uint originalNumSamples = res.numStreamSamples;
    for (int i = 0; i < NUM_NEIGHBORS; ++i) {
        float angle = randFloat(rand) * 2.0 * M_PI;
        float radius = sqrt(randFloat(rand)) * uniforms.spatialRadius;

        ivec2 randNeighbor = ivec2(round(vec2(cos(angle), sin(angle)) * radius));
        randNeighbor = clamp(ivec2(pixelCoord) + randNeighbor, ivec2(0), ivec2(uniforms.screenSize - 1));

        uint neighborIndex = randNeighbor.y * uniforms.screenSize.x + randNeighbor.x;

		Reservoir randRes = reservoirs[neighborIndex];

		neighborPositions[i] = randNeighbor;
		neighborNumSamples[i] = randRes.numStreamSamples;

		res.numStreamSamples += randRes.numStreamSamples;
		for (int j = 0; j < RESERVOIR_SIZE; ++j) {
			float newPHat = evaluatePHat(
				worldPos, randRes.samples[j].position_emissionLum.xyz, uniforms.cameraPos.xyz,
				normal, randRes.samples[j].normal.xyz, randRes.samples[j].normal.w > 0.5f,
				albedoLum, randRes.samples[j].position_emissionLum.w, roughnessMetallic.x, roughnessMetallic.y);
			float weight = newPHat * randRes.samples[j].w * randRes.numStreamSamples;
			if (weight > 0.0f) {
				updateReservoirAt(
					res, j, weight,
					randRes.samples[j].position_emissionLum.xyz, randRes.samples[j].normal,
					randRes.samples[j].position_emissionLum.w,
					randRes.samples[j].lightIndex, newPHat, randRes.samples[j].w, rand
				);
			}
        }
    }

	vec3 neighborWorldPos[NUM_NEIGHBORS];
	vec3 neighborNormal[NUM_NEIGHBORS];
	for (int i = 0; i < NUM_NEIGHBORS; ++i) {
		neighborWorldPos[i] = texelFetch(uniWorldPosition, neighborPositions[i], 0).xyz;
		neighborNormal[i] = texelFetch(uniNormal, neighborPositions[i], 0).xyz;
	}
	for (int i = 0; i < RESERVOIR_SIZE; ++i) {
		vec3 lightPos = res.samples[i].position_emissionLum.xyz;
		uint numSamples = originalNumSamples;
		for (int j = 0; j < NUM_NEIGHBORS; ++j) {
			if (dot(lightPos - neighborWorldPos[j], neighborNormal[j]) < 0.0f) {
				continue;
			}

			bool shadowed = testVisibility(neighborWorldPos[j], lightPos);
			if (shadowed) {
				continue;
			}

			numSamples += neighborNumSamples[j];
		}
		bool shadowed = testVisibility(worldPos, lightPos);
		if (shadowed) {
			numSamples = 0;
		}

		if (numSamples > 0) {
			res.samples[i].w = res.samples[i].sumWeights / (numSamples * res.samples[i].pHat);
		} else {
			res.samples[i].w = 0.0f;
			res.samples[i].sumWeights = 0.0f;
		}
	}

    resultReservoirs[reservoirIndex] = res;
}
