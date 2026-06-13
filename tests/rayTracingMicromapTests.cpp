#include "rayTracingMicromapTests.hpp"
#include "vtfBacktrace.hpp"
#include "vtfContext.hpp"
#include "vtfProgramCollection.hpp"
#include "vtfRTLayoutManager.hpp"
#include "vtfRTShaderCollection.hpp"
#include "vtfRTPipeline.hpp"
#include "vtfRTTypes.hpp"
#include "vtfZCommandBuffer.hpp"
#include "vtfZBuffer.hpp"
#include "vtfZUtils.hpp"
#include "vtfCommandLine.hpp"
#include "vtfVector.hpp"

#include <iostream>
#include <random>
#include <vector>

namespace
{
using namespace vtf;

struct Params
{
	add_cref<std::string> assets;
	uint32_t gridSide; // gridSide^2 triangles; 65^2 = 4225 > 64^2 exercises multi-triangle build paths
	uint32_t seed;
	Params(add_cref<std::string> assets_)
		: assets(assets_), gridSide(65u), seed(1614343620u) {}
};

struct Config
{
	uint32_t	mode;				// 2-state or 4-state opacity micromap
	uint32_t	subdivisionLevel;	// 0 -> 1 subtriangle, 2 -> 16 subtriangles per triangle
	const char*	name;
};

// Centroid of subtriangle 'index' inside the unit right triangle {(0,0),(1,0),(0,1)},
// expressed in (u,v) barycentric-like coordinates. Ported from the Vulkan CTS reference.
void subtriangleCentroid(uint32_t index, uint32_t subdivisionLevel, add_ref<float> outU, add_ref<float> outV)
{
	if (subdivisionLevel == 0u)
	{
		outU = 1.0f / 3.0f;
		outV = 1.0f / 3.0f;
		return;
	}

	uint32_t d = index;
	d = ((d >> 1) & 0x22222222u) | ((d << 1) & 0x44444444u) | (d & 0x99999999u);
	d = ((d >> 2) & 0x0c0c0c0cu) | ((d << 2) & 0x30303030u) | (d & 0xc3c3c3c3u);
	d = ((d >> 4) & 0x00f000f0u) | ((d << 4) & 0x0f000f00u) | (d & 0xf00ff00fu);
	d = ((d >> 8) & 0x0000ff00u) | ((d << 8) & 0x00ff0000u) | (d & 0xff0000ffu);

	uint32_t f = (d & 0xffffu) | ((d << 16) & ~d);
	f ^= (f >> 1) & 0x7fff7fffu;
	f ^= (f >> 2) & 0x3fff3fffu;
	f ^= (f >> 4) & 0x0fff0fffu;
	f ^= (f >> 8) & 0x00ff00ffu;

	uint32_t t = (f ^ d) >> 16;
	uint32_t iu = ((f & ~t) | (d & ~t) | (~d & ~f & t)) & 0xffffu;
	uint32_t iv = ((f >> 16) ^ d) & 0xffffu;
	uint32_t iw = ((~f & ~t) | (d & ~t) | (~d & f & t)) & ((1u << subdivisionLevel) - 1u);

	const float scale = 1.0f / float(1u << subdivisionLevel);
	const float u = (1.0f / 3.0f) * scale;
	const float v = (1.0f / 3.0f) * scale;

	iu &= (1u << subdivisionLevel) - 1u;
	iv &= (1u << subdivisionLevel) - 1u;
	iw &= (1u << subdivisionLevel) - 1u;

	const bool upright = ((iu & 1u) ^ (iv & 1u) ^ (iw & 1u)) != 0u;
	if (!upright)
	{
		iu += 1u;
		iv += 1u;
	}

	if (upright)
	{
		outU = u + float(iu) * scale;
		outV = v + float(iv) * scale;
	}
	else
	{
		outU = float(iu) * scale - u;
		outV = float(iv) * scale - v;
	}
}

// Expected raygen output for a subtriangle: 0 -> miss (transparent), 1 -> any-hit (unknown),
// 2 -> closest-hit (opaque). Mirrors the CTS state machine with no ray/instance flags.
uint32_t expectedMode(add_cref<std::vector<uint8_t>> opacityData, uint32_t triangle, uint32_t subtriangle,
					  uint32_t mode, uint32_t triangleBytes)
{
	uint32_t rawState = 0u;
	if (mode == 2u)
	{
		const uint8_t byte = opacityData[size_t(triangle) * triangleBytes + subtriangle / 8u];
		rawState = (byte >> (subtriangle % 8u)) & 0x1u;
	}
	else
	{
		const uint8_t byte = opacityData[size_t(triangle) * triangleBytes + subtriangle / 4u];
		rawState = (byte >> (2u * (subtriangle % 4u))) & 0x3u;
	}

	const uint32_t state = ~rawState; // move into the special-index number space
	if (state == uint32_t(VK_OPACITY_MICROMAP_SPECIAL_INDEX_FULLY_TRANSPARENT_EXT))
		return 0u;
	if (state == uint32_t(VK_OPACITY_MICROMAP_SPECIAL_INDEX_FULLY_OPAQUE_EXT))
		return 2u;
	return 1u; // fully-unknown (transparent or opaque) -> any-hit runs
}

bool runConfig(add_ref<VulkanContext> ctx, add_cref<Params> params, add_cref<Config> config)
{
	ZDevice device = ctx.device;

	const uint32_t gridSide		= params.gridSide;
	const uint32_t triangleCount	= gridSide * gridSide;
	const uint32_t numSubtriangles	= opacityMicromapSubtriangleCount(config.subdivisionLevel);
	const uint32_t numRays			= triangleCount * numSubtriangles;
	const uint32_t triangleBytes	= opacityMicromapTriangleBytes(config.mode, config.subdivisionLevel);

	// Per-triangle random opacity data.
	std::mt19937 rng(params.seed + config.mode * 7919u + config.subdivisionLevel * 104729u);
	std::vector<uint8_t> opacityData(size_t(triangleBytes) * triangleCount);
	for (add_ref<uint8_t> b : opacityData)
		b = uint8_t(rng() & 0xFFu);

	// One non-overlapping right triangle per grid cell (cell pitch 1.0, triangle size 0.5).
	const float triangleScale = 0.5f;
	std::vector<Vec3> vertices(size_t(triangleCount) * 3u);
	for (uint32_t t = 0u; t < triangleCount; ++t)
	{
		const float gx = float(t % gridSide);
		const float gy = float(t / gridSide);
		vertices[size_t(t) * 3u + 0u] = Vec3(gx, gy, 0.0f);
		vertices[size_t(t) * 3u + 1u] = Vec3(gx + triangleScale, gy, 0.0f);
		vertices[size_t(t) * 3u + 2u] = Vec3(gx, gy + triangleScale, 0.0f);
	}

	OpacityMicromapInfo mmInfo{ triangleCount, config.mode, config.subdivisionLevel, opacityData };
	ZOpacityMicromap micromap = createOpacityMicromap(device, mmInfo);

	ZAccelerationStructureGeometry geom = makeTrianglesGeometryWithOpacityMicromap(device, vertices, micromap);
	ZBtmAccelerationStructure btm = createBtmAccelerationStructure({ &geom });
	ZTopAccelerationStructure top = createTopAccelerationStructure(device, 1u);

	// One ray per (triangle, subtriangle) aimed at the subtriangle centroid mapped into the cell.
	std::vector<Vec4> origins(numRays);
	std::vector<uint32_t> expected(numRays);
	for (uint32_t t = 0u; t < triangleCount; ++t)
	{
		const float gx = float(t % gridSide);
		const float gy = float(t / gridSide);
		for (uint32_t s = 0u; s < numSubtriangles; ++s)
		{
			float u = 0.0f, v = 0.0f;
			subtriangleCentroid(s, config.subdivisionLevel, u, v);
			const uint32_t ray = t * numSubtriangles + s;
			origins[ray] = Vec4(gx + u * triangleScale, gy + v * triangleScale, 1.0f, 0.0f);
			expected[ray] = expectedMode(opacityData, t, s, config.mode, triangleBytes);
		}
	}

	RTShaderCollection coll(device, params.assets);
	RTShaderCollection::SBTShaderGroup group0(0u);
	RTShaderCollection::SBTShaderGroup hitGroup0(group0.next());
	coll.addFromFile(group0, VK_SHADER_STAGE_RAYGEN_BIT_KHR, "micromap.rgen");
	coll.addFromFile(group0, VK_SHADER_STAGE_MISS_BIT_KHR, "micromap.rmiss");
	coll.addFromFile(group0, hitGroup0, VK_SHADER_STAGE_ANY_HIT_BIT_KHR, "micromap.rahit");
	coll.addFromFile(group0, hitGroup0, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, "micromap.rchit");
	coll.buildAndVerify(Version(1, 4), Version(1, 4), true, false, false);
	const std::vector<ZShaderModule> rtShaders = coll.getAllShaders();

	ZCommandPool	cmdPool		= createCommandPool(device, ctx.computeQueue);
	RTLayoutManager	lm			(device, cmdPool);
	ZBuffer			originsBuffer	= createBuffer<Vec4>(device, numRays, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
	ZBuffer			outputBuffer	= createBuffer<uint32_t>(device, numRays, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
	const uint32_t	asBinding		= lm.addBinding(top); UNREF(asBinding);
	const uint32_t	originsBinding	= lm.addBinding(originsBuffer, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
	const uint32_t	outputBinding	= lm.addBinding(outputBuffer, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
	ZDescriptorSetLayout	dsLayout		= lm.createDescriptorSetLayout();
	ZPipelineLayout			pipelineLayout	= lm.createPipelineLayout({ dsLayout });
	ZPipeline				rtPipeline		= createRayTracingPipeline(pipelineLayout, rtShaders,
										rtdetails::PipelineShaderGroupOrder::Default,
										rtdetails::RTPipelineCreateFlags{ VK_PIPELINE_CREATE_RAY_TRACING_OPACITY_MICROMAP_BIT_EXT });

	lm.writeBinding(originsBinding, origins);

	SBTHandles	handles0(rtPipeline, group0);
	SBT<>		sbt0(handles0);

	{
		OneShotCommandBuffer cmd(device, ctx.computeQueue);
		commandBufferBuildMicromap(cmd, micromap);
		commandBufferBuildAccelerationStructure(cmd, top, BLAS(btm), {}, true);
		commandBufferBindPipeline(cmd, rtPipeline);
		sbt0.traceRays(cmd, numRays, 1u, 1u);
	}

	std::vector<uint32_t> result(numRays);
	lm.readBinding(outputBinding, result);

	uint32_t mismatches = 0u;
	for (uint32_t i = 0u; i < numRays; ++i)
	{
		if (result[i] != expected[i])
		{
			if (mismatches < 16u)
				std::cout << "  ray " << i << " (triangle " << (i / numSubtriangles)
						  << ", subtriangle " << (i % numSubtriangles) << "): expected "
						  << expected[i] << " got " << result[i] << std::endl;
			++mismatches;
		}
	}

	std::cout << "[" << config.name << "] triangles=" << triangleCount
			  << " rays=" << numRays << " -> "
			  << (mismatches ? "FAILED" : "Pass");
	if (mismatches)
		std::cout << " (" << mismatches << " mismatches)";
	std::cout << std::endl;

	return mismatches == 0u;
}

TriLogicInt runTest(add_ref<VulkanContext> ctx, add_cref<Params> params)
{
	{
		const auto p = deviceGetPhysicalProperties(ctx.device);
		printPhysicalDevice(p, std::cout);
	}

	VkPhysicalDeviceOpacityMicromapPropertiesEXT mmProps{};
	mmProps.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_OPACITY_MICROMAP_PROPERTIES_EXT;
	deviceGetPhysicalProperties2(ctx.device.getParam<ZPhysicalDevice>(), &mmProps);

	const Config configs[] = {
		{ 2u, 0u, "2_state" },
		{ 4u, 0u, "4_state" },
		{ 2u, 2u, "2_state_subdivision" },
		{ 4u, 2u, "4_state_subdivision" },
	};

	bool allPass = true;
	for (add_cref<Config> config : configs)
	{
		const uint32_t maxSubdivision = (config.mode == 2u)
			? mmProps.maxOpacity2StateSubdivisionLevel
			: mmProps.maxOpacity4StateSubdivisionLevel;
		if (config.subdivisionLevel > maxSubdivision)
		{
			std::cout << "[" << config.name << "] skipped: requires subdivision level "
					  << config.subdivisionLevel << " but device supports " << maxSubdivision << std::endl;
			continue;
		}

		allPass = runConfig(ctx, params, config) && allPass;
	}

	return allPass ? 0 : 1;
}

TriLogicInt prepareTest(add_cref<TestRecord> record, add_ref<CommandLine> cmdLine)
{
	UNREF(cmdLine);
	add_cref<GlobalAppFlags> gf(getGlobalAppFlags());

	Params params(record.assets);

	auto onEnablingFeatures = [](add_ref<DeviceCaps> caps)
	{
		onEnablingRayTracingFeatures(caps);
		onEnablingOpacityMicromapFeatures(caps);
	};

	VulkanContext ctx(record.name, gf.layers, {}, {}, onEnablingFeatures, Version(1, 3), gf.debugPrintfEnabled);
	return runTest(ctx, params);
}

} // unnamed namespace

template<> struct TestRecorder<RAY_TRACING_MICROMAP>
{
	static bool record(TestRecord&);
};
bool TestRecorder<RAY_TRACING_MICROMAP>::record(TestRecord& record)
{
	record.name = "ray_tracing_micromap";
	record.call = &prepareTest;
	return true;
}
