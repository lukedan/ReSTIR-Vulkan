#version 450

layout (push_constant) uniform PushConstants {
    mat4 transform;
} constants;

layout (location = 0) in vec3 inPosition;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec4 inColor;
layout (location = 3) in vec2 inUv;

layout (location = 0) out vec3 outNormal;
layout (location = 1) out vec4 outColor;
layout (location = 2) out vec2 outUv;

void main() {
    gl_Position = constants.transform * vec4(inPosition, 1.0f);
    outNormal = inNormal;
    outColor = inColor;
    outUv = inUv;
}
