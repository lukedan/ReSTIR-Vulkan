#version 450

#include "include/structs/sceneStructs.glsl"

layout (set = 0, binding = 0) uniform Uniforms {
	mat4 projectionViewMatrix;
} uniforms;
layout (set = 1, binding = 0) uniform Matrices {
	ModelMatrices matrices;
};

layout (location = 0) in vec3 inPosition;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec4 inTangent;
layout (location = 3) in vec4 inColor;
layout (location = 4) in vec2 inUv;

layout (location = 0) out vec3 outNormal;
layout (location = 1) out vec4 outTangent;
layout (location = 2) out vec4 outColor;
layout (location = 3) out vec2 outUv;

void main() {
	gl_Position = uniforms.projectionViewMatrix * matrices.transform * vec4(inPosition, 1.0f);

	outNormal = normalize((matrices.transformInverseTransposed * vec4(inNormal, 0.0f)).xyz);
	outTangent.xyz = normalize((matrices.transform * vec4(inTangent.xyz, 0.0f)).xyz);
	outTangent.w = inTangent.w;
	outColor = inColor;
	outUv = inUv;
}
