#include "vtfDSBMgr.hpp"
#include "vtfRTTypes.hpp"
#include "vtfZUtils.hpp"

namespace vtf
{
constexpr ZShaderStageRayTracingFlags ZShaderStageRayTracingFlagsAll (
	VK_SHADER_STAGE_RAYGEN_BIT_KHR,
	VK_SHADER_STAGE_MISS_BIT_KHR,
	VK_SHADER_STAGE_CALLABLE_BIT_KHR,
	VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
	VK_SHADER_STAGE_ANY_HIT_BIT_KHR,
	VK_SHADER_STAGE_INTERSECTION_BIT_KHR
);

class RTLayoutManager : public DescriptorSetBindingManager
{
public:
	using DescriptorSetBindingManager::DescriptorSetBindingManager;
	using DescriptorSetBindingManager::addBinding;

	uint32_t addBinding(ZTopAccelerationStructure tlas,
						ZShaderStageRayTracingFlags = ZShaderStageRayTracingFlagsAll, VkDescriptorBindingFlags = 0u);
	uint32_t addBinding(uint32_t suggestedBinding, ZTopAccelerationStructure,
						ZShaderStageRayTracingFlags = ZShaderStageRayTracingFlagsAll, VkDescriptorBindingFlags = 0u);

	virtual void updateDescriptorSet(ZDescriptorSet descriptorSet) override;
	virtual bool isDescryptorTypeSupported(VkDescriptorType type) const override;
};

} // namespace vtf

