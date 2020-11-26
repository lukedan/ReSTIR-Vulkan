struct LightingPassUniforms{
	mat4 inverseViewMatrix;
	vec4 tempLightPoint;
	float cameraNear;
	float cameraFar;
	float tanHalfFovY;
	float aspectRatio;
	int debugMode;
	int lightNum;
	int sample_num;
};
