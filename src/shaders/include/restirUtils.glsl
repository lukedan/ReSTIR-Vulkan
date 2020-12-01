#include "disneyBRDF.glsl"

vec3 evaluatePHat(
	vec3 worldPos, vec3 lightPos, vec3 camPos, vec3 normal,
	vec3 albedo, vec3 emission, float roughness, float metallic
) {
	vec3 wi = lightPos - worldPos;
	if (dot(wi, normal) < 0.0f) {
		return vec3(0.0f);
	}

	float sqrDist = dot(wi, wi);
	wi /= sqrt(sqrDist);
	vec3 wo = normalize(vec3(camPos) - worldPos);

	float cosIn = dot(normal, wi);
	float cosOut = dot(normal, wo);
	vec3 halfVec = normalize(wi + wo);
	float cosHalf = dot(normal, halfVec);
	float cosInHalf = dot(wi, halfVec);

	return emission * disneyBrdfColor(cosIn, cosOut, cosHalf, cosInHalf, albedo, roughness, metallic) * cosIn / sqrDist;
}
