#include "vtfRTLayoutManager.hpp"

namespace vtf
{

uint32_t RTLayoutManager::addBinding(ZTopAccelerationStructure, VkShaderStageFlags stages, VkDescriptorBindingFlags bindingFlags)
{
	UNREF(stages);
	UNREF(bindingFlags);
	return 0;
}

} // namespace vtf
