// these structs are not supposed to be seen by the cpu

#include "rand.glsl"
#include "structs/restirStructs.glsl"

void updateReservoirAt(inout Reservoir res, int i, float weight, vec3 position, vec4 pHat, inout Rand rand) {
	res.samples[i].sumWeights += weight;
	float replacePossibility = weight / res.samples[i].sumWeights;
	if (randFloat(rand) < replacePossibility) {
		res.samples[i].position.xyz = position;
		res.samples[i].pHat = pHat;
	}
}

void addSampleToReservoir(inout Reservoir res, vec3 position, vec4 pHat, float sampleP, vec4 lightColor, inout Rand rand) {
	float weight = pHat.w / sampleP;
	res.numStreamSamples += 1;

	for (int i = 0; i < RESERVOIR_SIZE; ++i) {
		updateReservoirAt(res, i, weight, position, pHat, rand);
	}
}

void combineReservoirs(inout Reservoir self, Reservoir other, vec4 pHat[RESERVOIR_SIZE], inout Rand rand) {
	self.numStreamSamples += other.numStreamSamples;

	for (int i = 0; i < RESERVOIR_SIZE; ++i) {
		if (other.samples[i].sumWeights != 0.0f) {
			float weight = (pHat[i].w / other.samples[i].pHat.w) * other.samples[i].sumWeights;
			updateReservoirAt(self, i, weight, other.samples[i].position.xyz, pHat[i], rand);
		}
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
