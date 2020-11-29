#define USE_PCG32

#ifdef USE_PCG32
#	extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

// https://www.pcg-random.org/
struct Rand {
	uint64_t state;
	uint64_t inc;
};

uint randUint(inout Rand r) {
	uint64_t oldState = r.state;
	r.state = oldState * 6364136223846793005ul + r.inc;
	uint xorShifted = uint(((oldState >> 18u) ^ oldState) >> 27u);
	uint rot = uint(oldState >> 59u);
	return (xorShifted >> rot) | (xorShifted << ((-rot) & 31u));
}

Rand seedRand(uint64_t seed, uint64_t seq) {
	Rand result;
	result.state = 0;
	result.inc = (seq << 1u) | 1u;
	randUint(result);
	result.state += seed;
	randUint(result);
	return result;
}

float randFloat(inout Rand r) {
	return randUint(r) / 4294967296.0f;
}
#else
struct Rand {
	uint state;
};

uint randUint16(inout Rand r) {
	r.state = r.state * 1103515245 + 12345;
	return r.state >> 16;
}

Rand seedRand(uint seed, uint seq) {
	Rand result;
	result.state = seed * 9349 + seq;
	randUint16(result);
	return result;
}

float randFloat(inout Rand r) {
	return randUint16(r) / 65536.0f;
}
#endif
