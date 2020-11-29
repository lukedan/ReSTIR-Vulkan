
struct pointLight {
	vec4 pos;
	vec4 color;
	float intensity;
};

struct triLight {
	vec4 p1;
	vec4 p2;
	vec4 p3;
	vec4 emissiveFactor;
	float area;
};
