#pragma once

#include <nvmath_glsltypes.h>

namespace shader {
#define int ::std::int32_t
#define vec2 ::nvmath::vec2
#define vec4 ::nvmath::vec4
#define ivec2 ::nvmath::ivec2
#define ivec4 ::nvmath::ivec4
#define mat4 ::nvmath::mat4

#include "shaders/include/structs/aabbTree.glsl"

#undef int
#undef vec2
#undef vec4
#undef ivec2
#undef ivec4
#undef mat4
}
