#version 450
#extension GL_ARB_separate_shader_objects: enable
#extension GL_EXT_scalar_block_layout : enable

layout (set = 2, binding = 0) uniform sampler2D uniAlbedo;
layout (set = 2, binding = 1) uniform sampler2D uniNormal;
layout (set = 2, binding = 1) uniform sampler2D uniMetallicRoughness;

layout (location = 0) in vec3 inNormal;
layout (location = 1) in vec3 inTangent;
layout (location = 2) in vec4 inColor;
layout (location = 3) in vec2 inUv;

layout (location = 0) out vec3 outAlbedo;
layout (location = 1) out vec3 outNormal;

void main() {
    // this front facing flag may come in handy later when handling double-sided geometry
    vec3 bitangent = /*(gl_FrontFacing ? 1.0f : -1.0f) **/ cross(inNormal, inTangent);
    vec3 normalTex = texture(uniNormal, inUv).xyz * 2.0f - 1.0f;
    outNormal = normalize(normalTex.x * inTangent + normalTex.y * bitangent + normalTex.z * inNormal);

    outAlbedo = texture(uniAlbedo, inUv).xyz;

    // TODO metallicRoughness
}
