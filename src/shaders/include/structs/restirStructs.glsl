#define LIGHT_SAMPLE_GROUP_SIZE_X 64
#define LIGHT_SAMPLE_GROUP_SIZE_Y 1

#define SW_VISIBILITY_TEST_GROUP_SIZE_X 64
#define SW_VISIBILITY_TEST_GROUP_SIZE_Y 1

#define TEMPORAL_REUSE_GROUP_SIZE_X 64
#define TEMPORAL_REUSE_GROUP_SIZE_Y 1


#define RESERVOIR_SIZE 4

struct LightSample {
	vec4 pHat;
	vec4 position;
	float sumWeights; // setting W to 0 is equivalent to setting sumWeights to 0
	float w;
	vec4 emission;
};

struct Reservoir {
	LightSample samples[RESERVOIR_SIZE];
	uint numStreamSamples;
};


struct RestirUniforms {
	mat4 prevFrameProjectionViewMatrix;
	vec4 cameraPos;
	uvec2 screenSize;
	uint frame;
	float posThreshold;
	float norThreshold;
};
