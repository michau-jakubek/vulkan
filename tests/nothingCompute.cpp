#include "nothingCompute.hpp"
#include "vtfBacktrace.hpp"
#include "vtfContext.hpp"
#include "vtfProgramCollection.hpp"
#include "vtfLayoutManager.hpp"
#include "vtfZPipeline.hpp"
#include "vtfZCommandBuffer.hpp"

namespace
{
using namespace vtf;

TriLogicInt runTest(const TestRecord& record, const strings& cmdLineParams)
{
	UNREF(cmdLineParams);
	add_cref<GlobalAppFlags> gf(getGlobalAppFlags());

	auto onEnablingFeatures = [](add_ref<DeviceCaps> caps) ->VkPhysicalDeviceFeatures2
	{
		bool has10 = false;
		bool has20 = false;
		MULTI_UNREF(has10, has20);

		has10 = caps.hasPhysicalDeviceFeatures10();
		has20 = caps.hasPhysicalDeviceFeatures20();
		caps.addUpdateFeature(VkPhysicalDeviceFeatures());
		try
		{
			caps.addUpdateFeature(VkPhysicalDeviceFeatures());
		}
		catch (...)
		{
			has10 = caps.hasPhysicalDeviceFeatures10();
			has20 = caps.hasPhysicalDeviceFeatures20();
		}
		has10 = caps.hasPhysicalDeviceFeatures10();
		has20 = caps.hasPhysicalDeviceFeatures20();
		caps.addUpdateFeature(VkPhysicalDeviceFeatures2());
		has10 = caps.hasPhysicalDeviceFeatures10();
		has20 = caps.hasPhysicalDeviceFeatures20();
		has20 = caps.removeFeature< VkPhysicalDeviceFeatures2>();
		has10 = caps.hasPhysicalDeviceFeatures10();
		has20 = caps.hasPhysicalDeviceFeatures20();
		has10 = caps.removeFeature(mkstype<VkPhysicalDeviceFeatures>);
		has10 = caps.hasPhysicalDeviceFeatures10();
		has20 = caps.hasPhysicalDeviceFeatures20();

		//auto f10 = caps.addFeature(VkPhysicalDeviceFeatures());
		//f10.checkNotSupported(&VkPhysicalDeviceFeatures::alphaToOne, false);

		//caps.addFeature(VkPhysicalDeviceFeatures2());
		return {};
	};

	VulkanContext ctx(record.name, gf.layers, {}, {}, onEnablingFeatures, gf.apiVer, gf.debugPrintfEnabled);

	ProgramCollection		programs	(ctx.device, record.assets);
	programs.addFromFile(VK_SHADER_STAGE_COMPUTE_BIT, "shader.comp");
	programs.buildAndVerify();
	ZShaderModule			shader		= programs.getShader(VK_SHADER_STAGE_COMPUTE_BIT);

	LayoutManager				lm(ctx.device);
	ZPipelineLayout				pipelineLayout = lm.createPipelineLayout();
	ZPipeline					pipeline = createComputePipeline(pipelineLayout, shader);

	OneShotCommandBuffer shot(ctx.device, ctx.computeQueue);
	ZCommandBuffer shotCmd = shot.commandBuffer;
	commandBufferBindPipeline(shotCmd, pipeline);
	commandBufferDispatch(shotCmd);

	return 0;
}

}

template<> struct TestRecorder<NOTHING_COMPUTE>
{
	static bool record(TestRecord&);
};
bool TestRecorder<NOTHING_COMPUTE>::record(TestRecord& record)
{
	record.name = "nothing_compute";
	record.call = &runTest;
	return true;
}
