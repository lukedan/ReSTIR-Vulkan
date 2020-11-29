// these structs are not supposed to be seen by the cpu

#include "rand.glsl"
#include "structs/restirStructs.glsl"

void updateReservoir(inout Reservoir res, float weight, vec3 position, vec4 pHat, vec4 lightColor, inout Rand rand) {
	for (int i = 0; i < RESERVOIR_SIZE; ++i) {
		res.samples[i].sumWeights += weight;
		float replacePossibility = weight / res.samples[i].sumWeights;
		if (randFloat(rand) < replacePossibility) {
			res.samples[i].position.xyz = position;
			res.samples[i].pHat = pHat;
			res.samples[i].emission = lightColor;
		}
	}
}

void addSampleToReservoir(inout Reservoir res, vec3 position, vec4 pHat, float sampleP, vec4 lightColor, inout Rand rand) {
	float weight = pHat.w / sampleP;
	++res.numStreamSamples;

	updateReservoir(res, weight, position, pHat, lightColor, rand);
}

void combineReservoirs(inout Reservoir self, Reservoir other, vec4 pHat[RESERVOIR_SIZE], inout Rand rand) {
	self.numStreamSamples += other.numStreamSamples * RESERVOIR_SIZE;

	for (int i = 0; i < RESERVOIR_SIZE; ++i) {
		float weight = (pHat[i].w / other.samples[i].pHat.w) * other.samples[i].sumWeights;
		updateReservoir(self, weight, other.samples[i].position.xyz, pHat[i], other.samples[i].emission, rand);
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
