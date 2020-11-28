struct Rand {
	uint context;
};

uint randUint16(inout Rand r) {
	r.context = r.context * 1103515245 + 12345;
	return r.context >> 16;
}

float randFloat(inout Rand r) {
	return randUint16(r) / 65536.0f;
}
