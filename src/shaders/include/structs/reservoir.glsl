#include "sceneStructs.glsl"
#include "../rand.glsl"

void updateReservoir(inout Reservoir res, float weight, vec3 emission, vec3 position, float pHat, inout Rand rand) {
	for (int i = 0; i < RESERVOIR_SIZE; ++i) {
		res.samples[i].sumWeights += weight;
		float replacePossibility = weight / res.samples[i].sumWeights;
		if (randFloat(rand) < replacePossibility) {
			res.samples[i].emission = emission;
			res.samples[i].position = position;
			res.samples[i].pHat = pHat;
		}
	}
} 

void addSampleToReservoir(inout Reservoir res, vec3 emission, vec3 position, float pHat, float sampleP, inout Rand rand) {
	float weight = pHat / sampleP;
	++res.numStreamSamples;

	updateReservoir(res, weight, emission, position, pHat, rand);
}

void combineReservoirs(inout Reservoir self, Reservoir other, float pHat[RESERVOIR_SIZE], inout Rand rand) {
	self.numStreamSamples += other.numStreamSamples * RESERVOIR_SIZE;

	for (int i = 0; i < RESERVOIR_SIZE; ++i) {
		float weight = (pHat[i] / other.samples[i].pHat) * other.samples[i].sumWeights;
		updateReservoir(self, weight, other.samples[i].emission, other.samples[i].position, pHat[i], rand);
	}
}
