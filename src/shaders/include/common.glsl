// this file will be included by c++, so make sure everything compiles

#ifndef CPP_FUNCTION
#	define CPP_FUNCTION
#endif

CPP_FUNCTION float luminance(float r, float g, float b) {
	return 0.2126 * r + 0.7152 * g + 0.0722 * b;
}
