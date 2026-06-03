#include "descriptorHeapTests.hpp"
#include "vtfCommandLine.hpp"
#include "vtfBacktrace.hpp"
#include "vtfCanvas.hpp"
#include "vtfExtensions.hpp"
#include "vtfDSBMgr.hpp"
#include "vtfZBuffer.hpp"
#include "vtfStructUtils.hpp"
#include "vtfVkUtils.hpp"
#include "vtfZUtils.hpp"
#include "vtfCUtils.hpp"
#include "vtfProgramCollection.hpp"
#include "vtfZPipeline.hpp"
#include "vtfZCommandBuffer.hpp"
#include "vtfZSpecializationInfo.hpp"

#include <algorithm>
#include <array>
#include <cstring>
#include <numeric>
#include <vector>

namespace
{
using namespace vtf;

constexpr uint32_t kElements = 64u;	// must match the array length in assets/descriptor_heap/shader.comp

struct Params
{
	enum State { Run, Help, Error };

	add_cref<std::string>	m_assets;
	add_ref<CommandLine>	m_cmdLine;
	bool					buildAlways;

	Params (add_cref<std::string> assets, add_ref<CommandLine> cmdLine)
		: m_assets		(assets)
		, m_cmdLine		(cmdLine)
		, buildAlways	(false)
	{
	}

	State parse ();

private:
	OptionParser<Params> getParser ();
};

TriLogicInt runTests (add_ref<Canvas> canvas, add_cref<Params> params);

TriLogicInt prepareTests (const TestRecord& record, add_ref<CommandLine> cmdLine)
{
	Params p(record.assets, cmdLine);
	if (p.parse() != Params::State::Run)
	{
		return {};
	}

	auto onGetEnabledFeatures = [&](add_ref<DeviceCaps> caps)
	{
		caps.addUpdateFeatureIf(
			&VkPhysicalDeviceBufferDeviceAddressFeatures::bufferDeviceAddress)
			.checkSupported("bufferDeviceAddress");

		caps.addUpdateFeatureIf(
			&VkPhysicalDeviceDescriptorHeapFeaturesEXT::descriptorHeap)
			.checkSupported("descriptorHeap");

		caps.addExtension(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME).checkSupported();
		caps.addExtension(VK_EXT_DESCRIPTOR_HEAP_EXTENSION_NAME).checkSupported();
		caps.addExtension(VK_KHR_MAINTENANCE_5_EXTENSION_NAME).checkSupported();
	};

	add_cref<GlobalAppFlags> gf = getGlobalAppFlags();
	CanvasStyle style = Canvas::DefaultStyle;
	style.visible = false;
	Canvas canvas(record.name, gf.layers, {}, {}, style, onGetEnabledFeatures, Version(1, 3), gf.debugPrintfEnabled);

	return runTests(canvas, p);
}

constexpr Option optionBuildAlways { "--build-always", 0 };
OptionParser<Params> Params::getParser ()
{
	add_ref<Params>			params	= *this;
	OptionParser<Params>	parser	(params);

	parser.addOption(&Params::buildAlways, optionBuildAlways,
		"force to build the shaders each time the program is run", { false }, OptionFlag::None);

	return parser;
}
Params::State Params::parse ()
{
	OptionParser<Params>	parser	(getParser());
	parser.parse(m_cmdLine);
	OptionParserState		state = parser.getState();

	if (state.hasHelp)
	{
		parser.printOptions(std::cout, 40);
		return State::Help;
	}

	if (state.hasErrors || state.hasWarnings)
	{
		std::cout << state.messages.str() << std::endl;
		if (state.hasErrors) return State::Error;
	}

	return State::Run;
}

ZShaderModule buildShader (add_ref<Canvas> canvas, add_cref<Params> params)
{
	add_cref<GlobalAppFlags> gf = getGlobalAppFlags();
	ProgramCollection programs(canvas.device, params.m_assets);
	programs.addFromFile(VK_SHADER_STAGE_COMPUTE_BIT, "shader.comp");
	programs.buildAndVerify(gf.vulkanVer, gf.spirvVer, gf.spirvValidate, gf.genSpirvDisassembly, params.buildAlways);
	return programs.getShader(VK_SHADER_STAGE_COMPUTE_BIT);
}

TriLogicInt runTests (add_ref<Canvas> canvas, add_cref<Params> params)
{
	add_cref<ZDeviceInterface> di = canvas.device.getInterface();

	VkPhysicalDeviceDescriptorHeapPropertiesEXT dhp = makeVkStruct();
	deviceGetPhysicalProperties2(canvas.physicalDevice, &dhp);

	const VkDeviceSize bufferDescriptorSize = dhp.bufferDescriptorSize;
	if (0u == bufferDescriptorSize)
	{
		std::cout << "bufferDescriptorSize reported as zero" << std::endl;
		return 1;
	}

	ZBufferUsageFlags	uniUsage	(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
	ZBufferUsageFlags	stoUsage	(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
	ZBuffer				inBuffer	= createBuffer<uint32_t>(canvas.device, kElements, uniUsage);
	ZBuffer				outBuffer	= createBuffer<uint32_t>(canvas.device, kElements, stoUsage);

	std::vector<uint32_t> seed(bufferGetElementCount<uint32_t>(inBuffer));
	{
		std::iota(seed.begin(), seed.end(), 1u);
		bufferWrite(inBuffer, seed);

		std::vector<uint32_t> zeros(bufferGetElementCount<uint32_t>(outBuffer), 0u);
		bufferWrite(outBuffer, zeros);
	}

	LayoutManager			lm			(canvas.device);
	const std::array<std::pair<uint32_t, VkDescriptorType>, 2> bindings
	{{
		{ lm.addBinding(inBuffer, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER), VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER },
		{ lm.addBinding(outBuffer, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER },
	}};
	const std::array<ZBuffer, 2> buffers { inBuffer, outBuffer };

	const DescriptorHeapMappings mappings =
		lm.getDescriptorHeapMappings(0u, VK_DESCRIPTOR_MAPPING_SOURCE_HEAP_WITH_PUSH_INDEX_EXT);
	ZBuffer					heapBuffer	= lm.createDescriptorHeap(mappings);

	int errors = 0;

	if (false == heapBuffer.has_handle())
	{
		std::cout << "createDescriptorHeap() returned a buffer without handle" << std::endl;
		return 1;
	}

	const VkDeviceSize heapSize		= bufferGetSize(heapBuffer);
	const VkDeviceSize heapAddress	= bufferGetAddress(heapBuffer);
	const VkDeviceSize slotStride	= ROUNDUP(bufferDescriptorSize, dhp.bufferDescriptorAlignment);
	const VkDeviceSize baseOffset	= ROUNDUP(dhp.minResourceHeapReservedRange, dhp.resourceHeapAlignment);
	const VkDeviceSize minExpected	= baseOffset + (slotStride * bindings.size());

	std::cout << "bufferDescriptorSize: "	<< bufferDescriptorSize << std::endl
			  << "heap size: "				<< heapSize << " (expected at least " << minExpected << ")" << std::endl
			  << "heap device address: "	<< std::hex << heapAddress << std::dec << std::endl;

	if (0u == heapAddress)
	{
		std::cout << "descriptor heap has no device address" << std::endl;
		errors += 1;
	}
	if (heapSize < minExpected)
	{
		std::cout << "descriptor heap is smaller than expected" << std::endl;
		errors += 1;
	}

	for (size_t i = 0u; i < bindings.size(); ++i)
	{
		const VkDeviceSize slotOffset = baseOffset + (slotStride * i);	// identity placement: binding i at heap index i

		std::vector<uint8_t> expected(static_cast<size_t>(bufferDescriptorSize), 0u);
		VkDeviceAddressRangeEXT addressRange{};
		addressRange.address = bufferGetAddress(buffers[i]);
		addressRange.size = bufferGetSize(buffers[i]);

		VkResourceDescriptorInfoEXT info = makeVkStruct();
		info.type = bindings[i].second;
		info.data.pAddressRange = &addressRange;

		VkHostAddressRangeEXT dst{ expected.data(), static_cast<size_t>(bufferDescriptorSize) };
		VKASSERT(VTF_CALL_CHECK(di.vkWriteResourceDescriptorsEXT, *canvas.device, 1u, &info, &dst));

		std::vector<uint8_t> actual(static_cast<size_t>(bufferDescriptorSize), 0u);
		const VkBufferCopy copy{ slotOffset, 0u, bufferDescriptorSize };
		bufferReadData(heapBuffer, actual.data(), copy);

		if (0 != std::memcmp(expected.data(), actual.data(), expected.size()))
		{
			std::cout << "Descriptor mismatch at binding " << bindings[i].first
					  << " (offset " << slotOffset << ")" << std::endl;
			errors += 1;
		}
	}

	ZShaderModule			shader		= buildShader(canvas, params);
	ZSpecializationInfo		specInfo;
    ZPipeline				pipeline	= createComputePipeline(shader, mappings, specInfo);

	const uint32_t			localSizeX	= 64u;
	const uint32_t			groupCountX	= (kElements + localSizeX - 1u) / localSizeX;

	// One contiguous push-data block shared by both heap features:
	//  [0 .. bindings.size())     : HEAP_WITH_PUSH_INDEX slot indices (identity {0,1,...})
	//  [bindings.size()]          : the heap-mode push constant `addend`, read by the shader
	//                               at byte offset bindings.size()*sizeof(uint32_t) (== 8).
	// addend is added to every element, so a driver that honours push data changes the read-back.
	const uint32_t			addend		= 1000u;
	std::vector<uint32_t>	pushData(bindings.size() + 1u);
	std::iota(pushData.begin(), std::prev(pushData.end()), 0u);
	pushData.back() = addend;

	{
		OneShotCommandBuffer	shot(canvas.device, canvas.computeQueue);
		ZCommandBuffer			cmd = shot.commandBuffer;
		commandBufferBindPipeline(cmd, pipeline, false);
		commandBufferBindResourceHeap(cmd, heapBuffer);
		commandBufferPushData(cmd, 0u, pushData.data(), pushData.size() * sizeof(uint32_t));
		commandBufferDispatch(cmd, UVec3(groupCountX, 1u, 1u));
	}

	std::vector<uint32_t> result(bufferGetElementCount<uint32_t>(outBuffer), 0u);
	bufferRead(outBuffer, result);

	if (result.size() != seed.size())
	{
		std::cout << "storage buffer element count mismatch" << std::endl;
		errors += 1;
	}
	else
	{
		for (size_t i = 0u; i < result.size(); ++i)
		{
			const uint32_t expectedValue = seed[i] + addend;
			if (result[i] != expectedValue)
			{
				std::cout << "storage buffer mismatch at index " << i
						  << ", expected " << expectedValue << ", got " << result[i] << std::endl;
				errors += 1;
				break;
			}
		}
	}

	std::cout << (errors ? "FAILED" : "PASSED") << std::endl;

	return errors;
}

} // unnamed namespace

template<> struct TestRecorder<DESCRIPTOR_HEAP>
{
	static bool record(TestRecord&);
};
bool TestRecorder<DESCRIPTOR_HEAP>::record(TestRecord& record)
{
	record.name = "descriptor_heap";
	record.call = &prepareTests;
	return true;
}
