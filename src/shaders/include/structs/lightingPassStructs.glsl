struct LightingPassUniforms{
	mat4 prevFrameProjectionViewMatrix;
	vec4 cameraPos;
	uvec2 bufferSize;
	int debugMode;
	float gamma;
};
