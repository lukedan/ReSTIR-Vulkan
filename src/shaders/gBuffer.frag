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

// sRGB to linear approximation
// see http://chilliant.blogspot.com/2012/08/srgb-approximations-for-hlsl.html
vec4 SRGBtoLINEAR(vec4 srgbIn, float gamma)
{
  return vec4(pow(srgbIn.xyz, vec3(gamma)), srgbIn.w);
}

void main() {
    vec4  baseColor = vec4(0.0, 0.0, 0.0, 1.0);
    // outAlbedo = inColor.xyz;
    // vec2 tempUV = inUv;
    outNormal = normalize(vec3(inUv, 0.0));
    // outAlbedo = vec3(in, 1.0);
    // outAlbedo = vec3(material.pbrBaseColorFactor.xyz);
    // outNormal = normalize(inNormal);
    // outAlbedo = vec3(texture(texSampler[0], inUv).xyz);

    
    if(material.pbrBaseColorTexture > -1){
         // baseColor *= SRGBtoLINEAR(texture(texSampler[nonuniformEXT(material.pbrBaseColorTexture)], inUv), 2.2);
         // baseColor = vec4(0.0, 0.0, 0.0, 1.0);
         baseColor = texture(texSampler[0], inUv);
    }else{
         baseColor = material.pbrBaseColorFactor;
    }
    // baseColor = vec4(0.0, 0.0, 0.0, 1.0);
    outAlbedo = vec3(baseColor.xyz);
}
