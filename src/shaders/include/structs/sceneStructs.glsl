#define RESERVOIR_SIZE 4

struct ModelMatrices {
	mat4 transform;
	mat4 transformInverseTransposed;
};


#define SHADING_MODEL_METALLIC_ROUGHNESS 0
#define SHADING_MODEL_SPECULAR_GLOSSINESS 1

#define ALPHA_MODE_OPAQUE 0
#define ALPHA_MODE_MASK 1
#define ALPHA_MODE_BLEND 2

struct MaterialUniforms {
	// metallic-roughness:
	//   vec4 baseColorFactor;
	//   float unused;
	//   float roughnessFactor;
	//   float metallicFactor;
	// specular-glossiness:
	//   vec4 diffuseFactor;
	//   vec3 specularFactor;
	//   float glossinessFactor;
	vec4 colorParam;
	vec4 materialParam;

	vec4 emissiveFactor;
	int shadingModel;
	int alphaMode;
	float alphaCutoff;
	float normalTextureScale;
};
