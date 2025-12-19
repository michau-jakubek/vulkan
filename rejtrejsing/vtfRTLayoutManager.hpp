#include "vtfDSBMgr.hpp"
#include "vtfRTTypes.hpp"

namespace vtf
{
constexpr uint32_t nil = 0;

class RTLayoutManager : public DescriptorSetBindingManager
{
public:
	using DescriptorSetBindingManager::DescriptorSetBindingManager;

	uint32_t addBinding(ZTopAccelerationStructure, VkShaderStageFlags stages, VkDescriptorBindingFlags = 0u);
};

} // namespace vtf

