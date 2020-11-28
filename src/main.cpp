#define TINYGLTF_IMPLEMENTATION
#include <tiny_gltf.h>
#undef TINYGLTF_IMPLEMENTATION

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#undef STB_IMAGE_IMPLEMENTATION

#ifdef _MSC_VER
#	define STBI_MSC_SECURE_CRT
#endif
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>
#undef STB_IMAGE_WRITE_IMPLEMENTATION

#include "app.h"

int main() {
	App app;
	app.mainLoop();
	return 0;
}
