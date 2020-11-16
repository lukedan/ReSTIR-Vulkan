#version 450
#extension GL_ARB_separate_shader_objects: enable

layout (binding = 0, location = 0) uniform sampler2D uniAlbedo;
layout (binding = 0, location = 1) uniform sampler2D uniNormal;
layout (binding = 0, location = 2) uniform sampler2D uniDepth;

layout (location = 0) in vec2 inUv;

layout (location = 0) out vec4 outColor;

void main() {
    vec3 albedo = texture(uniAlbedo, inUv).xyz;
    vec3 normal = texture(uniNormal, inUv).xyz;
    float depth = texture(uniDepth, inUv).x;
    /*outColor = vec4((vec3(normal) + 1.0f) * 0.5f, 1.0f);*/
    outColor = vec4(depth, depth, depth, 1.0f);
    /*outColor = vec4(inUv, 0.0f, 1.0f);*/
}
