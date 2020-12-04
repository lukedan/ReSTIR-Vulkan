// these structs are not supposed to be seen by the cpu

#include "rand.glsl"
#include "structs/restirStructs.glsl"

void updateReservoirAt(
	inout Reservoir res, int i, float weight, vec3 position, float emissionLum, int lightIdx, float pHat, inout Rand rand
) {
	res.samples[i].sumWeights += weight;
	float replacePossibility = weight / res.samples[i].sumWeights;
	if (randFloat(rand) < replacePossibility) {
		res.samples[i].position_emissionLum = vec4(position, emissionLum);
		res.samples[i].lightIndex = lightIdx;
		res.samples[i].pHat = pHat;
	}
}

void addSampleToReservoir(inout Reservoir res, vec3 position, float emissionLum, int lightIdx, float pHat, float sampleP, inout Rand rand) {
	float weight = pHat / sampleP;
	res.numStreamSamples += 1;

	for (int i = 0; i < RESERVOIR_SIZE; ++i) {
		updateReservoirAt(res, i, weight, position, emissionLum, lightIdx, pHat, rand);
		res.samples[i].w = res.samples[i].sumWeights / (res.numStreamSamples * res.samples[i].pHat);
	}
}

void combineReservoirs(inout Reservoir self, Reservoir other, float pHat[RESERVOIR_SIZE], inout Rand rand) {
	self.numStreamSamples += other.numStreamSamples;

	for (int i = 0; i < RESERVOIR_SIZE; ++i) {
		float weight = pHat[i] * other.samples[i].w * other.numStreamSamples;
		if (weight > 0.0f) {
			updateReservoirAt(
				self, i, weight,
				other.samples[i].position_emissionLum.xyz, other.samples[i].position_emissionLum.w,
				other.samples[i].lightIndex, pHat[i], rand
			);
		}
		self.samples[i].w = self.samples[i].sumWeights / (self.numStreamSamples * self.samples[i].pHat);
	}
}

Reservoir newReservoir() {
	Reservoir result;
	for (int i = 0; i < RESERVOIR_SIZE; ++i) {
		result.samples[i].sumWeights = 0.0f;
	}
	result.numStreamSamples = 0;
	return result;
}
