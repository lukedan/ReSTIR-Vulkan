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

float disneyBrdfDiffuseFactor(float cosIn, float cosOut, float cosInHalf, float roughness, float metallic) {
	float fresnelIn = schlickFresnel(cosIn);
	float fresnelOut = schlickFresnel(cosOut);
	float fresnelDiffuse90 = 0.5 + 2.0 * cosInHalf * cosInHalf * roughness;
	float fresnelDiffuse = mix(1.0, fresnelDiffuse90, fresnelIn) * mix(1.0, fresnelDiffuse90, fresnelOut);
	return fresnelDiffuse * (1.0f - metallic) / M_PI;
}
vec3 disneyBrdfDiffuse(float cosIn, float cosOut, float cosInHalf, vec3 albedo, float roughness, float metallic) {
	return albedo * disneyBrdfDiffuseFactor(cosIn, cosOut, cosInHalf, roughness, metallic);
}
float disneyBrdfDiffuseLuminance(float cosIn, float cosOut, float cosInHalf, float luminance, float roughness, float metallic) {
	return luminance * disneyBrdfDiffuseFactor(cosIn, cosOut, cosInHalf, roughness, metallic);
}

/// Returns (fresnelInHalf, Gs * Ds)
vec2 disneyBrdfSpecularFactors(float cosIn, float cosOut, float cosHalf, float cosInHalf, float roughness, float metallic) {
	// Fresnel specular (Fs)
	float fresnelInHalf = schlickFresnel(cosInHalf);

	float a = max(0.001, pow(roughness, 2.0));
	//  Microfacet normal distribution (Ds)
	float Ds = GTR2(cosHalf, a);

	float Gs;
	Gs = smithG_GGX(cosIn, a);
	Gs *= smithG_GGX(cosOut, a);

	return vec2(fresnelInHalf, Gs * Ds);
}
vec3 disneyBrdfSpecular(float cosIn, float cosOut, float cosHalf, float cosInHalf, vec3 albedo, float roughness, float metallic) {
	vec2 factors = disneyBrdfSpecularFactors(cosIn, cosOut, cosHalf, cosInHalf, roughness, metallic);

	vec3 specularColor = mix(vec3(0.04f), albedo, metallic);
	vec3 Fs = mix(specularColor, vec3(1.0), factors.x);
	
	return Fs * factors.y;
}
float disneyBrdfSpecularLuminance(float cosIn, float cosOut, float cosHalf, float cosInHalf, float luminance, float roughness, float metallic) {
	vec2 factors = disneyBrdfSpecularFactors(cosIn, cosOut, cosHalf, cosInHalf, roughness, metallic);

	float specularLuminance = mix(0.04f, luminance, metallic);
	float Fs = mix(specularLuminance, 1.0f, factors.x);

	return Fs * factors.y;
}

vec3 disneyBrdfColor(float cosIn, float cosOut, float cosHalf, float cosInHalf, vec3 albedo, float roughness, float metallic) {
	if (cosIn < 0.0f) {
		return vec3(0.0f);
	}
	vec3 diffuse = disneyBrdfDiffuse(cosIn, cosOut, cosInHalf, albedo, roughness, metallic);
	vec3 specular = disneyBrdfSpecular(cosIn, cosOut, cosHalf, cosInHalf, albedo, roughness, metallic);

	return diffuse + specular;
}
float disneyBrdfLuminance(float cosIn, float cosOut, float cosHalf, float cosInHalf, float albedoLuminance, float roughness, float metallic) {
	if (cosIn < 0.0f) {
		return 0.0f;
	}
	float diffuse = disneyBrdfDiffuseLuminance(cosIn, cosOut, cosInHalf, albedoLuminance, roughness, metallic);
	float specular = disneyBrdfSpecularLuminance(cosIn, cosOut, cosHalf, cosInHalf, albedoLuminance, roughness, metallic);

	return diffuse + specular;
}
