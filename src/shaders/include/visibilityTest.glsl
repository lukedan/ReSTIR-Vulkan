bool testVisibility(vec3 p1, vec3 p2) {
	float tMin = 0.001f;
	vec3 dir = p2 - p1;

#ifdef HARDWARE_RAY_TRACING
	isShadowed = true;

	float curTMax = length(dir);
	dir /= curTMax;

	traceRayEXT(
		acc,            // acceleration structure
		gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsOpaqueEXT | gl_RayFlagsSkipClosestHitShaderEXT,       // rayFlags
		0xFF,           // cullMask
		0,              // sbtRecordOffset
		0,              // sbtRecordStride
		0,              // missIndex
		p1,             // ray origin
		tMin,           // ray min range
		dir,            // ray direction
		curTMax - 2.0f * tMin,           // ray max range
		0               // payload (location = 0)
	);

	return isShadowed;
#else
	vec3 offset = tMin * normalize(dir);
	return !raytrace(p1 + offset, dir - 2 * offset);
#endif
}
