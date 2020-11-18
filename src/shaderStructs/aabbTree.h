#pragma once

#include <nvmath_glsltypes.h>

struct AabbNode {
	nvmath::vec4 leftAabbMin;
	nvmath::vec4 leftAabbMax;
	nvmath::vec4 rightAabbMin;
	nvmath::vec4 rightAabbMax;
	int32_t leftChild;
	int32_t rightChild;
};

struct Triangle {
	nvmath::vec4 p1;
	nvmath::vec4 p2;
	nvmath::vec4 p3;
};
