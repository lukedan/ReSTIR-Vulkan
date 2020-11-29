#define LIGHT_SAMPLE_GROUP_SIZE_X 8
#define LIGHT_SAMPLE_GROUP_SIZE_Y 8

#define SW_VISIBILITY_TEST_GROUP_SIZE_X 8
#define SW_VISIBILITY_TEST_GROUP_SIZE_Y 8


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
	vec4 cameraPos;
	uvec2 screenSize;
	uint frame;
};
