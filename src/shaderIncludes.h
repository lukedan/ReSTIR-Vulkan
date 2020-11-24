#pragma once

#include <nvmath_glsltypes.h>

namespace shader {
#ifdef SHADER_DEFINE_INT_UB
#	define int ::std::int32_t
#else
	static_assert(
		sizeof(int) == sizeof(std::int32_t),
		"int size mismatch - define SHADER_DEFINE_INT_UB to force int to use int32_t, "
		"WHICH RESULTS IN UNDEFINED BEHAVIOR"
	);
#endif
#define vec2 ::nvmath::vec2
#define vec4 ::nvmath::vec4
#define ivec2 ::nvmath::ivec2
#define ivec4 ::nvmath::ivec4
#define mat4 ::nvmath::mat4

#include "shaders/include/structs/aabbTree.glsl"
#include "shaders/include/structs/lightingPassStructs.glsl"

#ifdef SHADER_DEFINE_INT_UB
#	undef int
#endif
#undef vec2
#undef vec4
#undef ivec2
#undef ivec4
#undef mat4
}
