#include "vtfRTLayoutManager.hpp"

namespace vtf
{

uint32_t RTLayoutManager::addBinding (
	uint32_t suggestedBinding, ZTopAccelerationStructure tlas,
	ZShaderStageRayTracingFlags stages, VkDescriptorBindingFlags bindingFlags)
{
	ASSERTMSG(tlas.has_handle(), "Acceleration structure must have a handle");
	auto index = std::type_index(typeid(typename add_extent<ZTopAccelerationStructure>::type));
	uint32_t binding = addBinding_(suggestedBinding, index, {/*buffer*/ }, {/*view*/ }, {/*sampler*/ },
		VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, false, stages(), bindingFlags);
	m_extbindings.back().tlas = tlas;
	return binding;
}

uint32_t RTLayoutManager::addBinding (ZTopAccelerationStructure tlas,
	ZShaderStageRayTracingFlags stages, VkDescriptorBindingFlags bindingFlags)
{
	return addBinding(INVALID_UINT32, tlas, stages, bindingFlags);
}

bool RTLayoutManager::isDescryptorTypeSupported(VkDescriptorType type) const
{
	if (VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR == type)
		return true;
	return DescriptorSetBindingManager::isDescryptorTypeSupported(type);
}

void RTLayoutManager::updateDescriptorSet (ZDescriptorSet descriptorSet)
{
	DescriptorSetBindingManager::updateDescriptorSet(descriptorSet);

	for (add_cref<ExtBinding> b : m_extbindings)
	{
		if (b.descriptorType != VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR) {
			continue;
		}

		VkWriteDescriptorSetAccelerationStructureKHR asInfo{};
		asInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
		asInfo.accelerationStructureCount = 1u;
		asInfo.pAccelerationStructures = std::any_cast<ZTopAccelerationStructure>(b.tlas).ptr();

		VkWriteDescriptorSet write{};
		write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		write.dstSet = *descriptorSet;
		write.dstBinding = b.binding;
		write.dstArrayElement = 0u;
		write.descriptorCount = 1u;
		write.descriptorType = b.descriptorType;
		write.pNext = &asInfo;

		vkUpdateDescriptorSets(*device,
			1u,		// descriptorWriteCount
			&write,	// pDescriptorWrites
			0u,		// copyCount,
			nullptr	// copyParams
		);
	}
}

} // namespace vtf
