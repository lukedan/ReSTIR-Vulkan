#version 450
#extension GL_ARB_separate_shader_objects: enable
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : enable

layout (set = 2, binding = 0) uniform sampler2D texSampler[];

layout (location = 0) in vec3 inNormal;
layout (location = 1) in vec4 inColor;
layout (location = 2) in vec2 inUv;

layout (location = 0) out vec3 outAlbedo;
layout (location = 1) out vec3 outNormal;

layout(push_constant) uniform shaderInformation
{
  int shadingModel;  // 0: metallic-roughness, 1: specular-glossiness

  // PbrMetallicRoughness
  vec4  pbrBaseColorFactor;
  int   pbrBaseColorTexture;
  float pbrMetallicFactor;
  float pbrRoughnessFactor;
  int   pbrMetallicRoughnessTexture;

  // KHR_materials_pbrSpecularGlossiness
  vec4  khrDiffuseFactor;
  int   khrDiffuseTexture;
  vec3  khrSpecularFactor;
  float khrGlossinessFactor;
  int   khrSpecularGlossinessTexture;

  int   emissiveTexture;
  vec3  emissiveFactor;
  int   alphaMode;
  float alphaCutoff;
  bool  doubleSided;

  int   normalTexture;
  float normalTextureScale;
  int   occlusionTexture;
  float occlusionTextureStrength;
}
material;

#include "textures.glsl"
#include "functions.glsl"
#include "tonemapping.glsl"


void main() {
    vec4  baseColor = vec4(0.0, 0.0, 0.0, 1.0);
    // outAlbedo = inColor.xyz;
    // vec2 tempUV = inUv;
    outNormal = normalize(vec3(inUv, 0.0));
    // outAlbedo = vec3(in, 1.0);
    // outAlbedo = vec3(material.pbrBaseColorFactor.xyz);
    // outNormal = normalize(inNormal);
    // outAlbedo = vec3(texture(texSampler[0], inUv).xyz);

    baseColor = material.pbrBaseColorFactor;
    if(material.pbrBaseColorTexture > -1){
         baseColor = texture(texSampler[nonuniformEXT(material.pbrBaseColorTexture)], inUv);
         // baseColor = texture(texSampler[nonuniformEXT(1)], inUv);
    }
    // baseColor = vec4(0.0, 0.0, 0.0, 1.0);
    outAlbedo = vec3(baseColor.xyz);
}
