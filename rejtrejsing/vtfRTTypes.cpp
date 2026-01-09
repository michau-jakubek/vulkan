#include "vtfRTTypes.hpp"
#include "vtfCUtils.hpp"
#include "vtfZBuffer.hpp"
#include "vtfZBarriers2.hpp"
#include "vtfZCommandBuffer.hpp"
#include "vtfTemplateUtils.hpp"
#include "vtfRTPipeline.hpp"
#include "vtfBacktrace.hpp"
#include "vulkan/vulkan_to_string.hpp"

#include <map>
#include <numeric>

namespace vtf
{

void freeAccelerationStructure(ZDevice dev, VkAccelerationStructureKHR str, VkAllocationCallbacksPtr cbs)
{
	add_cref<ZDeviceInterface> di = dev.getInterface();
	di.vkDestroyAccelerationStructureKHR(*dev, str, cbs);
}

void onEnablingRayTracingFeatures(add_ref<DeviceCaps> caps)
{
	caps.addUpdateFeatureIf(&VkPhysicalDeviceRayTracingPipelineFeaturesKHR::rayTracingPipeline)
		.checkSupported("rayTracingPipeline");
	caps.addExtension(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME).checkSupported();

	caps.addUpdateFeatureIf(&VkPhysicalDeviceAccelerationStructureFeaturesKHR::accelerationStructure)
		.checkSupported("accelerationStructure");
	caps.addExtension(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME).checkSupported();

	caps.addExtension(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME).checkSupported();
	caps.addExtension(VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME).checkSupported();
	caps.addExtension(VK_KHR_SPIRV_1_4_EXTENSION_NAME).checkSupported();
	caps.addExtension(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME).checkSupported();

	caps.addUpdateFeatureIf(&VkPhysicalDeviceBufferDeviceAddressFeaturesEXT::bufferDeviceAddress)
		.checkSupported("bufferDeviceAddress");
	caps.addExtension(VK_EXT_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME).checkSupported();

	caps.addUpdateFeatureIf(&VkPhysicalDeviceSynchronization2FeaturesKHR::synchronization2)
		.checkSupported("synchronization2");
	caps.addExtension(VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME).checkSupported();

	caps.addUpdateFeature<VkPhysicalDeviceDescriptorIndexingFeatures>();
	
	/*
	caps.addUpdateFeatureIf(&VkPhysicalDeviceShaderFloatControls2Features::shaderFloatControls2);

	caps.addUpdateFeatureIf(&VkPhysicalDeviceVulkan12Features::bufferDeviceAddress)
		.checkSupported("bufferDeviceAddress");
	caps.addUpdateFeatureIf(&VkPhysicalDeviceVulkan12Features::shaderFloa
	vk12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
	vk12.bufferDeviceAddress = VK_TRUE;
	vk12.shaderFloatControls = VK_TRUE;
	*/
}

ZAccelerationStructureGeometry::ZAccelerationStructureGeometry()
	: VkAccelerationStructureGeometryKHR
	{
		VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
		nullptr,
		VK_GEOMETRY_TYPE_MAX_ENUM_KHR,	// geometryType
		{ // VkAccelerationStructureGeometryTrianglesDataKHR    triangles;
		  // VkAccelerationStructureGeometryAabbsDataKHR        aabbs;
		  // VkAccelerationStructureGeometryInstancesDataKHR    instances;
		}, // VkAccelerationStructureGeometryDataKHR geometry
		VK_GEOMETRY_OPAQUE_BIT_KHR
	}
	, m_buffers()
	, m_primitiveCount()
{
}

ZAccelerationStructureGeometry::ZAccelerationStructureGeometry(ZDevice device,
	add_cref<std::vector<Vec3>> vertices, add_cref<std::vector<uint32_t>> indices)
	: ZAccelerationStructureGeometry()
{
	const uint32_t vertexCount = data_count(vertices);
	ASSERTMSG(vertexCount && ((vertexCount % 3u) == 0u), "Vertex count must be a multiplication of 3");

	ZBuffer vertexBuffer = createBuffer<Vec3>(device, vertexCount,
		ZBufferUsageFlags(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
							VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
							VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR));
	bufferWrite(vertexBuffer, vertices);

	this->geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
	add_ref<VkAccelerationStructureGeometryTrianglesDataKHR> triangles = geometry.triangles;
	triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
	triangles.indexType = VK_INDEX_TYPE_NONE_KHR;
	triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
	triangles.vertexData.deviceAddress = bufferGetAddress(vertexBuffer);
	triangles.vertexStride = sizeof(Vec3);
	triangles.maxVertex = vertexCount - 1u;
	m_buffers.emplace_back(vertexBuffer);
	m_primitiveCount = vertexCount / 3u;

	if (const uint32_t indexCount = data_count(indices); indexCount)
	{
		uint32_t maxIndex = 0u;

		ASSERTMSG(((indexCount % 3u) == 0u), "Index count must be a multiplication of 3");
		for (uint32_t i = 0u; i < indexCount; ++i)
		{
			ASSERTMSG(indices[i] < vertexCount, "Index (", i, ") exceeds vertex count (", vertexCount, ")");
			maxIndex = std::max(indices[i], maxIndex);
		}

		ZBuffer indexBuffer = createBuffer<uint32_t>(device, indexCount,
			ZBufferUsageFlags(VK_BUFFER_USAGE_INDEX_BUFFER_BIT,	VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
								VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
								VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR));
		bufferWrite(indexBuffer, indices);

		triangles.indexType = VK_INDEX_TYPE_UINT32;
		triangles.indexData.deviceAddress = bufferGetAddress(indexBuffer);
		triangles.maxVertex = maxIndex;
		m_buffers.emplace_back(indexBuffer);
		m_primitiveCount = indexCount / 3u;
	}
}

ZAccelerationStructureGeometry makeTrianglesGeometry(
	ZDevice device,
	add_cref<std::vector<Vec3>> vertices,
	add_cref<std::vector<uint32_t>> indices)
{
	return ZAccelerationStructureGeometry(device, vertices, indices);
}

VkAccelerationStructureBuildGeometryInfoKHR
buildGeometryInfo(add_cref<std::vector<ZAccelerationStructureGeometry>> geoms,
					add_ref<std::vector<VkAccelerationStructureGeometryKHR>> aux)
{
	aux.resize(geoms.size());
	for (auto begin = geoms.begin(), g = begin; g != geoms.end(); ++g)
	{
		aux[make_unsigned(std::distance(begin, g))] = g->operator()();
	}

	VkAccelerationStructureBuildGeometryInfoKHR buildInfo{};
	buildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
	buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
	buildInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
	buildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
	buildInfo.geometryCount = data_count(geoms);
	buildInfo.pGeometries = aux.data();

	return buildInfo;
}

template<class AccStruct>
VkDeviceAddress accelerationStructureGetDeviceAddress(AccStruct str)
{
	ZDevice device = str.template getParam<ZDevice>();
	add_cref<ZDeviceInterface> di = device.getInterface();

	VkAccelerationStructureDeviceAddressInfoKHR addressInfo{};
	addressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
	addressInfo.accelerationStructure = *str;

	return di.vkGetAccelerationStructureDeviceAddressKHR(*device, &addressInfo);
}
template VkDeviceAddress accelerationStructureGetDeviceAddress<ZBtmAccelerationStructure>(ZBtmAccelerationStructure);
template VkDeviceAddress accelerationStructureGetDeviceAddress<ZTopAccelerationStructure>(ZTopAccelerationStructure);

ZBtmAccelerationStructure
createBtmAccelerationStructure(ZAccelerationStructureGeometry::Geoms geoms)
{
	ASSERTMSG(geoms.size(), "Geometries list must not be empty");

	ZDevice device = geoms[0]->getBuffers()[0].getParam<ZDevice>();
	add_cref<ZDeviceInterface> di = device.getInterface();
	const VkAllocationCallbacksPtr callbacks = device.getParam<VkAllocationCallbacksPtr>();

	ZBtmAccelerationStructure blas(VK_NULL_HANDLE, device, callbacks,
									{/*blasBuffer*/}, {/*scratchBuffer*/}, {/*geoms*/},
									0u, 0u, 0u, VK_FALSE);
	add_ref<std::vector<ZAccelerationStructureGeometry>> blasGeoms =
		blas.getParamRef<std::vector<ZAccelerationStructureGeometry>>();
	blasGeoms.resize(geoms.size());

	std::vector<uint32_t> primitiveCounts(geoms.size());
	//std::vector<VkAccelerationStructureGeometryKHR> gs(geoms.size());
	for (auto begin = geoms.begin(), g = begin; g != geoms.end(); ++g)
	{
		const auto idx = make_unsigned(std::distance(begin, g));
		primitiveCounts[idx] = (*g)->primitiveCount();
		blasGeoms[idx] = *(*g);
	}

	std::vector<VkAccelerationStructureGeometryKHR> auxGeoms;
	const VkAccelerationStructureBuildGeometryInfoKHR buildInfo = buildGeometryInfo(blasGeoms, auxGeoms);
	
	VkAccelerationStructureBuildSizesInfoKHR sizeInfo{};
	sizeInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;

	di.vkGetAccelerationStructureBuildSizesKHR(
		*device,
		VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
		&buildInfo,
		primitiveCounts.data(),
		&sizeInfo
	);

	blas.setParam<ZDistType<SizeFirst, VkDeviceSize>>(sizeInfo.accelerationStructureSize);
	blas.setParam<ZDistType<SizeSecond, VkDeviceSize>>(sizeInfo.updateScratchSize);
	blas.setParam<ZDistType<SizeThird, VkDeviceSize>>(sizeInfo.buildScratchSize);

	const ZBufferUsageFlags blasUsage(VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR,
										VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
	ZBuffer blasBuffer = createBuffer(device, sizeInfo.accelerationStructureSize, blasUsage, ZMemoryPropertyDeviceFlags);
	blas.setParam<ZDistType<SomeZero, ZBuffer>>(blasBuffer);

	VkAccelerationStructureCreateInfoKHR createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
	createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
	createInfo.size = sizeInfo.accelerationStructureSize;
	createInfo.buffer = *blasBuffer;

	VKASSERT(di.vkCreateAccelerationStructureKHR(*device, &createInfo, callbacks, blas.setter()));

	return blas;
}

VkTransformMatrixKHR getTransformMatrix()
{
	const VkTransformMatrixKHR identityMatrix{
	{
		{ 1.0f, 0.0f, 0.0f, 0.0f },
		{ 0.0f, 1.0f, 0.0f, 0.0f },
		{ 0.0f, 0.0f, 1.0f, 0.0f }
	} };
	return identityMatrix;
}

BLAS::BLAS(
	ZBtmAccelerationStructure str,
	uint32_t sbtOffset,
	uint32_t instIndex,
	VkTransformMatrixKHR mtx,
	uint32_t amask,
	VkGeometryInstanceFlagsKHR aflags
)		: structure(str)
		, instanceShaderBindingTableRecordOffset(sbtOffset)
		, instanceCustomIndex(instIndex)
		, transform(mtx)
		, mask(amask)
		, flags(aflags)
{
}

[[maybe_unused]] static std::vector<ZBtmAccelerationStructure>
distinctBottomsByHandle(BLAS btm, std::optional<std::vector<BLAS>> otherBtms)
{
	const uint32_t btmCount = 1u + (otherBtms.has_value() ? data_count(*otherBtms) : 0u);
	std::vector<ZBtmAccelerationStructure> bottoms;
	bottoms.reserve(btmCount);
	std::map<VkAccelerationStructureKHR, ZBtmAccelerationStructure> map{};
	map[btm.structure.handle()] = btm.structure;
	if (btmCount > 1u)
	{
		for (add_cref<BLAS> b : *otherBtms)
			map[b.structure.handle()] = b.structure;
	}
	for (add_cref<std::pair<const VkAccelerationStructureKHR, ZBtmAccelerationStructure>> i : map)
		bottoms.emplace_back(i.second);
	bottoms.shrink_to_fit();
	return bottoms;
}

ZTopAccelerationStructure
createTopAccelerationStructure(ZDevice device, uint32_t maxInstances)
{
	add_cref<ZDeviceInterface> di = device.getInterface();
	const VkAllocationCallbacksPtr callbacks = device.getParam<VkAllocationCallbacksPtr>();

	ZBuffer instancesBuffer = createBuffer<VkAccelerationStructureInstanceKHR>(device, maxInstances,
										ZBufferUsageFlags(VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
											VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR),
										ZMemoryPropertyHostFlags);

	VkAccelerationStructureGeometryKHR tlasGeom{};
	tlasGeom.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
	tlasGeom.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
	tlasGeom.geometry.instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
	tlasGeom.geometry.instances.arrayOfPointers = VK_FALSE;
	tlasGeom.geometry.instances.data.deviceAddress = bufferGetAddress(instancesBuffer);

	VkAccelerationStructureBuildGeometryInfoKHR buildInfo{};
	buildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
	buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
	buildInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
	buildInfo.geometryCount = 1u;
	buildInfo.pGeometries = &tlasGeom;

	VkAccelerationStructureBuildSizesInfoKHR sizeInfo{};
	sizeInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;

	di.vkGetAccelerationStructureBuildSizesKHR(
		*device,
		VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
		&buildInfo,
		&maxInstances,
		&sizeInfo
	);

	ZBuffer scratchBuffer = createBuffer(device, sizeInfo.buildScratchSize,
		ZBufferUsageFlags(VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT));

	ZBuffer tlasBuffer = createBuffer(device, sizeInfo.accelerationStructureSize,
		ZBufferUsageFlags(VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
			VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR),
		ZMemoryPropertyDeviceFlags);

	VkAccelerationStructureCreateInfoKHR createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
	createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
	createInfo.size = sizeInfo.accelerationStructureSize;
	createInfo.buffer = *tlasBuffer;

	ZTopAccelerationStructure tlas(VK_NULL_HANDLE, device, callbacks,
		tlasBuffer, instancesBuffer, scratchBuffer,
		{}, sizeInfo.accelerationStructureSize, sizeInfo.updateScratchSize, sizeInfo.buildScratchSize, maxInstances);


	VKASSERT(di.vkCreateAccelerationStructureKHR(*device, &createInfo, callbacks, tlas.setter()));

	return tlas;
}

void commandBufferBuildAccelerationStructure(
	ZCommandBuffer cmd, ZBtmAccelerationStructure blas)
{
	if (blas.getParam<VkBool32>())
	{
		return;
	}

	ZDevice device = cmd.getParam<ZDevice>();
	add_cref<ZDeviceInterface> di = device.getInterface();
	//const VkAllocationCallbacksPtr callbacks = device.getParam<VkAllocationCallbacksPtr>();

	add_ref<std::vector<ZAccelerationStructureGeometry>> blasGeoms =
		blas.getParamRef<std::vector<ZAccelerationStructureGeometry>>();

	std::vector<VkAccelerationStructureGeometryKHR> aux;
	VkAccelerationStructureBuildGeometryInfoKHR buildInfo = buildGeometryInfo(blasGeoms, aux);

	std::vector<VkAccelerationStructureBuildRangeInfoKHR> rangeInfos(blasGeoms.size());
	std::vector<add_cptr<VkAccelerationStructureBuildRangeInfoKHR>> rangeInfosPtrs(blasGeoms.size());
	std::vector<uint32_t> primitiveCounts(blasGeoms.size());

	for (auto begin = blasGeoms.begin(), geom = begin; geom != blasGeoms.end(); ++geom)
	{
		const auto idx = make_unsigned(std::distance(begin, geom));
		primitiveCounts[idx] = geom->primitiveCount();

		rangeInfos[idx].firstVertex = 0u;
		rangeInfos[idx].primitiveCount = geom->primitiveCount();
		rangeInfos[idx].primitiveOffset = 0u;
		rangeInfos[idx].transformOffset = 0u;
		rangeInfosPtrs[idx] = &rangeInfos[idx];
	}

	const VkDeviceSize buildScratchSize = blas.getParam<ZDistType<SizeThird, VkDeviceSize>>().get();
	ZBuffer scratchBuffer = createBuffer(device, buildScratchSize,
							ZBufferUsageFlags(VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
												VK_BUFFER_USAGE_STORAGE_BUFFER_BIT));
	blas.setParam<ZDistType<SomeTwo, ZBuffer>>(scratchBuffer);
	buildInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
	buildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
	buildInfo.dstAccelerationStructure = *blas;
	buildInfo.scratchData.deviceAddress = bufferGetAddress(scratchBuffer);

	di.vkCmdBuildAccelerationStructuresKHR(*cmd, 1u, &buildInfo, rangeInfosPtrs.data());

	using A = ZBarrierConstants::Access;
	using S = ZBarrierConstants::Stage;
	ZMemoryBarrier2 barrier(
		A::ACCELERATION_STRUCTURE_WRITE_BIT_KHR,
		S::ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
		A::ACCELERATION_STRUCTURE_READ_BIT_KHR,
		S::ACCELERATION_STRUCTURE_BUILD_BIT_KHR);
	commandBufferPipelineBarriers2(cmd, barrier);

	blas.setParam<VkBool32>(VK_TRUE); // already built
}

void commandBufferBuildAccelerationStructure(
	ZCommandBuffer cmd,	ZTopAccelerationStructure tlas,
	BLAS firstInstance,	std::optional<std::vector<BLAS>> otherInstances)
{
	const uint32_t declaredInstanceCount = tlas.getParam<uint32_t>();
	const uint32_t instanceCount = 1u + (otherInstances.has_value() ? data_count(*otherInstances) : 0u);
	ASSERTMSG(instanceCount <= declaredInstanceCount, "Build instance count (", instanceCount,
		") exceeds declared TLAS instance count (", declaredInstanceCount, ")");

	ZDevice device = cmd.getParam<ZDevice>();
	add_cref<ZDeviceInterface> di = device.getInterface();
	ZBuffer instancesBuffer = tlas.getParam<ZDistType<SomeOne, ZBuffer>>();
	ZBuffer scratchBuffer = tlas.getParam<ZDistType<SomeTwo, ZBuffer>>();

	for (uint32_t b = 0u; b < instanceCount; ++b)
	{
		add_ref<BLAS> blas = (0u == b) ? firstInstance : (*otherInstances)[b - 1u];
		commandBufferBuildAccelerationStructure(cmd, blas.structure);
	}

	for (uint32_t b = 0u; b < instanceCount; ++b)
	{
		add_ref<BLAS> blas = (0u == b) ? firstInstance : (*otherInstances)[b - 1u];

		VkAccelerationStructureInstanceKHR i{};
		i.mask = blas.mask;
		i.flags = blas.flags;
		i.transform = blas.transform;
		i.instanceCustomIndex = blas.instanceCustomIndex;
		i.instanceShaderBindingTableRecordOffset = blas.instanceShaderBindingTableRecordOffset;
		i.accelerationStructureReference = accelerationStructureGetDeviceAddress(blas.structure);
		bufferWrite(instancesBuffer, i, b);
	}

	VkAccelerationStructureGeometryKHR geom{};
	geom.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
	geom.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
	geom.geometry.instances.sType =
		VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
	geom.geometry.instances.arrayOfPointers = VK_FALSE;
	geom.geometry.instances.data.deviceAddress = bufferGetAddress(instancesBuffer);

	VkAccelerationStructureBuildGeometryInfoKHR buildInfo{};
	buildInfo.sType =
		VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
	buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
	buildInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
	buildInfo.geometryCount = 1u;
	buildInfo.pGeometries = &geom;
	buildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
	buildInfo.dstAccelerationStructure = *tlas;
	buildInfo.scratchData.deviceAddress = bufferGetAddress(scratchBuffer);

	VkAccelerationStructureBuildRangeInfoKHR range{};
	range.primitiveCount = instanceCount;

	using A = ZBarrierConstants::Access;
	using S = ZBarrierConstants::Stage;

	ZBufferMemoryBarrier2 bufferBarrier(instancesBuffer,
		A::HOST_WRITE_BIT,
		S::HOST_BIT,
		A::ACCELERATION_STRUCTURE_READ_BIT_KHR,
		S::ACCELERATION_STRUCTURE_BUILD_BIT_KHR);
	commandBufferPipelineBarriers2(cmd, bufferBarrier);

	const VkAccelerationStructureBuildRangeInfoKHR* pRanges[] = { &range };
	di.vkCmdBuildAccelerationStructuresKHR(*cmd, 1u, &buildInfo, pRanges);

	ZMemoryBarrier2 buildBarrier(
		A::ACCELERATION_STRUCTURE_WRITE_BIT_KHR,
		S::ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
		A::ACCELERATION_STRUCTURE_READ_BIT_KHR,
		S::RAY_TRACING_SHADER_BIT_KHR);
	commandBufferPipelineBarriers2(cmd, buildBarrier);
}

#define COLLECTION_INDEX		0
// combined hi=hitGroupIndex & low=logicalGroupIndex
#define SBT_SHADER_GROUP_INDEX	1
#define PIPELINE_GROUP_INDEX	2
#define PIPELINE_SHADER_INDEX	3
using tuple4 = std::tuple<uint32_t, uint32_t, uint32_t, uint32_t>;
using tuple3 = std::tuple<uint32_t, uint32_t, uint32_t>;
using SBTShaderGroup = RTShaderCollection::SBTShaderGroup;

extern SBTShaderGroup combineGroups(
	add_cref<SBTShaderGroup> logicalGroup,
	add_cref<SBTShaderGroup> hitGroup);

extern std::pair<SBTShaderGroup, SBTShaderGroup>
decombineGroup(add_cref<SBTShaderGroup> combinedGroup);

extern void advance(add_ref<uint32_t> val, uint32_t n = 1u);
extern void retreat(add_ref<uint32_t> val, uint32_t n = 1u);

extern rtdetails::ShaderCounts countShaders (span::span<const ZShaderModule> shaders);
extern uint32_t shaderGetLogicalGroup (add_cref<ZShaderModule> module);
extern uint32_t shaderGetHitGroup (add_cref<ZShaderModule> module);
extern uint32_t shaderGetPipelineFirstGroup (add_cref<ZShaderModule> module);
extern std::pair<uint32_t, uint32_t> shaderGetPipelineGroup (add_cref<ZShaderModule> module);

SBTHandles::SBTHandles (ZPipeline pipeline, add_cref<RTShaderCollection::SBTShaderGroup> group, int debug)
	: m_pipeline(pipeline)
	, m_handles()
	, m_group(group)
	, m_counts()
{
	ZDevice device = pipeline.getParam<ZDevice>();
	add_cref<ZDeviceInterface> di = device.getInterface();

	const VkPhysicalDeviceRayTracingPipelinePropertiesKHR props = [&]() {
		VkPhysicalDeviceRayTracingPipelinePropertiesKHR p{};
		p.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;
		deviceGetPhysicalProperties2(device.getParam<ZPhysicalDevice>(), &p);
		return p;
		}();

	uint32_t firstGroup = INVALID_UINT32;
	std::tie(firstGroup, m_counts) = pipelineGetGroupsInfo(pipeline, group);

	const uint32_t batchGroupCount = m_counts.batchCount();
	m_handles.resize(batchGroupCount * (ROUNDUP(props.shaderGroupHandleSize, props.shaderGroupHandleAlignment)));
	if (debug)
	{
		ASSERTION(props.shaderGroupHandleSize % sizeof(uint32_t) == 0);
		auto ints = makeStdBeginEnd<int>(m_handles.data(), m_handles.size() / sizeof(uint32_t));
		std::iota(ints.first, ints.second, uint32_t(debug));
	}
	else
	{
		VKASSERT(di.vkGetRayTracingShaderGroupHandlesKHR(
			*device,
			*pipeline,
			firstGroup,
			batchGroupCount,
			m_handles.size(),
			m_handles.data()
		));
	}
}

void SBTAny::setRegionData (uint32_t region, void_cptr data)
{
	const std::pair<std::size_t, std::size_t> dataOffsetsAndSizes[]{
		{ 0u, m_rgenDataSize },
		{ m_rgenDataSize, m_missDataSize },
		{ m_missDataSize, m_hitDataSize },
		{ m_hitDataSize, m_callDataSize }
	};
	std::copy_n(static_cast<add_cptr<std::byte>>(data), dataOffsetsAndSizes[region].second,
		std::next(m_data.begin(), std::vector<std::byte>::difference_type(dataOffsetsAndSizes[region].first)));
}

bool SBTAny::buildOnce (ZCommandBuffer cmd, int method)
{
	ASSERTION(method == 0 || method == 1);
	const bool res = method ? buildOnce1() : buildOnce();
	ZBufferMemoryBarrier barrier(m_sbtBuffer, VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT);
	commandBufferPipelineBarriers(cmd, VK_PIPELINE_STAGE_HOST_BIT,
										VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, barrier);
	return res;
}

bool SBTAny::buildOnce ()
{
	if (m_sbtBuffer.has_handle()) {
		return false;
	}

	ZDevice device = m_handles.getPipeline().getParam<ZDevice>();

	const VkPhysicalDeviceRayTracingPipelinePropertiesKHR props = [&]() {
		VkPhysicalDeviceRayTracingPipelinePropertiesKHR p{};
		p.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;
		deviceGetPhysicalProperties2(device.getParam<ZPhysicalDevice>(), &p);
		return p;
		}();

	const uint32_t		handleSize		= props.shaderGroupHandleSize;
	const uint32_t		alignHandleSize	= ROUNDUP(props.shaderGroupHandleSize, props.shaderGroupHandleAlignment);
	const uint32_t		baseAlignment	= props.shaderGroupBaseAlignment;
	const uint32_t		rgenDataSize	= uint32_t(m_rgenDataSize);
	const uint32_t		missDataSize	= uint32_t(m_missDataSize);
	const uint32_t		hitDataSize		= uint32_t(m_hitDataSize);
	const uint32_t		callDataSize	= uint32_t(m_callDataSize);
	const uint32_t		userDataSize	= std::max({ rgenDataSize, missDataSize,
													hitDataSize, callDataSize }) * 0;
	const uint32_t		recordSize = ROUNDUP(alignHandleSize + userDataSize, baseAlignment);

	const VkDeviceSize	sbtSize = m_handles.getCounts().batchCount() * recordSize;
	m_sbtBuffer = createBuffer(
		device,
		sbtSize,
		ZBufferUsageFlags(
			VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
			VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR,
			VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
			VK_BUFFER_USAGE_TRANSFER_DST_BIT
		),
		ZMemoryPropertyHostFlags
	);
	const VkDeviceAddress sbtAddr = bufferGetAddress(m_sbtBuffer);

	const auto order = pipelineGetOrder(m_handles.getPipeline());
	std::vector<std::byte> testHandle(alignHandleSize);
	add_cref<Data> handles(m_handles.getHandles());

	const auto indexToStage = [](const uint32_t index, const uint32_t = 0u) {
		switch (index)
		{
		case 0: return VK_SHADER_STAGE_RAYGEN_BIT_KHR;
		case 1: return VK_SHADER_STAGE_MISS_BIT_KHR;
		case 2: return VK_SHADER_STAGE_CALLABLE_BIT_KHR;
		case 3: return VK_SHADER_STAGE_ANY_HIT_BIT_KHR;
		default:
			ASSERTFALSE("Index of bounds: ", index);
			return VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM;
		}
	};
	using NewOrder = std::array<uint32_t, STED_ARRAY_SIZE(order.getOrder())>;
	const NewOrder newOrder = [&] {
		NewOrder tmp{ 0, 1, 2, 3 };
		order.apply(std::begin(tmp), std::end(tmp), indexToStage,
			rtdetails::PipelineShaderGroupOrder::HitShadersOrder(), false);
		return tmp;
		}();
	add_ptr<VkStridedDeviceAddressRegionKHR> const regions[STED_ARRAY_SIZE(newOrder)]
	{
		&m_rgenRegion,
		&m_missRegion,
		&m_callRegion,
		&m_hitRegion
	};
	uint32_t const handleInfos[STED_ARRAY_SIZE(newOrder)]
	{
		m_handles.getCounts().rgCount,
		m_handles.getCounts().missCount,
		m_handles.getCounts().callCount,
		m_handles.getCounts().hitCount()
	};

	//const std::pair<uint32_t, uint32_t> dataOffsetsAndSizes[]{
	//	{ 0u, rgenDataSize },
	//	{ rgenDataSize, missDataSize },
	//	{ missDataSize, callDataSize },
	//	{ callDataSize, hitDataSize }
	//};

	for (uint32_t i = 0u; i < ARRAY_LENGTH(handleInfos); ++i)
	{
		const auto currentStage = indexToStage(newOrder[i]);
		UNREF(currentStage);

		uint32_t handleIndex = 0u;
		for (uint32_t j = 0u; j < i; ++j)
			handleIndex += handleInfos[newOrder[j]];

		const uint32_t handleCount = handleInfos[newOrder[i]];
		if (handleCount == 0u) {
			regions[newOrder[i]]->deviceAddress = 0u;
			regions[newOrder[i]]->stride = 0u;
			regions[newOrder[i]]->size = 0u;
			continue;
		}

		{
			std::copy_n(handles.begin() + handleIndex * handleSize, handleSize, testHandle.begin());
			bool handleEmpty = std::all_of(testHandle.begin(), testHandle.end(),
				[](add_cref<std::byte> b) { return b == std::byte(0); });
			UNREF(handleEmpty);
		}

		// Agnostic Buffer Copy? :)
		for (uint32_t k = 0; k < handleCount; ++k) {
			const uint32_t currentHandle = handleIndex + k;
			bufferWrite(
				m_sbtBuffer,		// destination
				handles,					// source
				currentHandle * recordSize,	// dstIndex
				currentHandle * handleSize,	// srcIndex
				handleSize	// count
			);
		}

		//bufferWrite(
		//	m_impl->m_sbtBuffer,
		//	m_impl->m_userData,
		//	handleIndex* recordSize + handleSize,
		//	dataOffsetsAndSizes[handleIndex].first,
		//	dataOffsetsAndSizes[handleIndex].second
		//);

		regions[newOrder[i]]->deviceAddress = sbtAddr + handleIndex * recordSize;
		regions[newOrder[i]]->size = handleCount * recordSize;
		regions[newOrder[i]]->stride = recordSize;
	}

	return (false == m_sbtBuffer.has_handle());
}

bool SBTAny::buildOnce1 ()
{
	if (m_sbtBuffer.has_handle()) {
		return false;
	}

	ZDevice device = m_handles.getPipeline().getParam<ZDevice>();

	const VkPhysicalDeviceRayTracingPipelinePropertiesKHR props = [&]() {
		VkPhysicalDeviceRayTracingPipelinePropertiesKHR p{};
		p.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;
		deviceGetPhysicalProperties2(device.getParam<ZPhysicalDevice>(), &p);
		return p;
		}();

	const uint32_t		rgenDataSize = uint32_t(m_rgenDataSize);
	const uint32_t		missDataSize = uint32_t(m_missDataSize);
	const uint32_t		hitDataSize = uint32_t(m_hitDataSize);
	const uint32_t		callDataSize = uint32_t(m_callDataSize);
	const uint32_t		handleSize = props.shaderGroupHandleSize;
	const uint32_t		baseAlignment = props.shaderGroupBaseAlignment;
	const uint32_t		userDataSize = std::max({ rgenDataSize, missDataSize,
													hitDataSize, callDataSize }) * 0;
	const uint32_t		recordSize = ROUNDUP(handleSize + userDataSize, baseAlignment);

	//const VkDeviceSize	sbtSize = m_handles.getCounts().batchCount() * recordSize;
	const VkDeviceSize rgenSbtSize = m_handles.getCounts().rgCount * recordSize;
	const VkDeviceSize missSbtSize = m_handles.getCounts().missCount * recordSize;
	const VkDeviceSize callSbtSize = m_handles.getCounts().callCount * recordSize;
	const VkDeviceSize hitSbtSize = m_handles.getCounts().hitCount() * recordSize;
	const VkDeviceSize sbtSizes[]{ rgenSbtSize, missSbtSize, callSbtSize, hitSbtSize };
	add_ptr<ZBuffer> sbtBuffers[]{ &m_sbtBuffer, &m_sbtMissBuffer, &m_sbtCallBuffer, &m_sbtHitBuffer };
	VkDeviceAddress sbtAddresses[4];
	for (uint32_t i = 0u; i < 4; ++i)
	{
		if (const auto size = sbtSizes[i]; size != 0u)
		{
			*sbtBuffers[i] = createBuffer(
				device,
				size,
				ZBufferUsageFlags(
					VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
					VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR,
					VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
					VK_BUFFER_USAGE_TRANSFER_DST_BIT
				),
				ZMemoryPropertyHostFlags
			);
			sbtAddresses[i] = bufferGetAddress(*sbtBuffers[i]);
		}
		else
		{
			sbtAddresses[i] = 0;
		}
	}

	const auto order = pipelineGetOrder(m_handles.getPipeline());
	std::vector<std::byte> testHandle(handleSize);
	add_cref<Data> handles(m_handles.getHandles());

	const auto indexToStage = [](const uint32_t index, const uint32_t = 0u) {
		switch (index)
		{
		case 0: return VK_SHADER_STAGE_RAYGEN_BIT_KHR;
		case 1: return VK_SHADER_STAGE_MISS_BIT_KHR;
		case 2: return VK_SHADER_STAGE_CALLABLE_BIT_KHR;
		case 3: return VK_SHADER_STAGE_ANY_HIT_BIT_KHR;
		default:
			ASSERTFALSE("Index of bounds: ", index);
			return VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM;
		}
		};
	using NewOrder = std::array<uint32_t, STED_ARRAY_SIZE(order.getOrder())>;
	const NewOrder newOrder = [&] {
		NewOrder tmp{ 0, 1, 2, 3 };
		order.apply(std::begin(tmp), std::end(tmp), indexToStage,
			rtdetails::PipelineShaderGroupOrder::HitShadersOrder(), false);
		return tmp;
		}();
	add_ptr<VkStridedDeviceAddressRegionKHR> const regions[STED_ARRAY_SIZE(newOrder)]
	{
		&m_rgenRegion,
		&m_missRegion,
		&m_callRegion,
		&m_hitRegion
	};
	uint32_t const handleInfos[STED_ARRAY_SIZE(newOrder)]
	{
		m_handles.getCounts().rgCount,
		m_handles.getCounts().missCount,
		m_handles.getCounts().callCount,
		m_handles.getCounts().hitCount()
	};

	//const std::pair<uint32_t, uint32_t> dataOffsetsAndSizes[]{
	//	{ 0u, rgenDataSize },
	//	{ rgenDataSize, missDataSize },
	//	{ missDataSize, callDataSize },
	//	{ callDataSize, hitDataSize }
	//};
	for (uint32_t i = 0u; i < ARRAY_LENGTH(handleInfos); ++i)
	{
		const auto currentStage = indexToStage(newOrder[i]);
		UNREF(currentStage);

		uint32_t handleIndex = 0u;
		for (uint32_t j = 0u; j < i; ++j)
			handleIndex += handleInfos[newOrder[j]];

		const uint32_t handleCount = handleInfos[newOrder[i]];
		if (handleCount == 0u) {
			continue;
		}

		ZBuffer sbtBuffer = *sbtBuffers[newOrder[i]];

		{
			std::copy_n(handles.begin() + handleIndex * handleSize, handleSize, testHandle.begin());
			bool handleEmpty = std::all_of(testHandle.begin(), testHandle.end(),
				[](add_cref<std::byte> b) { return b == std::byte(0); });
			UNREF(handleEmpty);
		}

		// Agnostic Buffer Copy? :)
		for (uint32_t k = 0; k < handleCount; ++k) {
			const uint32_t currentHandle = handleIndex + k;
			bufferWrite(
				sbtBuffer,		// destination
				handles,					// source
				k * recordSize,	// dstIndex
				currentHandle * handleSize,	// srcIndex
				handleSize	// count
			);
		}

		//bufferWrite(
		//	m_impl->m_sbtBuffer,
		//	m_impl->m_userData,
		//	handleIndex* recordSize + handleSize,
		//	dataOffsetsAndSizes[handleIndex].first,
		//	dataOffsetsAndSizes[handleIndex].second
		//);

		regions[newOrder[i]]->deviceAddress = sbtAddresses[newOrder[i]];
		regions[newOrder[i]]->size = handleCount * recordSize;
		regions[newOrder[i]]->stride = recordSize;
	}

	return (false == m_sbtBuffer.has_handle());
}

std::array<ZBuffer, sted_array_size<rtdetails::PipelineShaderGroupOrder::Order>>
SBTAny::getBuffers (add_cref<rtdetails::PipelineShaderGroupOrder> order) const
{
	rtdetails::PipelineShaderGroupOrder::Order stages{
		VK_SHADER_STAGE_RAYGEN_BIT_KHR,
		VK_SHADER_STAGE_MISS_BIT_KHR,
		VK_SHADER_STAGE_CALLABLE_BIT_KHR,
		VK_SHADER_STAGE_ANY_HIT_BIT_KHR
	};
	std::array<ZBuffer, STED_ARRAY_SIZE(stages)> buffers
		{ m_sbtBuffer, m_sbtMissBuffer,	m_sbtCallBuffer, m_sbtHitBuffer };
	rtdetails::PipelineShaderGroupOrder bufferOrder(stages);
	order.apply(buffers.begin(), buffers.end(), bufferOrder);
	return buffers;
}

void SBTAny::traceRays(
	ZCommandBuffer cmd,
	uint32_t width, uint32_t height, uint32_t depth)
{
	ZDevice device = cmd.getParam<ZDevice>();
	add_cref<ZDeviceInterface> di = device.getInterface();

	buildOnce(cmd);

	if (getGlobalAppFlags().verbose) {
		auto writeAddress = [](VkDeviceAddress a, VkDeviceAddress b = 0) -> std::string {
			if (a == 0 && b != 0)
				std::cout << "<unavailable>";
			else
				std::cout << "0x" << std::hex << a << std::dec << " (" << a << ")";
			return std::string();
		};
		const VkDeviceAddress addr = bufferGetAddress(m_sbtBuffer);
		std::cout << "[APP] call " << __func__ << "(\n"
			<< "    cmd = " << cmd << ",\n"
			<< "    raygenRegion: {" << std::endl
			<< "       .deviceAddress = " << writeAddress(m_rgenRegion.deviceAddress) << std::endl
			<< "       .size          = " << writeAddress(m_rgenRegion.size) << std::endl
			<< "       .stride        = " << writeAddress(m_rgenRegion.stride) << std::endl
			<< "       offset         = " << writeAddress(m_rgenRegion.deviceAddress, addr) << std::endl
			<< "    }," << std::endl
			<< "    missRegion: {" << std::endl
			<< "       .deviceAddress = " << writeAddress(m_missRegion.deviceAddress) << std::endl
			<< "       .size          = " << writeAddress(m_missRegion.size) << std::endl
			<< "       .stride        = " << writeAddress(m_missRegion.stride) << std::endl
			<< "       offset         = " << writeAddress(m_missRegion.deviceAddress, addr) << std::endl
			<< "    }," << std::endl
			<< "    hitRegion: {" << std::endl
			<< "       .deviceAddress = " << writeAddress(m_hitRegion.deviceAddress) << std::endl
			<< "       .size          = " << writeAddress(m_hitRegion.size) << std::endl
			<< "       .stride        = " << writeAddress(m_hitRegion.stride) << std::endl
			<< "       offset         = " << writeAddress(m_hitRegion.deviceAddress, addr) << std::endl
			<< "    }," << std::endl
			<< "    callableRegion: {" << std::endl
			<< "       .deviceAddress = " << writeAddress(m_callRegion.deviceAddress) << std::endl
			<< "       .size          = " << writeAddress(m_callRegion.size) << std::endl
			<< "       .stride        = " << writeAddress(m_callRegion.stride) << std::endl
			<< "       offset         = " << writeAddress(m_callRegion.deviceAddress, addr) << std::endl
			<< "    }," << std::endl
			<< "    width = " << width << ", height = " << height << ", depth = " << depth << ");\n";
		std::cout << "  SBT address = " << writeAddress(addr) << std::endl;
	}

	di.vkCmdTraceRaysKHR(*cmd,
		&m_rgenRegion, &m_missRegion, &m_hitRegion, &m_callRegion,
		width, height, depth);
}

} // namespace vtf

