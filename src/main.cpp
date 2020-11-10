#include <iostream>

#include <vulkan/vulkan.hpp>

int main() {
	vk::ApplicationInfo appInfo;
	appInfo
		.setPApplicationName("ReSTIR")
		.setApiVersion(VK_MAKE_VERSION(1, 0, 0));
	vk::InstanceCreateInfo instanceInfo;
	instanceInfo
		.setPApplicationInfo(&appInfo);

	if (auto instance = vk::createInstanceUnique(instanceInfo)) {

	} else {
		std::cout << "failed to create instance\n";
	}

	return 0;
}
