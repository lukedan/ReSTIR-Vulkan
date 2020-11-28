void RIS(int M, vec2 pixel, out vec3 y, out float sumWeights) {
	y = vec3(1.0, 1.0, 1.0);
	sumWeights = 0.5;
    int maxM = 10;
    if (M > maxM) {
        y = vec3(0.0, 0.0, 0.0);
        sumWeights = -1.0;
        return;
    }
    
    vec3 xSet[10];
    float wSet[10];

    // Generate proposals
    for (int i = 0; i < M; ++i) {

    }

    // Select from candidates
}

// https://math.stackexchange.com/questions/18686/uniform-random-point-in-triangle
vec3 pickPointOnTriangle(float r1, float r2, vec3 p1, vec3 p2, vec3 p3) {
    float sqrt_r1 = sqrt(r1);
    return (1.0 - sqrt_r1) * p1 + (sqrt_r1 * (1.0 - r2)) * p2 + (r2 * sqrt_r1) * p3;
}

float DistanceSquared(vec3 a, vec3 b) {
    float len = length(a - b);
    return len * len;
}

// http://www.pbr-book.org/3ed-2018/Light_Transport_I_Surface_Reflection/Sampling_Light_Sources.html#sec:sampling-lights
float triLightPDF(triLight sampledTriLight, vec3 wi, vec3 sampledTriLightPos, vec3 refPoint) {
    vec3 lightNormal = normalize(cross(sampledTriLight.p2.xyz - sampledTriLight.p1.xyz, sampledTriLight.p3.xyz - sampledTriLight.p1.xyz));
    return DistanceSquared(refPoint, sampledTriLightPos) / (abs(dot(lightNormal, wi)) * sampledTriLight.area);
}
