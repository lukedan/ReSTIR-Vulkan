
struct pointLight {
	vec3 pos;
	float intensity;
	vec3 color;
};

struct triLight {
	vec3 p1;
	vec3 p2;
	vec3 p3;
	vec3 emissiveFactor;
	float area;
};
