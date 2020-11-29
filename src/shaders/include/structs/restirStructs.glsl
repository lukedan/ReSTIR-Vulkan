#define LIGHT_SAMPLE_GROUP_SIZE_X 64
#define LIGHT_SAMPLE_GROUP_SIZE_Y 1

#define SW_VISIBILITY_TEST_GROUP_SIZE_X 64
#define SW_VISIBILITY_TEST_GROUP_SIZE_Y 1

#define TEMPORAL_REUSE_GROUP_SIZE_X 64
#define TEMPORAL_REUSE_GROUP_SIZE_Y 1


#define RESERVOIR_SIZE 4

struct LightSample {
	vec4 pHat; // first three components are rgb, last component is luminance
	vec4 position;
	float sumWeights; // setting W to 0 is equivalent to setting sumWeights to 0
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
};
