#include <iostream>
#include <cstdint>
#if VTF_SAFETY_CRITICAL
  #include <vulkan/vulkan_sc_core.h>
#else
  #include <vulkan/vulkan_core.h>
#endif

int main()
{
	std::cout
		<< VK_API_VERSION_VARIANT(VK_HEADER_VERSION_COMPLETE) << ','
		<< VK_API_VERSION_MAJOR(VK_HEADER_VERSION_COMPLETE) << ','
		<< VK_API_VERSION_MINOR(VK_HEADER_VERSION_COMPLETE) << ','
		<< VK_API_VERSION_PATCH(VK_HEADER_VERSION_COMPLETE);

	return 0;
}
