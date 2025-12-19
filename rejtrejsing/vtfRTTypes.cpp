#include "vtfRTTypes.hpp"
#include "vtfCUtils.hpp"
#include "vtfZBuffer.hpp"
#include "vtfZBarriers2.hpp"
#include "vtfZCommandBuffer.hpp"

namespace vtf
{

void freeAccelerationStructure(ZDevice dev, VkAccelerationStructureKHR str, VkAllocationCallbacksPtr cbs)
{
	add_cref<ZDeviceInterface> di = dev.getInterface();
	di.vkDestroyAccelerationStructureKHR(*dev, str, cbs);
}

void onEnablingRTFeatures(add_ref<DeviceCaps> caps)
{
	caps.addUpdateFeatureIf(&VkPhysicalDeviceAccelerationStructureFeaturesKHR::accelerationStructure)
		.checkSupported("accelerationStructure");
	caps.addExtension(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME).checkSupported();

	caps.addUpdateFeatureIf(&VkPhysicalDeviceRayTracingPipelineFeaturesKHR::rayTracingPipeline)
		.checkSupported("rayTracingPipeline");
	caps.addExtension(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME).checkSupported();

	caps.addExtension(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME).checkSupported();
	caps.addExtension(VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME).checkSupported();
	caps.addExtension(VK_KHR_SPIRV_1_4_EXTENSION_NAME).checkSupported();

	caps.addUpdateFeatureIf(&VkPhysicalDeviceBufferDeviceAddressFeaturesEXT::bufferDeviceAddress)
		.checkSupported("bufferDeviceAddress");
	caps.addExtension(VK_EXT_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME).checkSupported();

	caps.addUpdateFeatureIf(&VkPhysicalDeviceSynchronization2FeaturesKHR::synchronization2)
		.checkSupported("synchronization2");
	caps.addExtension(VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME).checkSupported();
	
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
		VK_GEOMETRY_TYPE_MAX_ENUM_KHR,
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
							VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR));
	bufferWrite(vertexBuffer, vertices);

	this->geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
	add_ref<VkAccelerationStructureGeometryTrianglesDataKHR> triangles = geometry.triangles;
	triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
	triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
	triangles.vertexData.deviceAddress = bufferGetAddress(vertexBuffer);
	triangles.vertexStride = sizeof(Vec3);
	triangles.maxVertex = data_count(vertices);
	m_buffers.emplace_back(vertexBuffer);
	m_primitiveCount = vertexCount / 3u;

	if (const uint32_t indexCount = data_count(indices); indexCount)
	{
		ASSERTMSG(((indexCount % 3u) == 0u), "Index count must be a multiplication of 3");
		for (uint32_t i = 0u; i < indexCount; ++i)
		{
			ASSERTMSG(indices[i] < vertexCount, "Index (", i, ") exceeds vertex count (", vertexCount, ")");
		}

		ZBuffer indexBuffer = createBuffer<uint32_t>(device, data_count(indices),
			ZBufferUsageFlags(VK_BUFFER_USAGE_INDEX_BUFFER_BIT,	VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
								VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR));
		bufferWrite(indexBuffer, indices);

		triangles.indexType = VK_INDEX_TYPE_UINT32;
		triangles.indexData.deviceAddress = bufferGetAddress(indexBuffer);
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

ZBtmAccelerationStructure
createBtmAccelerationStructure(ZAccelerationStructureGeometry::Geoms geoms)
{
	ASSERTMSG(geoms.size(), "Geometries list must not be empty");

	ZDevice device = geoms[0]->getBuffers()[0].getParam<ZDevice>();
	add_cref<ZDeviceInterface> di = device.getInterface();
	const VkAllocationCallbacksPtr callbacks = device.getParam<VkAllocationCallbacksPtr>();

	ZBtmAccelerationStructure str(VK_NULL_HANDLE, device, callbacks, {/*blasBuffer*/}, {/*geoms*/}, VK_FALSE);
	add_ref<std::vector<ZAccelerationStructureGeometry>> strGeoms =
		str.getParamRef<std::vector<ZAccelerationStructureGeometry>>();
	strGeoms.resize(geoms.size());

	std::vector<uint32_t> primitiveCounts(geoms.size());
	//std::vector<VkAccelerationStructureGeometryKHR> gs(geoms.size());
	for (auto begin = geoms.begin(), g = begin; g != geoms.end(); ++g)
	{
		const auto idx = make_unsigned(std::distance(begin, g));
		primitiveCounts[idx] = (*g)->primitiveCount();
		strGeoms[idx] = *(*g);
	}

	std::vector<VkAccelerationStructureGeometryKHR> auxGeoms;
	const VkAccelerationStructureBuildGeometryInfoKHR buildInfo = buildGeometryInfo(strGeoms, auxGeoms);
	
	VkAccelerationStructureBuildSizesInfoKHR sizeInfo{};
	sizeInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;

	di.vkGetAccelerationStructureBuildSizesKHR(
		*device,
		VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
		&buildInfo,
		primitiveCounts.data(),
		&sizeInfo
	);

	const ZBufferUsageFlags blasUsage(VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR,
										VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
	ZBuffer blasBuffer = createBuffer(device, sizeInfo.accelerationStructureSize, blasUsage, ZMemoryPropertyDeviceFlags);
	str.setParam<ZBuffer>(blasBuffer);

	VkAccelerationStructureCreateInfoKHR createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
	createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
	createInfo.size = sizeInfo.accelerationStructureSize;
	createInfo.buffer = *blasBuffer;

	VKASSERT(di.vkCreateAccelerationStructureKHR(*device, &createInfo, callbacks, str.setter()));

	return str;
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

static std::vector<ZBtmAccelerationStructure>
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
createTopAccelerationStructure(BLAS btm, std::optional<std::vector<BLAS>> otherBtms)
{
	ZDevice device = btm.structure.getParam<ZDevice>();
	add_cref<ZDeviceInterface> di = device.getInterface();
	const VkAllocationCallbacksPtr callbacks = device.getParam<VkAllocationCallbacksPtr>();

	const uint32_t instanceCount = 1u + (otherBtms.has_value() ? data_count(*otherBtms) : 0u);
	ZTopAccelerationStructure tlas(VK_NULL_HANDLE, device, callbacks,
									{/*tlas buffer*/}, {/*instances buffer*/ },
									distinctBottomsByHandle(btm, otherBtms), 0u, 0u, 0u, instanceCount);

	ZBuffer instancesBuffer = createBuffer<VkAccelerationStructureInstanceKHR>(device, instanceCount,
										ZBufferUsageFlags(VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
											VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR));

	for (uint32_t b = 0u; b < instanceCount; ++b)
	{
		add_ref<BLAS> blas = (0u == b) ? btm : (*otherBtms)[b - 1u];

		VkAccelerationStructureInstanceKHR i{};
		i.mask = blas.mask;
		i.flags = blas.flags;
		i.transform = blas.transform;
		i.instanceCustomIndex = blas.instanceCustomIndex;
		i.instanceShaderBindingTableRecordOffset = blas.instanceShaderBindingTableRecordOffset;
		i.accelerationStructureReference = bufferGetAddress(blas.structure.getParam<ZBuffer>());
		bufferWrite(instancesBuffer, i, b);
	}
	tlas.setParam<ZDistType<SomeOne, ZBuffer>>(instancesBuffer);

	VkAccelerationStructureGeometryKHR tlasGeom{};
	tlasGeom.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
	tlasGeom.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
	tlasGeom.geometry.instances.sType =	VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
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
		&instanceCount,
		&sizeInfo
	);
	tlas.setParam<ZDistType<SizeFirst, VkDeviceSize>>(sizeInfo.accelerationStructureSize);
	tlas.setParam<ZDistType<SizeSecond, VkDeviceSize>>(sizeInfo.updateScratchSize);
	tlas.setParam<ZDistType<SizeThird, VkDeviceSize>>(sizeInfo.buildScratchSize);

	ZBuffer tlasBuffer = createBuffer(device, sizeInfo.accelerationStructureSize,
										ZBufferUsageFlags(VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
											VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR));
	tlas.setParam<ZDistType<SomeZero, ZBuffer>>(tlasBuffer);

	VkAccelerationStructureCreateInfoKHR createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
	createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
	createInfo.size = sizeInfo.accelerationStructureSize;
	createInfo.buffer = *tlasBuffer;

	VKASSERT(di.vkCreateAccelerationStructureKHR(*device, &createInfo, callbacks, tlas.setter()));

	return tlas;
}

void commandBufferBuildAccelerationStructure(ZCommandBuffer cmd, ZBtmAccelerationStructure bottomAS)
{
	if (bottomAS.getParam<VkBool32>())
	{
		return;
	}

	ZDevice device = cmd.getParam<ZDevice>();
	add_cref<ZDeviceInterface> di = device.getInterface();
	//const VkAllocationCallbacksPtr callbacks = device.getParam<VkAllocationCallbacksPtr>();

	add_ref<std::vector<ZAccelerationStructureGeometry>> strGeoms =
		bottomAS.getParamRef<std::vector<ZAccelerationStructureGeometry>>();

	std::vector<VkAccelerationStructureGeometryKHR> aux;
	VkAccelerationStructureBuildGeometryInfoKHR buildInfo = buildGeometryInfo(strGeoms, aux);

	std::vector<VkAccelerationStructureBuildRangeInfoKHR> rangeInfos(strGeoms.size());
	std::vector<add_cptr<VkAccelerationStructureBuildRangeInfoKHR>> rangeInfosPtrs(strGeoms.size());
	std::vector<uint32_t> primitiveCounts(strGeoms.size());

	for (auto begin = strGeoms.begin(), g = begin; g != strGeoms.end(); ++g)
	{
		const auto idx = make_unsigned(std::distance(begin, g));
		primitiveCounts[idx] = g->primitiveCount();

		rangeInfos[idx].firstVertex = 0u;
		rangeInfos[idx].primitiveCount = g->primitiveCount();
		rangeInfos[idx].primitiveOffset = 0u;
		rangeInfos[idx].transformOffset = 0u;
		rangeInfosPtrs[idx] = &rangeInfos[idx];
	}

	VkAccelerationStructureBuildSizesInfoKHR sizeInfo{};
	sizeInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;

	di.vkGetAccelerationStructureBuildSizesKHR(
		*device,
		VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
		&buildInfo,
		primitiveCounts.data(),
		&sizeInfo
	);

	ZBuffer scratchBuffer = createBuffer(device, sizeInfo.buildScratchSize,
							ZBufferUsageFlags(VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
												VK_BUFFER_USAGE_STORAGE_BUFFER_BIT));
	buildInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
	buildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
	buildInfo.dstAccelerationStructure = *bottomAS;
	buildInfo.scratchData.deviceAddress = bufferGetAddress(scratchBuffer);

	di.vkCmdBuildAccelerationStructuresKHR(*cmd, 1, &buildInfo, rangeInfosPtrs.data());

	bottomAS.setParam<VkBool32>(VK_TRUE); // already built
}

void commandBufferBuildAccelerationStructure(ZCommandBuffer cmd, ZTopAccelerationStructure topAS)
{
	add_ref<std::vector<ZBtmAccelerationStructure>> bottoms =
		topAS.getParamRef<std::vector<ZBtmAccelerationStructure>>();
	for (add_ref<ZBtmAccelerationStructure> btm : bottoms)
	{
		commandBufferBuildAccelerationStructure(cmd, btm);
	}

	ZDevice device = cmd.getParam<ZDevice>();
	add_cref<ZDeviceInterface> di = device.getInterface();
	//const VkAllocationCallbacksPtr callbacks = device.getParam<VkAllocationCallbacksPtr>();
	//ZBuffer tlasBuffer = topAS.getParam<ZDistType<ZDistName::SomeZero, ZBuffer>>();
	ZBuffer instancesBuffer = topAS.getParam<ZDistType<ZDistName::SomeOne, ZBuffer>>();
	ZBuffer scratchBuffer = createBuffer(device, topAS.getParam<ZDistType<SizeThird, VkDeviceSize>>(),
											ZBufferUsageFlags(VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
												VK_BUFFER_USAGE_STORAGE_BUFFER_BIT));

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
	buildInfo.dstAccelerationStructure = *topAS;
	buildInfo.scratchData.deviceAddress = bufferGetAddress(scratchBuffer);

	VkAccelerationStructureBuildRangeInfoKHR range{};
	range.primitiveCount = topAS.getParam<uint32_t>();

	const VkAccelerationStructureBuildRangeInfoKHR* pRanges[] = { &range };
	di.vkCmdBuildAccelerationStructuresKHR(*cmd, 1u, &buildInfo, pRanges);

	using A = ZBarrierConstants::Access;
	using S = ZBarrierConstants::Stage;
	ZMemoryBarrier2 barrier(A::ACCELERATION_STRUCTURE_WRITE_BIT_KHR,
		S::ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
		A::ACCELERATION_STRUCTURE_READ_BIT_KHR,
		S::RAY_TRACING_SHADER_BIT_KHR);
	commandBufferPipelineBarriers2(cmd, barrier);
}

} // namespace vtf

