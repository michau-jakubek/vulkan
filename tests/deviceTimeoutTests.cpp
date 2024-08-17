#include "deviceTimeoutTests.hpp"
#include "vtfBacktrace.hpp"
#include "vtfCUtils.hpp"
#include "vtfContext.hpp"
#include "vtfStructUtils.hpp"
#include "vtfZCommandBuffer.hpp"
#include "vtfProgramCollection.hpp"
#include "vtfLayoutManager.hpp"
#include "vtfZPipeline.hpp"
#include "vtfZBarriers.hpp"
#include "vtfCopyUtils.hpp"
#include "vtfOptionParser.hpp"

#include <bitset>
#include <numeric>

namespace
{
using namespace vtf;
using Programs = ProgramCollection;

struct Params
{
	add_cref<std::string>	assets;
	bool					buildAlways;

	Params (add_cref<std::string>	assets_)
		: assets		(assets_)
		, buildAlways	(false) {}
	OptionParser<Params> getParser ();
};
constexpr Option optionBuildAlways { "--build-always", 0 };
OptionParser<Params> Params::getParser ()
{
	OptionFlags				flags	(OptionFlag::PrintDefault);
	OptionParser<Params>	parser	(*this);
	parser.addOption(&Params::buildAlways, optionBuildAlways,
					 "Force to build the shaders always", { buildAlways }, flags);
	return parser;
}

TriLogicInt prepareTests (const TestRecord& record, const strings& cmdLineParams);
TriLogicInt runSynchronization2Tests (add_ref<VulkanContext> ctx, add_cref<Params> params);

TriLogicInt prepareTests(const TestRecord& record, const strings& cmdLineParams)
{
	Params					params	(record.assets);
	OptionParser<Params>	parser	= params.getParser();
	OptionParserState		state	{};

	parser.parse(cmdLineParams);
	state = parser.getState();

	if (state.hasHelp)
	{
		parser.printOptions(std::cout);
		return {};
	}

	if (state.hasErrors || state.hasWarnings)
	{
		std::cout << state.messagesText() << std::endl;
		if (state.hasErrors) return {};
	}

	VkPhysicalDeviceSynchronization2Features	synchFeatures = makeVkStruct();

	auto onEnablingFeatures = [&](ZPhysicalDevice physicalDevice, add_ref<strings> extensions)
	{
		VkPhysicalDeviceFeatures2 availableFeatures =
				deviceGetPhysicalFeatures2(physicalDevice, &synchFeatures);
		synchFeatures.synchronization2 |= (containsString(VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME,
														  extensions) ? VK_TRUE : VK_FALSE);
		availableFeatures.features = {};
		return availableFeatures;
	};

	add_cref<GlobalAppFlags>	gf = getGlobalAppFlags();
	VulkanContext				ctx(record.name, gf.layers, strings(), strings(), onEnablingFeatures, gf.apiVer);

	if (VK_FALSE == synchFeatures.synchronization2)
	{
		std::cout << "ERROR: " VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME " is not supported by device" << std::endl;
		return (-1);
	}

	return runSynchronization2Tests(ctx, params);
}


TriLogicInt runSynchronization2Tests (add_ref<VulkanContext> ctx, add_cref<Params> params)
{
	LayoutManager		lm				(ctx.device);
	Programs			programs		(ctx.device, params.assets);

	const uint32_t		elementCount	= 256;
	std::vector<uint32_t> bufferData	(elementCount);
	const uint32_t		bufferBinding	= lm.addBindingAsVector<uint32_t>(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, elementCount);

	programs.addFromFile(VK_SHADER_STAGE_COMPUTE_BIT, "shader.comp");
	programs.buildAndVerify(params.buildAlways);
	ZShaderModule		compShader		= programs.getShader(VK_SHADER_STAGE_COMPUTE_BIT);

	ZPipelineLayout		pipelineLayout	= lm.createPipelineLayout(lm.createDescriptorSetLayout());
	ZPipeline			compPipeline	= createComputePipeline(pipelineLayout, compShader, UVec3(1));
	ZCommandPool		compCommandPool = ctx.createComputeCommandPool();
	ZFence				timeoueFence	= createFence(ctx.device);
	ZCommandBuffer		cmd				= allocateCommandBuffer(compCommandPool);
	const uint32_t		timeoutSeconds	= 2u;
	const uint64_t		milliseconds	= uint64_t(timeoutSeconds * 1e9);
	const uint32_t		data0			= 1u;
	const uint32_t		data1			= data0 + 1u;
	const uint32_t		data2			= data1 + 1u;

	ZBuffer				buffer			= std::get<DescriptorBufferInfo>(lm.getDescriptorInfo(bufferBinding)).buffer;	
	std::cout << "Binding: " << bufferBinding << " Buffer: " << buffer << std::endl;
	std::cout << "Compute: " << compShader << std::endl;

	{
		ASSERTMSG(elementCount == lm.getBindingElementCount(bufferBinding), "???");
		std::iota(bufferData.begin(), bufferData.end(), data0);
		lm.writeBinding(bufferBinding, bufferData);
	}

	commandBufferBegin(cmd, VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT);
	commandBufferBindPipeline(cmd, compPipeline);
	commandBufferDispatch(cmd);
	commandBufferEnd(cmd);
	std::cout << "Force timeout, wait " << timeoutSeconds << " second(s)" << std::endl;
	const VkResult timeoutResult = commandBufferSubmitAndWait(cmd, timeoueFence, milliseconds, false);

	VkResult secondResult = VK_ERROR_UNKNOWN;

	if (timeoutResult == VK_TIMEOUT)
	{
		ZFence fence = createFence(ctx.device);
		std::cout << "Timeout occurred, wait " << timeoutSeconds << " second(s)" << std::endl;
		secondResult = commandBufferSubmitAndWait(cmd, fence, milliseconds);
	}

	lm.readBinding(bufferBinding, bufferData);

	const bool unchanged0	= data0 == bufferData[0];
	const bool unchanged1	= data1 == bufferData[1];
	const bool changed2		= data2 != bufferData[2];

	std::cout
		<< "Timeout result: " << vkResultToString(timeoutResult) << std::endl
		<< "Second result:  " << vkResultToString(secondResult) << std::endl
		<< "Buffer[0] is:   " << bufferData.at(0) << ' ' << (unchanged0 ? "unchanged" : "CHANGED") << std::endl
		<< "Buffer[1] is:   " << bufferData.at(1) << ' ' << (unchanged1 ? "unchanged" : "CHANGED") << std::endl
		<< "Buffer[2] is:   " << bufferData.at(2) << ' ' << (changed2 ? "changed" : "UNCHANGED") << std::endl;

	return ((VK_TIMEOUT == timeoutResult) &&
			((VK_SUCCESS == secondResult) || (VK_ERROR_DEVICE_LOST == secondResult)) &&
			unchanged0 && unchanged1 && changed2) ? 0 : 1;
}

} // unnamed namespace

template<> struct TestRecorder<DEVICE_TIMEOUT>
{
	static bool record(TestRecord&);
};
bool TestRecorder<DEVICE_TIMEOUT>::record(TestRecord& record)
{
	record.name = "device_timeout";
	record.call = &prepareTests;
	return true;
}

