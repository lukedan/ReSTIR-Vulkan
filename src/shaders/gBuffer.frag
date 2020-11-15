#version 450
#extension GL_ARB_separate_shader_objects: enable

layout (location = 0) in vec3 inNormal;
layout (location = 1) in vec4 inColor;
layout (location = 2) in vec2 inUv;

layout (location = 0) out vec3 outAlbedo;
layout (location = 1) out vec3 outNormal;

void main() {
    outAlbedo = inColor.xyz;
    outNormal = normalize(inNormal);
}
