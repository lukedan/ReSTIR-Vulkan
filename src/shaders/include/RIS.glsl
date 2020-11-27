// Generate a random unsigned int from two unsigned int values, using 16 pairs
// of rounds of the Tiny Encryption Algorithm. See Zafar, Olano, and Curtis,
// "GPU Random Numbers via the Tiny Encryption Algorithm"
uint tea(uint val0, uint val1)
{
    uint v0 = val0;
    uint v1 = val1;
    uint s0 = 0;

    for (uint n = 0; n < 16; n++)
    {
        s0 += 0x9e3779b9;
        v0 += ((v1 << 4) + 0xa341316c) ^ (v1 + s0) ^ ((v1 >> 5) + 0xc8013ea4);
        v1 += ((v0 << 4) + 0xad90777d) ^ (v0 + s0) ^ ((v0 >> 5) + 0x7e95761e);
    }

    return v0;
}

// Generate a random unsigned int in [0, 2^24) given the previous RNG state
// using the Numerical Recipes linear congruential generator
uint lcg(inout uint prev)
{
    uint LCG_A = 1664525u;
    uint LCG_C = 1013904223u;
    prev = (LCG_A * prev + LCG_C);
    return prev & 0x00FFFFFF;
}

// Generate a random float in [0, 1) given the previous RNG state
float rnd(inout uint prev)
{
    return (float(lcg(prev)) / float(0x01000000));
}


// Randomly sampling around +Z
vec3 samplingHemisphere(inout uint seed, in vec3 x, in vec3 y, in vec3 z)
{
    float r1 = rnd(seed);
    float r2 = rnd(seed);
    float sq = sqrt(1.0 - r2);

    vec3 direction = vec3(cos(2 * M_PI * r1) * sq, sin(2 * M_PI * r1) * sq, sqrt(r2));
    direction = direction.x * x + direction.y * y + direction.z * z;

    return direction;
}



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
vec3 pickPointOnTriangle(uint seed1, uint seed2, vec3 p1, vec3 p2, vec3 p3) {
    float r1 = rnd(seed1);
    float r2 = rnd(seed2); 
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
