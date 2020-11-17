#version 450

vec2 positions[4] = vec2[](
    vec2(-1.0f, -1.0f),
    vec2(-1.0f,  1.0f),
    vec2( 1.0f, -1.0f),
    vec2( 1.0f,  1.0f)
);

layout (location = 0) out vec2 outUv;

void main() {
    vec2 position = positions[gl_VertexIndex];
    gl_Position = vec4(position, 0.0, 1.0);
    outUv = (position + 1.0f) * 0.5f;
}
