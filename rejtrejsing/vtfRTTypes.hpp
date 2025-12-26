#ifndef __VTF_RTTYPES_HPP_INCLUDED__
#define __VTF_RTTYPES_HPP_INCLUDED__

#include "vtfZDeletable.hpp"
#include "vtfVector.hpp"
#include "vtfExtensions.hpp"
#include "vtfRTShaderCollection.hpp"

namespace vtf
{

struct ZAccelerationStructureGeometry : VkAccelerationStructureGeometryKHR
{
	using Geoms = add_cref<std::vector<add_cptr<ZAccelerationStructureGeometry>>>;

	ZAccelerationStructureGeometry();
	ZAccelerationStructureGeometry(ZDevice, add_cref<std::vector<Vec3>> vertices, add_cref<std::vector<uint32_t>> indices);
	add_cref<std::vector<ZBuffer>> getBuffers() const {
		return m_buffers;
	}
	VkAccelerationStructureGeometryKHR operator ()() const {
		return static_cast<add_cref<VkAccelerationStructureGeometryKHR>>(*this);
	}
	uint32_t primitiveCount() const {
		return m_primitiveCount;
	}
private:
	std::vector<ZBuffer>	m_buffers;
	uint32_t				m_primitiveCount;
};

void freeAccelerationStructure(ZDevice, VkAccelerationStructureKHR, VkAllocationCallbacksPtr);
typedef ZDeletable<VkAccelerationStructureKHR,
	decltype(&freeAccelerationStructure), &freeAccelerationStructure,
	std::integer_sequence<int, (0 + DontDereferenceParamOffset), -1, 1>,
	ZDeletableBase, ZDevice, VkAllocationCallbacksPtr,
	ZBuffer, std::vector<ZAccelerationStructureGeometry>,
	VkBool32 // alreadyBuilt
> ZBtmAccelerationStructure;
typedef ZDeletable<VkAccelerationStructureKHR,
	decltype(&freeAccelerationStructure), &freeAccelerationStructure,
	std::integer_sequence<int, (0 + DontDereferenceParamOffset), -1, 1>,
	ZDeletableBase, ZDevice, VkAllocationCallbacksPtr,
	ZDistType<SomeZero, ZBuffer>, // tlas buffer
	ZDistType<SomeOne, ZBuffer>, // instances buffer
	std::vector<ZBtmAccelerationStructure>,
	ZDistType<SizeFirst, VkDeviceSize>, // accelerationStructureSize
	ZDistType<SizeSecond, VkDeviceSize>, // updateScratchSize
	ZDistType<SizeThird, VkDeviceSize>, // buildScratchSize
	uint32_t // instance count
> ZTopAccelerationStructure;

void onEnablingRTFeatures(add_ref<DeviceCaps> caps);

ZAccelerationStructureGeometry
makeTrianglesGeometry(
	ZDevice device,
	add_cref<std::vector<Vec3>> vertices,
	add_cref<std::vector<uint32_t>> indices
);

ZBtmAccelerationStructure createBtmAccelerationStructure(
	ZAccelerationStructureGeometry::Geoms geoms);

VkTransformMatrixKHR getTransformMatrix();
struct BLAS
{
	ZBtmAccelerationStructure structure;
	uint32_t instanceShaderBindingTableRecordOffset;
	uint32_t instanceCustomIndex;
	VkTransformMatrixKHR	transform;
	uint32_t				mask;
	VkGeometryInstanceFlagsKHR flags;
	BLAS(ZBtmAccelerationStructure,
		uint32_t sbtOffset = 0u,
		uint32_t instIndex = 0u,
		VkTransformMatrixKHR = getTransformMatrix(),
		uint32_t amask = 0xFF,
		VkGeometryInstanceFlagsKHR = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR);
};

ZTopAccelerationStructure createTopAccelerationStructure(
	BLAS btm, std::optional<std::vector<BLAS>> otherBtms = {});

void commandBufferBuildAccelerationStructure(ZCommandBuffer cmd, ZBtmAccelerationStructure bottomAS);
void commandBufferBuildAccelerationStructure(ZCommandBuffer cmd, ZTopAccelerationStructure topAS);

struct SBT
{
	explicit SBT(ZPipeline rtPipeline, add_cref<RTShaderCollection::SBTShaderGroup>) noexcept;
	~SBT() noexcept;

	bool buildOnce();
	void traceRays(ZCommandBuffer cmd, uint32_t width, uint32_t height, uint32_t depth = 1u);

	add_cref<VkStridedDeviceAddressRegionKHR> raygenRegion() const;
	add_cref<VkStridedDeviceAddressRegionKHR> missRegion() const;
	add_cref<VkStridedDeviceAddressRegionKHR> hitRegion() const;
	add_cref<VkStridedDeviceAddressRegionKHR> callableRegion() const;

private:
	struct Impl;
	std::unique_ptr<Impl> m_impl;
};

} // namespace vtf

#endif // __VTF_RTTYPES_HPP_INCLUDED__
