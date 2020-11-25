#version 460 core
#extension GL_EXT_ray_tracing : enable

layout(location = 0) rayPayloadInEXT bool isShadowed;

void main() {
  isShadowed = false;
}
