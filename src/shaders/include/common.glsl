// this file will be included by c++, so make sure everything compiles

#ifndef CPP_FUNCTION
#	define CPP_FUNCTION
#endif

CPP_FUNCTION float luminance(float r, float g, float b) {
	return 0.2126f * r + 0.7152f * g + 0.0722f * b;
}
