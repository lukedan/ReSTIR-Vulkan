#define M_PI 3.1415926535897932384626433832795

#include "common.glsl"

float schlickFresnel(float cos) 
{
	float m = clamp(1 - cos, 0.0, 1.0);
	float sm = m * m;
	return sm * sm * m;
}

// Isotropic GTR2
float GTR2(float NdotH, float a) 
{
	float a2 = a * a;
	float t = 1.0 + (a2 - 1.0) * NdotH * NdotH;
	return a2 / (M_PI * t * t);
}

float smithG_GGX(float NdotV, float alphaG) 
{
	float a = alphaG * alphaG;
	float b = NdotV * NdotV;
	return 1.0 / (abs(NdotV) + max(sqrt(a + b - a * b), 0.0001));
}

vec3 disneyBrdfDiffuse(float cosIn, float cosOut, float cosInHalf, vec3 albedo, float roughness, float metallic) 
{
	float fresnelIn = schlickFresnel(cosIn);
	float fresnelOut = schlickFresnel(cosOut);
	float fresnelDiffuse90 = 0.5 + 2.0 * cosInHalf * cosInHalf * roughness;
	float fresnelDiffuse = mix(1.0, fresnelDiffuse90, fresnelIn) * mix(1.0, fresnelDiffuse90, fresnelOut);

	return albedo * fresnelDiffuse * (1.0 - metallic) / M_PI;
}

vec3 disneyBrdfSpecular(float cosIn, float cosOut, float cosHalf, float cosInHalf, vec3 albedo, float roughness, float metallic)
{
	// Fresnel specular (Fs)
	float albedoLum = luminance(albedo.r, albedo.g, albedo.b);
	vec3 colorTint = albedo / albedoLum;
	float specularTint = 0.0f;
	float specular = 0.5f;
	vec3 specularColor = mix(0.08 * specular * mix(vec3(1.0), colorTint, specularTint), albedo, metallic);
	float fresnelInHalf = schlickFresnel(cosInHalf);
	vec3 Fs = mix(specularColor, vec3(1.0), fresnelInHalf);
	
	float a = max(0.001, pow(roughness, 2.0));
	//  Microfacet normal distribution (Ds)
	float Ds = GTR2(cosHalf, a);

	float Gs;
	Gs = smithG_GGX(cosIn, a);
	Gs *= smithG_GGX(cosOut, a);

	return Gs * Fs * Ds;
}

vec3 disneyBrdfColor(float cosIn, float cosOut, float cosHalf, float cosInHalf, vec3 albedo, float roughness, float metallic) 
{
	if (cosIn < 0.0f) {
		return vec3(0.0f);
	}
	vec3 diffuse = disneyBrdfDiffuse(cosIn, cosOut, cosInHalf, albedo, roughness, metallic);
	vec3 specular = disneyBrdfSpecular(cosIn, cosOut, cosHalf, cosInHalf, albedo, roughness, metallic);

	return diffuse + specular;
}
