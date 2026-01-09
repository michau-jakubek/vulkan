#ifndef __VTF_RTTYPES_HPP_INCLUDED__
#define __VTF_RTTYPES_HPP_INCLUDED__

#include "vtfZDeletable.hpp"
#include "vtfVector.hpp"
#include "vtfExtensions.hpp"
#include "vtfRTShaderCollection.hpp"
#include "vtfRTPipeline.hpp"

namespace vtf
{

using ZShaderStageRayTracingFlags = Flags<VkShaderStageFlags, VkShaderStageFlagBits>;

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
	ZDistType<SomeZero, ZBuffer>, // blas buffer
	ZDistType<SomeTwo, ZBuffer>, // scratch buffer
	std::vector<ZAccelerationStructureGeometry>,
	ZDistType<SizeFirst, VkDeviceSize>, // accelerationStructureSize
	ZDistType<SizeSecond, VkDeviceSize>, // updateScratchSize
	ZDistType<SizeThird, VkDeviceSize>, // buildScratchSize
	VkBool32 // alreadyBuilt
> ZBtmAccelerationStructure;

typedef ZDeletable<VkAccelerationStructureKHR,
	decltype(&freeAccelerationStructure), &freeAccelerationStructure,
	std::integer_sequence<int, (0 + DontDereferenceParamOffset), -1, 1>,
	ZDeletableBase, ZDevice, VkAllocationCallbacksPtr,
	ZDistType<SomeZero, ZBuffer>, // tlas buffer
	ZDistType<SomeOne, ZBuffer>, // instances buffer
	ZDistType<SomeTwo, ZBuffer>, // scratch buffer
	std::vector<ZBtmAccelerationStructure>,
	ZDistType<SizeFirst, VkDeviceSize>, // accelerationStructureSize
	ZDistType<SizeSecond, VkDeviceSize>, // updateScratchSize
	ZDistType<SizeThird, VkDeviceSize>, // buildScratchSize
	uint32_t // instance count
> ZTopAccelerationStructure;

void onEnablingRayTracingFeatures(add_ref<DeviceCaps> caps);

ZAccelerationStructureGeometry
makeTrianglesGeometry(
	ZDevice device,
	add_cref<std::vector<Vec3>> vertices,
	add_cref<std::vector<uint32_t>> indices
);

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

ZBtmAccelerationStructure
createBtmAccelerationStructure(ZAccelerationStructureGeometry::Geoms geoms);

ZTopAccelerationStructure
createTopAccelerationStructure(ZDevice device, uint32_t maxInstances);

template<class AccStruct>
VkDeviceAddress accelerationStructureGetDeviceAddress(AccStruct str);


void commandBufferBuildAccelerationStructure(
	ZCommandBuffer cmd, ZBtmAccelerationStructure bottomAS);
void commandBufferBuildAccelerationStructure(
	ZCommandBuffer cmd, ZTopAccelerationStructure tlas,
	BLAS firstInstance, std::optional<std::vector<BLAS>> otherInstances = {});

struct SBTHandles;
struct SBTAny
{
protected:
	using Data = std::vector<std::byte>;
	add_cref<SBTHandles>	m_handles;
	const std::size_t	m_rgenDataSize;
	const std::size_t	m_missDataSize;
	const std::size_t	m_callDataSize;
	const std::size_t	m_hitDataSize;
	Data				m_data;
	VkStridedDeviceAddressRegionKHR	m_rgenRegion{};
	VkStridedDeviceAddressRegionKHR m_missRegion{};
	VkStridedDeviceAddressRegionKHR m_callRegion{};
	VkStridedDeviceAddressRegionKHR m_hitRegion{};
	ZBuffer				m_sbtBuffer;
	ZBuffer				m_sbtMissBuffer;
	ZBuffer				m_sbtCallBuffer;
	ZBuffer				m_sbtHitBuffer;

	explicit SBTAny (add_cref<SBTHandles> handles,
	                 size_t rgDataSize, size_t msDataSize, size_t htDataSize, size_t clDataSize)
		: m_handles(handles)
		, m_rgenDataSize(rgDataSize)
		, m_missDataSize(msDataSize)
		, m_callDataSize(clDataSize)
		, m_hitDataSize(htDataSize)
		, m_data(rgDataSize + msDataSize + htDataSize + clDataSize)
	{
	}
	void setRegionData (uint32_t region, void_cptr data);
	bool buildOnce (ZCommandBuffer cmd, int method = 0);
	bool buildOnce1();
	bool buildOnce ();
	void traceRays (ZCommandBuffer cmd, uint32_t width, uint32_t height, uint32_t depth = 1u);

	std::array<ZBuffer, sted_array_size<rtdetails::PipelineShaderGroupOrder::Order>>
		getBuffers (add_cref<rtdetails::PipelineShaderGroupOrder> order) const;
};
struct SBTHandles
{
	explicit SBTHandles (ZPipeline, add_cref<RTShaderCollection::SBTShaderGroup>, int debug = 0);
	auto getPipeline() const { return m_pipeline; }
	auto getHandles	() const { return m_handles; }
	auto getCounts	() const { return m_counts; }
	auto getGroup	() const { return m_group; }
private:
	ZPipeline m_pipeline;
	std::vector<std::byte> m_handles;
	RTShaderCollection::SBTShaderGroup m_group;
	rtdetails::ShaderCounts m_counts;
};

template<
	class RayGenData = void,
	class MissData = void,
	class HitData = void,
	class CallableData = void,
	size_t rgDataSize = rtdetails::RegionDataSize<RayGenData>,
	size_t msDataSize = rtdetails::RegionDataSize<MissData>,
	size_t htDataSize = rtdetails::RegionDataSize<HitData>,
	size_t clDataSize = rtdetails::RegionDataSize<CallableData>
>
struct SBT : protected SBTAny
{
	struct EmptyData {};
	template<class X> using ed = std::conditional_t<std::is_void_v<X>, EmptyData, X>;

	explicit SBT (add_cref<SBTHandles> handles) noexcept
		: SBTAny(handles, rgDataSize, msDataSize, htDataSize, clDataSize) {}
	using SBTAny::buildOnce1;
	using SBTAny::buildOnce;
	using SBTAny::traceRays;
	using SBTAny::getBuffers;

	void setRayGenData	(add_cref<ed<RayGenData>>	data) { setRegionData(0u, &data); }
	void setMissData	(add_cref<ed<MissData>>		data) { setRegionData(1u, &data); }
	void setHitData		(add_cref<ed<HitData>>		data) { setRegionData(2u, &data); }
	void setCallableData(add_cref<ed<CallableData>>	data) { setRegionData(3u, &data); }

	add_cref<VkStridedDeviceAddressRegionKHR> raygenRegion  () const { return m_rgenRegion; }
	add_cref<VkStridedDeviceAddressRegionKHR> missRegion    () const { return m_missRegion; }
	add_cref<VkStridedDeviceAddressRegionKHR> hitRegion     () const { return m_hitRegion; }
	add_cref<VkStridedDeviceAddressRegionKHR> callableRegion() const { return m_callRegion; }
};
template<class P, class Q, class R, class S>
inline void commandBufferTraceRays (
	ZCommandBuffer cmd,
	SBT<P, Q, R, S> & sbt,
	uint32_t width, uint32_t height, uint32_t depth = 1u)
{
	sbt.traceRays(cmd, width, height, depth);
}

} // namespace vtf

#endif // __VTF_RTTYPES_HPP_INCLUDED__
