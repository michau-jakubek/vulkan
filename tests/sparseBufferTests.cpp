#include "sparseBufferTests.hpp"
#include "vtfCUtils.hpp"
#include "vtfBacktrace.hpp"
#include "vtfStructUtils.hpp"
#include "vtfContext.hpp"
#include "vtfZCommandBuffer.hpp"
#include "vtfProgramCollection.hpp"
#include "vtfDSBMgr.hpp"
#include "vtfZPipeline.hpp"

namespace
{
using namespace vtf;

struct Params
{
	add_cref<std::string> assets;
};

TriLogicInt selfTests (add_ref<VulkanContext> ctx, add_cref<Params> params);
TriLogicInt prepareTests (add_cref<TestRecord> record, add_cref<strings> cmdLineParams);
TriLogicInt performTests (add_ref<VulkanContext> ctx, add_cref<Params> params);

TriLogicInt prepareTests(add_cref<TestRecord> record, add_cref<strings> cmdLineParams)
{
	UNREF(cmdLineParams);

	auto onEnablingFeatures = [&](add_ref<DeviceCaps> caps)
	{
		caps.addUpdateFeatureIf(&VkPhysicalDeviceFeatures::sparseBinding)
			.checkSupported("sparseBinding");
	};

	Params						params	{ record.assets };
	add_cref<GlobalAppFlags>	gf		= getGlobalAppFlags();
	VulkanContext				ctx		(record.name, gf.layers, strings(), strings(), onEnablingFeatures, gf.apiVer);

	return performTests(ctx, params);
}

TriLogicInt selfTests (add_ref<VulkanContext> ctx, add_cref<Params> params)
{
	const VkShaderStageFlagBits			shaderStage			= VK_SHADER_STAGE_COMPUTE_BIT;
	add_cref<GlobalAppFlags>			flags				= getGlobalAppFlags();
	ProgramCollection					programs			(ctx.device, params.assets);
	programs.addFromFile(shaderStage, "self_test.comp");
	programs.buildAndVerify(flags.vulkanVer, flags.spirvVer);
	ZShaderModule						shaderModule		= programs.getShader(shaderStage);

	ZQueue								queue				= deviceGetNextQueue(ctx.device,
																VK_QUEUE_TRANSFER_BIT | VK_QUEUE_SPARSE_BINDING_BIT, false);
	ASSERTION(queue.has_handle());

	struct Item
	{
		uint32_t value;
		uint8_t pad[16];
	};
	const uint32_t						count				= 1000 * 1000;
	const VkDeviceSize					size				= count * sizeof(Item);

	ZBuffer								buffer				= createBuffer(ctx.device, size,
																		ZBufferUsageFlags(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT),
																		ZMemoryPropertyHostFlags,
																		ZBufferCreateFlags(VK_BUFFER_CREATE_SPARSE_BINDING_BIT));
	bufferBindMemory(buffer, queue);

	LayoutManager						lm					(ctx.device);
	const uint32_t						binding				= lm.addBinding(buffer, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER); UNREF(binding);
	ZDescriptorSetLayout				descriptorSetLayout	= lm.createDescriptorSetLayout(false);
	ZDescriptorSet						descriptorSet		= lm.getDescriptorSet(descriptorSetLayout);
	ZPipelineLayout						pipelineLayout		= lm.createPipelineLayout({descriptorSetLayout});

	ZPipeline							pipeline			= createComputePipeline(pipelineLayout, shaderModule, {}, UVec3(10,10,1));

	Alloc								alloc				(buffer);
	std::fill_n(alloc.begin<Item>(), count, Item{ 3u, {} });

	OneShotCommandBuffer				shot				(ctx.device, queue);
	ZCommandBuffer						cmd					= shot.commandBuffer;
		commandBufferBindPipeline(cmd, pipeline);
		commandBufferDispatch(cmd, UVec3(10, 10, 100));
	shot.endRecordingAndSubmit();

	uint32_t i = 0u;
	uint32_t e = 0u;
	for (auto item = alloc.begin<Item>(); item != alloc.end<Item>(); ++item)
	{
		if (i == 3276)
		{
			i = 3276;
		}

		item->value -= 1u;
		if (item->value != (i + 3u))
		{
			std::cout << i << std::endl;
			++e;
		}
		++i;
	}
	std::cout << "e: expected 0, got " << e << std::endl;
	std::cout << "i: expected " << count << ", got " << i << std::endl;

	return (count == i && 0u == e) ? 0 : 1;
}

TriLogicInt performTests (add_ref<VulkanContext> ctx, add_cref<Params> params)
{
	return selfTests(ctx, params);
}

} // unnamed namespace

template<> struct TestRecorder<SPARSE_BUFFER>
{
	static bool record (TestRecord&);
};
bool TestRecorder<SPARSE_BUFFER>::record (TestRecord& record)
{
	record.name = "sparse_buffer";
	record.call = &prepareTests;
	return true;
}
