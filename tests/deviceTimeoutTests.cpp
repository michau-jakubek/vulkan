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
using time_point = std::chrono::time_point<std::chrono::steady_clock>;

struct Params
{
	add_cref<std::string>	assets;
	int						execCount;
	int						waitTime;
	int						wgCountX;
	int						wgCountY;
	int						wgCountZ;
	bool					buildAlways;
	bool					singleInstance;
	bool					deviceLost;

	Params (add_cref<std::string>	assets_)
		: assets			(assets_)
		, execCount			(1)
		, waitTime			(1)
		, wgCountX			(1)
		, wgCountY			(1)
		, wgCountZ			(1)
		, buildAlways		(false)
		, singleInstance	(false)
		, deviceLost		(false) {}
	OptionParser<Params> getParser ();
};
constexpr Option optionBuildAlways		{ "--build-always", 0 };
constexpr Option optionSingleInstance	{ "--single-instance", 0 };
constexpr Option optionDeviceLoat		{ "--device-lost", 0 };
constexpr Option optionExecCount		{ "-exec-count", 1 };
constexpr Option optionWaitTime			{ "-wait-time", 1 };
constexpr Option optionWgCountX			{ "-wg-count-x" ,1 };
constexpr Option optionWgCountY			{ "-wg-count-y" ,1 };
constexpr Option optionWgCountZ			{ "-wg-count-z" ,1 };
OptionParser<Params> Params::getParser ()
{
	OptionFlags				flags	(OptionFlag::PrintDefault);
	OptionParser<Params>	parser	(*this);
	parser.addOption(&Params::buildAlways, optionBuildAlways,
					 "Force to build the shaders always", { buildAlways }, flags);
	parser.addOption(&Params::execCount, optionExecCount,
					"Execute the test exec-count times", { execCount }, flags);
	parser.addOption(&Params::waitTime, optionWaitTime,
					"Time in milliseconds to get device timeout or lost", { waitTime }, flags);
	parser.addOption(&Params::deviceLost, optionDeviceLoat,
					"Try to lost device instead of timeout", { deviceLost }, flags);
	parser.addOption(&Params::singleInstance, optionSingleInstance,
					"Use the same instance for all the executions", { singleInstance }, flags);
	parser.addOption(&Params::wgCountX, optionWgCountX,
					"Change work group count X", { wgCountX }, flags);
	parser.addOption(&Params::wgCountY, optionWgCountY,
					"Change work group count Y", { wgCountY }, flags);
	parser.addOption(&Params::wgCountZ, optionWgCountZ,
					"Change work group count Z", { wgCountZ }, flags);

	return parser;
}

TriLogicInt prepareTest (const TestRecord& record, const strings& cmdLineParams);
int			runTest (add_ref<VulkanContext> ctx, add_cref<Params> params,
					 const time_point start, const int exeNum, const int exeCount);

TriLogicInt prepareTest (const TestRecord& record, const strings& cmdLineParams)
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
	params.waitTime = std::max(params.waitTime, 1);

	const int execCount = params.execCount > 0 && params.execCount <= 100 ? params.execCount : 1;
	params.wgCountX = std::max(params.wgCountX, 1);
	params.wgCountY = std::max(params.wgCountY, 1);
	params.wgCountZ = std::max(params.wgCountZ, 1);

	std::vector<int> results(make_unsigned(execCount));

	add_cref<GlobalAppFlags>	gf = getGlobalAppFlags();

	time_point singleStart;
	ZInstance singleInstance;
	if (params.singleInstance)
	{
		singleStart = std::chrono::steady_clock::now();
		singleInstance = createInstance(record.name, getAllocationCallbacks(),
							gf.layers, strings(), gf.apiVer, gf.debugPrintfEnabled);
	}

	for (int execution = 0; execution < execCount; ++execution)
	{
		VkPhysicalDeviceSynchronization2Features	synchFeatures = makeVkStruct();

		auto onEnablingFeatures = [&](ZPhysicalDevice, add_ref<strings>)
		{
			const VkPhysicalDeviceFeatures2 requiredFeatures = makeVkStruct(&synchFeatures);
			return requiredFeatures;
		};

		const time_point start = params.singleInstance
									? singleStart
									: std::chrono::steady_clock::now();
		ZInstance instance = params.singleInstance
								? singleInstance
								: createInstance(record.name, getAllocationCallbacks(),
									gf.layers, strings(), gf.apiVer, gf.debugPrintfEnabled);
		ZPhysicalDevice physDevice = selectPhysicalDevice(make_signed(gf.physicalDeviceIndex), instance, strings());
		deviceGetPhysicalFeatures2(physDevice, &synchFeatures);
		if (VK_FALSE == synchFeatures.synchronization2)
		{
			std::cout << "ERROR: " VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME " is not supported by device" << std::endl;
			return (-1);
		}
		ZDevice device = createLogicalDevice(physDevice, onEnablingFeatures, ZSurfaceKHR(), gf.debugPrintfEnabled);

		VulkanContext ctx(instance, physDevice, device);

		results.at(make_unsigned(execution)) = runTest(ctx, params, start, execution, execCount);
	}

	return std::all_of(results.begin(), results.end(), [](const int k) { return k == 0; }) ? 0 : 1;
}

int runTest (add_ref<VulkanContext> ctx, add_cref<Params> params,
			 const time_point start, const int exeNum, const int exeCount)
{
	UNREF(exeCount);

	const time_point	t0				= std::chrono::steady_clock::now();
	LayoutManager		lm				(ctx.device);
	Programs			programs		(ctx.device, params.assets);

	const uint32_t		elementCount	= 256;
	std::vector<uint32_t> bufferData	(elementCount);
	const uint32_t		bufferBinding	= lm.addBindingAsVector<uint32_t>(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, elementCount);

	programs.addFromFile(VK_SHADER_STAGE_COMPUTE_BIT, "timeout.comp");
	programs.addFromFile(VK_SHADER_STAGE_COMPUTE_BIT, "device_lost.comp");
	programs.buildAndVerify(exeNum == 0 && params.buildAlways);
	ZShaderModule		shaderModule	= programs.getShader(VK_SHADER_STAGE_COMPUTE_BIT, (params.deviceLost ? 1u : 0u));

	ZPipelineLayout		pipelineLayout	= lm.createPipelineLayout(lm.createDescriptorSetLayout(),
																	ZPushRange<UVec2>(VK_SHADER_STAGE_COMPUTE_BIT));
	ZPipeline			computePipeline	= createComputePipeline(pipelineLayout, shaderModule, UVec3(1));
	ZCommandPool		compCommandPool = ctx.createComputeCommandPool();
	ZFence				timeoutFence	= createFence(ctx.device);
	ZFence				devLostFence	= createFence(ctx.device);
	ZCommandBuffer		cmd				= allocateCommandBuffer(compCommandPool);
	const uint32_t		timeoutMilli	= make_unsigned(params.waitTime);
	const uint64_t		nanoseconds		= uint64_t(timeoutMilli * 1000 * 1000);
	const uint32_t		data0			= 7u;
	const uint32_t		data1			= 5u;
	const uint32_t		data2			= data0 + data1;
	const uint32_t		data3			= 0u;

	ZBuffer				buffer			= std::get<DescriptorBufferInfo>(lm.getDescriptorInfo(bufferBinding)).buffer;

	std::cout << "Execution index: " << exeNum << std::endl;
	std::cout << "Binding: " << bufferBinding << " Buffer: " << buffer << std::endl;
	std::cout << "Compute: " << shaderModule << std::endl;
	std::cout << "CmdPool: " << compCommandPool << std::endl;
	std::cout << "Command: " << cmd << std::endl;
	std::cout << "Fence1:  " << timeoutFence << std::endl;

	{
		ASSERTMSG(elementCount == lm.getBindingElementCount(bufferBinding), "???");
		bufferData.at(0) = data0; bufferData.at(1) = data1;
		bufferData.at(2) = data2; bufferData.at(3) = data3;
		lm.writeBinding(bufferBinding, bufferData);
	}

	const time_point t1 = std::chrono::steady_clock::now();

	commandBufferBegin(cmd, (params.deviceLost
								? VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT
								: VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT));
	commandBufferBindPipeline(cmd, computePipeline);
	commandBufferPushConstants(cmd, pipelineLayout, UVec2(data0, data1));
	commandBufferDispatch(cmd, (params.deviceLost
						? UVec3(1)
						: UVec3(params.wgCountX, params.wgCountY, params.wgCountZ)));
	commandBufferEnd(cmd);

	const time_point t2 = std::chrono::steady_clock::now();

	const VkResult timeoutResult = commandBufferSubmitAndWait(cmd, timeoutFence, nanoseconds, false);

	const time_point t3 = std::chrono::steady_clock::now();

	std::cout << "Creating of instance and device took "
		<< std::chrono::duration_cast<std::chrono::microseconds>(t0 - start).count()
		<< " microseconds" << std::endl;
	std::cout << "Creating other vulkan objects on the host took "
		<< std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count()
		<< " microseconds" << std::endl;
	std::cout << "Recording of the command buffer took "
		<< std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count()
		<< " microseconds" << std::endl;
	std::cout << "Submitting the command buffer and waiting for the timeout took "
		<< std::chrono::duration_cast<std::chrono::nanoseconds>(t3 - t2).count()
		<< " nanoseconds, you wanted " << nanoseconds << std::endl;

	lm.readBinding(bufferBinding, bufferData);

	const bool changed0		= data0 != bufferData[0];
	const bool changed1		= data1 != bufferData[1];
	const bool unchanged2	= data2 == bufferData[2];
	const bool unchanged3	= data3 == bufferData[3];

	VkResult secondResult = params.deviceLost
		? commandBufferSubmitAndWait(cmd, devLostFence, INVALID_UINT64, false)
		: vkQueueWaitIdle(*ctx.computeQueue);
	const time_point t4 = std::chrono::steady_clock::now();
	std::cout << "To put the command buffer into idle state took "
		<< std::chrono::duration_cast<std::chrono::microseconds>(t4 - t3).count()
		<< " microseconds" << std::endl;
	//secondResult = vkQueueWaitIdle(*ctx.computeQueue);

	std::cout
		<< "Timeout result:  " << vkResultToString(timeoutResult) << std::endl
		<< "Second result:   " << vkResultToString(secondResult) << std::endl
		<< "Buffer[0] is:    " << bufferData.at(0) << ' ' << (changed0 ? "changed" : "UNCHANGED") << std::endl
		<< "Buffer[1] is:    " << bufferData.at(1) << ' ' << (changed1 ? "changed" : "UNCHANGED") << std::endl
		<< "Buffer[2] is:    " << bufferData.at(2) << ' ' << (unchanged2 ? "unchanged" : "CHANGED") << std::endl
		<< "Buffer[3] is:    " << bufferData.at(3) << ' ' << (unchanged3 ? "unchanged" : "CHANGED") << std::endl;

	return ((VK_TIMEOUT == timeoutResult) &&
			((VK_SUCCESS == secondResult) || (VK_ERROR_DEVICE_LOST == secondResult)) &&
			changed0 && changed1 && unchanged2 && unchanged3) ? 0 : 1;
}

} // unnamed namespace

template<> struct TestRecorder<DEVICE_TIMEOUT>
{
	static bool record(TestRecord&);
};
bool TestRecorder<DEVICE_TIMEOUT>::record(TestRecord& record)
{
	record.name = "device_timeout";
	record.call = &prepareTest;
	return true;
}

