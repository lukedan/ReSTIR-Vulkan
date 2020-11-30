
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

struct aliasTableColumn {
	float prab; // The i's column's event i's prab
	int alias; // The i's column's another event's idx
	float oriPrab;
	int padding;
	vec4 largePadding;
};
