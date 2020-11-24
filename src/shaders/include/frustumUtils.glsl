float sceneDepthToWorldDepth(float depth, float near, float far) {
	return near * far / (far + depth * (near - far));
}
vec3 fragCoordDepthToViewPos(vec2 uv, float worldDepth, float tanHalfFovY, float aspectRatio) {
	uv = uv * 2.0f - 1.0f; // [0, 1] to [-1, 1]
	uv.x *= aspectRatio;
	return vec3(uv * worldDepth * tanHalfFovY, worldDepth);
}
