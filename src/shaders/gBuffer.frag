#version 450
#extension GL_ARB_separate_shader_objects: enable

layout (set = 2, binding = 0) uniform sampler2D texSampler;

layout (location = 0) in vec3 inNormal;
layout (location = 1) in vec4 inColor;
layout (location = 2) in vec2 inUv;

layout (location = 0) out vec3 outAlbedo;
layout (location = 1) out vec3 outNormal;

void main() {
    // outAlbedo = inColor.xyz;
    vec2 tempUV = inUv;
    outNormal = normalize(vec3(inUv, 0.0));
    // outAlbedo = vec3(in, 1.0);
    // outAlbedo = vec3(inUv, 0.0);
    // outNormal = normalize(inNormal);
    outAlbedo = vec3(texture(texSampler, inUv).xyz);
}
