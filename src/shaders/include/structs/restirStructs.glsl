#define LIGHT_SAMPLE_GROUP_SIZE_X 64
#define LIGHT_SAMPLE_GROUP_SIZE_Y 1

#define SW_VISIBILITY_TEST_GROUP_SIZE_X 64
#define SW_VISIBILITY_TEST_GROUP_SIZE_Y 1

#define TEMPORAL_REUSE_GROUP_SIZE_X 64
#define TEMPORAL_REUSE_GROUP_SIZE_Y 1

#define OMNI_GROUP_SIZE_X 64
#define OMNI_GROUP_SIZE_Y 1

#define UNBIASED_REUSE_GROUP_SIZE_X 64
#define UNBIASED_REUSE_GROUP_SIZE_Y 1

/*#define UNBIASED_MIS*/
#define RESERVOIR_SIZE 1

struct LightSample {
	vec4 position_emissionLum;
	vec4 normal;
	int lightIndex; // negative for triangle lights
	float pHat;
	float sumWeights;
	float w;
#ifdef UNBIASED_MIS
	float sumPHat;
#endif
};

struct Reservoir {
	LightSample samples[RESERVOIR_SIZE];
	uint numStreamSamples;
};


#define RESTIR_VISIBILITY_REUSE_FLAG (1 << 0)
#define RESTIR_TEMPORAL_REUSE_FLAG (1 << 1)

struct RestirUniforms {
	mat4 prevFrameProjectionViewMatrix;
	vec4 cameraPos;
	uvec2 screenSize;
	uint frame;

	uint initialLightSampleCount;

	uint temporalSampleCountMultiplier;

	float spatialPosThreshold;
	float spatialNormalThreshold;
	uint spatialNeighbors;
	float spatialRadius;

	int flags;
};
