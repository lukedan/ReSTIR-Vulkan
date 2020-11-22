// these structs are not supposed to be seen by the cpu

struct LightSample {
	vec3 emission;
	float pHat;
	vec3 position;
	float sumWeights; // setting W to 0 is equivalent to setting sumWeights to 0
};

struct Reservoir {
	LightSample samples[RESERVOIR_SIZE];
	int numStreamSamples;
};


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
