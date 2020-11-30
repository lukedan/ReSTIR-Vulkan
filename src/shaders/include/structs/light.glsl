
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
	vec4 normalArea;
};

struct aliasTableColumn {
	float prob; // The i's column's event i's prob
	int alias; // The i's column's another event's idx
	float oriProb;
};
