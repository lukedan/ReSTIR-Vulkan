#version 450

layout (set = 0, binding = 0) uniform Uniforms {
    mat4 projectionViewMatrix;
} uniforms;
layout (set = 1, binding = 0) uniform Matrices {
    mat4 modelMatrix;
    mat4 modelInverseTransposed;
} matrices;

layout (location = 0) in vec3 inPosition;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec4 inColor;
layout (location = 3) in vec2 inUv;

layout (location = 0) out vec3 outNormal;
layout (location = 1) out vec4 outColor;
layout (location = 2) out vec2 outUv;

void main() {
    gl_Position = uniforms.projectionViewMatrix * matrices.modelMatrix * vec4(inPosition, 1.0f);
    outNormal = (matrices.modelInverseTransposed * vec4(inNormal, 0.0f)).xyz;
    outColor = inColor;
    outUv = inUv;
}
