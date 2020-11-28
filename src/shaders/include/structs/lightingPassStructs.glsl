struct LightingPassUniforms{
	mat4 inverseViewMatrix;
	vec4 tempLightPoint;
	vec4 cameraPos;
	float cameraNear;
	float cameraFar;
	float tanHalfFovY;
	float aspectRatio;
	int debugMode;
	int sampleNum;
	int sysTime;
};
