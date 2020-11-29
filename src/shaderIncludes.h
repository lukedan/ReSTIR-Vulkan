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
#define uint ::std::uint32_t
#define vec2 ::nvmath::vec2
	// do NOT define or use vec3 or mat3 - they may lead to alignment issues
#define vec4 ::nvmath::vec4
#define ivec2 ::nvmath::ivec2
#define ivec4 ::nvmath::ivec4
#define uvec2 ::nvmath::uvec2
#define uvec4 ::nvmath::uvec4
#define mat4 ::nvmath::mat4

#define CPP_FUNCTION inline

#include "shaders/include/common.glsl"
#include "shaders/include/structs/aabbTree.glsl"
#include "shaders/include/structs/lightingPassStructs.glsl"
#include "shaders/include/structs/restirStructs.glsl"
#include "shaders/include/structs/sceneStructs.glsl"
#include "shaders/include/structs/light.glsl"

#ifdef SHADER_DEFINE_INT_UB
#	undef int
#endif
#undef uint
#undef vec2
#undef vec4
#undef ivec2
#undef ivec4
#undef uvec2
#undef uvec4
#undef mat4

#undef CPP_FUNCTION
}
