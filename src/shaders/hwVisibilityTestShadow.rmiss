#version 460
#extension GL_EXT_ray_tracing : require

layout(location = 0) rayPayloadInEXT bool isShadowed;

void main()
{
  isShadowed = false;
}
