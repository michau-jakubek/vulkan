#include "descriptorHeapTests.hpp"
#include "vtfCommandLine.hpp"
#include "vtfBacktrace.hpp"
#include "vtfGlfwEvents.hpp"
#include "vtfCanvas.hpp"
#include "vtfExtensions.hpp"
#include "vtfDSBMgr.hpp"
#include "vtfZBuffer.hpp"
#include "vtfZImage.hpp"
#include "vtfStructUtils.hpp"
#include "vtfVkUtils.hpp"
#include "vtfZUtils.hpp"
#include "vtfCUtils.hpp"
#include "vtfProgramCollection.hpp"
#include "vtfZPipeline.hpp"
#include "vtfZCommandBuffer.hpp"
#include "vtfZSpecializationInfo.hpp"
#include "vtfZRenderPass.hpp"
#include "vtfVertexInput.hpp"
#include "vtfCopyUtils.hpp"
#include "vtfZBarriers.hpp"

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

		caps.addUpdateFeatureIf(
			&VkPhysicalDeviceScalarBlockLayoutFeatures::scalarBlockLayout)
			.checkSupported("scalarBlockLayout");

		caps.addExtension(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME).checkSupported();
		caps.addExtension(VK_EXT_DESCRIPTOR_HEAP_EXTENSION_NAME).checkSupported();
		caps.addExtension(VK_KHR_MAINTENANCE_5_EXTENSION_NAME).checkSupported();
	};

	add_cref<GlobalAppFlags> gf = getGlobalAppFlags();
	CanvasStyle style = Canvas::DefaultStyle;
	style.visible = true;
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

ZShaderModule buildComputeShader (add_ref<Canvas> canvas, add_cref<Params> params, add_cptr<char> fileName)
{
	add_cref<GlobalAppFlags> gf = getGlobalAppFlags();
	ProgramCollection programs(canvas.device, params.m_assets);
	programs.addFromFile(VK_SHADER_STAGE_COMPUTE_BIT, fileName);
	programs.buildAndVerify(gf.vulkanVer, gf.spirvVer, gf.spirvValidate, gf.genSpirvDisassembly, params.buildAlways);
	return programs.getShader(VK_SHADER_STAGE_COMPUTE_BIT);
}

void buildGraphicsShaders (add_ref<Canvas> canvas, add_cref<Params> params,
						   add_ref<ZShaderModule> vert, add_ref<ZShaderModule> frag)
{
	add_cref<GlobalAppFlags> gf = getGlobalAppFlags();
	ProgramCollection programs(canvas.device, params.m_assets);
	programs.addFromFile(VK_SHADER_STAGE_VERTEX_BIT, "sample.vert");
	programs.addFromFile(VK_SHADER_STAGE_FRAGMENT_BIT, "sample.frag");
	programs.buildAndVerify(gf.vulkanVer, gf.spirvVer, gf.spirvValidate, gf.genSpirvDisassembly, params.buildAlways);
	vert = programs.getShader(VK_SHADER_STAGE_VERTEX_BIT);
	frag = programs.getShader(VK_SHADER_STAGE_FRAGMENT_BIT);
}

// One resource expected in a descriptor heap, in binding (addBinding) order.
struct HeapResource
{
	VkDescriptorType	type;
	ZBuffer				buffer;	// for buffer descriptor types
	ZImageView			view;	// for image descriptor types
	VkImageLayout		layout;	// for image descriptor types
};

// Physical slot offset the mapping resolves to (mirrors the framework's internal placement).
VkDeviceSize heapSlotOffsetOf (add_cref<VkDescriptorSetAndBindingMappingEXT> m, uint32_t index)
{
	return (m.source == VK_DESCRIPTOR_MAPPING_SOURCE_HEAP_WITH_PUSH_INDEX_EXT)
			? VkDeviceSize(m.sourceData.pushIndex.heapOffset) + VkDeviceSize(index) * m.sourceData.pushIndex.heapIndexStride
			: VkDeviceSize(m.sourceData.constantOffset.heapOffset);
}

VkDeviceSize heapDescriptorSizeOf (add_cref<VkPhysicalDeviceDescriptorHeapPropertiesEXT> dhp, VkDescriptorType type)
{
	switch (type)
	{
	case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
	case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
		return dhp.imageDescriptorSize;
	default:
		return dhp.bufferDescriptorSize;
	}
}

// Independently rebuilds each descriptor with vkWriteResourceDescriptorsEXT and byte-compares it
// against what createDescriptorHeap() wrote into the heap buffer at the mapping's slot offset.
// Returns the number of detected errors (0 == the heap was created as expected).
int verifyDescriptorHeap (
	add_ref<Canvas>										canvas,
	add_cref<VkPhysicalDeviceDescriptorHeapPropertiesEXT> dhp,
	add_cref<std::string>								label,
	ZBuffer												heapBuffer,
	add_cref<DescriptorHeapMappings>					mappings,
	add_cref<std::vector<HeapResource>>					resources)
{
	add_cref<ZDeviceInterface> di = canvas.device.getInterface();

	if (false == heapBuffer.has_handle())
	{
		std::cout << label << ": createDescriptorHeap() returned a buffer without handle" << std::endl;
		return 1;
	}

	int errors = 0;

	const VkDeviceSize heapSize		= bufferGetSize(heapBuffer);
	const VkDeviceSize heapAddress	= bufferGetAddress(heapBuffer);

	VkDeviceSize minExpected = ROUNDUP(dhp.minResourceHeapReservedRange, std::max<VkDeviceSize>(dhp.resourceHeapAlignment, 1u));
	for (size_t i = 0u; i < resources.size(); ++i)
		minExpected = std::max(minExpected,
			heapSlotOffsetOf(mappings[i], static_cast<uint32_t>(i)) + heapDescriptorSizeOf(dhp, resources[i].type));

	std::cout << label << ": heap size " << heapSize << " (expected at least " << minExpected << ")"
			  << ", device address " << std::hex << heapAddress << std::dec << std::endl;

	if (0u == heapAddress)
	{
		std::cout << label << ": descriptor heap has no device address" << std::endl;
		errors += 1;
	}
	if (heapSize < minExpected)
	{
		std::cout << label << ": descriptor heap is smaller than expected" << std::endl;
		errors += 1;
	}

	for (size_t i = 0u; i < resources.size(); ++i)
	{
		add_cref<HeapResource>	res			= resources[i];
		const VkDeviceSize		descSize	= heapDescriptorSizeOf(dhp, res.type);
		const VkDeviceSize		slotOffset	= heapSlotOffsetOf(mappings[i], static_cast<uint32_t>(i));

		VkResourceDescriptorInfoEXT	info	= makeVkStruct();
		info.type = res.type;

		VkDeviceAddressRangeEXT		addressRange{};
		VkImageDescriptorInfoEXT	imageInfo	= makeVkStruct();
		VkImageViewCreateInfo		viewInfo{};

		switch (res.type)
		{
		case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
		case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
			addressRange.address = bufferGetAddress(res.buffer);
			addressRange.size = bufferGetSize(res.buffer);
			info.data.pAddressRange = &addressRange;
			break;
		case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
		case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
			viewInfo = res.view.getParamRef<VkImageViewCreateInfo>();
			imageInfo.pView = &viewInfo;
			imageInfo.layout = res.layout;
			info.data.pImage = &imageInfo;
			break;
		default:
			std::cout << label << ": verification does not support descriptor type "
					  << uint32_t(res.type) << " at slot " << i << std::endl;
			errors += 1;
			continue;
		}

		std::vector<uint8_t> expected(static_cast<size_t>(descSize), 0u);
		VkHostAddressRangeEXT dst{ expected.data(), static_cast<size_t>(descSize) };
		VKASSERT(VTF_CALL_CHECK(di.vkWriteResourceDescriptorsEXT, *canvas.device, 1u, &info, &dst));

		std::vector<uint8_t> actual(static_cast<size_t>(descSize), 0u);
		const VkBufferCopy copy{ slotOffset, 0u, descSize };
		bufferReadData(heapBuffer, actual.data(), copy);

		if (0 != std::memcmp(expected.data(), actual.data(), expected.size()))
		{
			std::cout << label << ": descriptor mismatch at slot " << i << " (offset " << slotOffset << ")" << std::endl;
			errors += 1;
		}
	}

	return errors;
}

// Builds a storage-image-only heap to exercise createDescriptorHeap()'s VkImageDescriptorInfoEXT
// (pImage) write path, then verifies it host-side. Independent of the compute read-back below.
int runImageHeapTest (add_ref<Canvas> canvas, add_cref<VkPhysicalDeviceDescriptorHeapPropertiesEXT> dhp)
{
	ZImage		image	= createImage(canvas.device, VK_FORMAT_R32_UINT, VK_IMAGE_TYPE_2D, 8u, 8u,
									  ZImageUsageFlags(VK_IMAGE_USAGE_STORAGE_BIT));
	ZImageView	view	= createImageView(image);

	LayoutManager imgLm(canvas.device);
	imgLm.addBinding(view, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_IMAGE_LAYOUT_GENERAL);

	const DescriptorHeapMappings mappings =
		imgLm.getDescriptorHeapMappings(0u, VK_DESCRIPTOR_MAPPING_SOURCE_HEAP_WITH_CONSTANT_OFFSET_EXT);
	ZBuffer heapBuffer = imgLm.createDescriptorHeap(mappings);

	const std::vector<HeapResource> resources
	{
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, ZBuffer(), view, VK_IMAGE_LAYOUT_GENERAL },
	};
	return verifyDescriptorHeap(canvas, dhp, "image heap", heapBuffer, mappings, resources);
}

TriLogicInt runTests (add_ref<Canvas> canvas, add_cref<Params> params)
{
	add_cref<ZDeviceInterface> di = canvas.device.getInterface();

	VkPhysicalDeviceDescriptorHeapPropertiesEXT dhp = makeVkStruct();
	deviceGetPhysicalProperties2(canvas.physicalDevice, &dhp);

	if (0u == dhp.bufferDescriptorSize)
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

	// Heap A (phase 1): two buffer bindings, HEAP_WITH_PUSH_INDEX (unchanged from the original test).
	LayoutManager			lmA			(canvas.device);
	lmA.addBinding(inBuffer,  VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	lmA.addBinding(outBuffer, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
	const DescriptorHeapMappings mappingsA =
		lmA.getDescriptorHeapMappings(0u, VK_DESCRIPTOR_MAPPING_SOURCE_HEAP_WITH_PUSH_INDEX_EXT);
	ZBuffer					heapA		= lmA.createDescriptorHeap(mappingsA);

	// Storage image written by phase 2 through the heap, then sampled by phase 3.
	ZImage					stoImage	= createImage(canvas.device, VK_FORMAT_R32_UINT, VK_IMAGE_TYPE_2D, 8u, 8u,
								ZImageUsageFlags(VK_IMAGE_USAGE_STORAGE_BIT, VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_USAGE_TRANSFER_SRC_BIT));
	ZImageView				stoView		= createImageView(stoImage);

	// Heap B (phase 2): storage buffer (phase-1 output) + storage image -> mixed, HEAP_WITH_CONSTANT_OFFSET.
	LayoutManager			lmB			(canvas.device);
	lmB.addBinding(outBuffer, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
	lmB.addBinding(stoView,   VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_IMAGE_LAYOUT_GENERAL);
	const DescriptorHeapMappings mappingsB =
		lmB.getDescriptorHeapMappings(0u, VK_DESCRIPTOR_MAPPING_SOURCE_HEAP_WITH_CONSTANT_OFFSET_EXT);
	ZBuffer					heapB		= lmB.createDescriptorHeap(mappingsB);

	int errors = 0;

	// Host-side descriptor byte checks: phase-1 buffer heap, the mixed buffer+image heap, and a
	// standalone image-only heap.
	{
		const std::vector<HeapResource> bufRes
		{
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, inBuffer,  ZImageView(), VK_IMAGE_LAYOUT_UNDEFINED },
			{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, outBuffer, ZImageView(), VK_IMAGE_LAYOUT_UNDEFINED },
		};
		errors += verifyDescriptorHeap(canvas, dhp, "buffer heap", heapA, mappingsA, bufRes);

		const std::vector<HeapResource> mixRes
		{
			{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, outBuffer, ZImageView(),	VK_IMAGE_LAYOUT_UNDEFINED },
			{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,  ZBuffer(),  stoView,		VK_IMAGE_LAYOUT_GENERAL },
		};
		errors += verifyDescriptorHeap(canvas, dhp, "mixed heap", heapB, mappingsB, mixRes);
	}
	errors += runImageHeapTest(canvas, dhp);

	// Both compute pipelines consume their heap via the per-binding mappings.
	ZPipeline				pipeline1	= createComputePipeline(buildComputeShader(canvas, params, "shader.comp"), mappingsA);
	ZPipeline				pipeline2	= createComputePipeline(buildComputeShader(canvas, params, "image.comp"),  mappingsB);

	const uint32_t			groupCountX	= (kElements + 63u) / 64u;

	// Phase-1 push data: HEAP_WITH_PUSH_INDEX slot indices {0,1} then the addend at byte offset 8.
	const uint32_t			addend		= 1000u;
	std::vector<uint32_t>	pushData	{ 0u, 1u, addend };

	// Graphics (phase 3): an ordinary combined-image-sampler descriptor set samples the heap-written image.
	ZSampler				sampler		= createSampler(stoView, false /*nearest*/);
	LayoutManager			lmG			(canvas.device);
	lmG.addBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
	ZDescriptorSetLayout	dsLayoutG	= lmG.createDescriptorSetLayout(false);
	ZPipelineLayout			pLayoutG	= lmG.createPipelineLayout({ dsLayoutG });

	const uint32_t			fbWidth		= 256u;
	const uint32_t			fbHeight	= 256u;
	const VkClearValue		clearColor	{ { { 0.0f, 0.0f, 0.0f, 1.0f } } };
	const std::vector<RPA>	colorAtt	{ RPA(AttachmentDesc::Color, canvas.surfaceFormat, clearColor,
											  VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) };
	ZRenderPass				colorRP		= createRenderPass(canvas.device, ZAttachmentPool(colorAtt),
											  ZSubpassDescription2({ RPAR(0u, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) }));
	ZImage					colorImage	= createImage(canvas.device, canvas.surfaceFormat, VK_IMAGE_TYPE_2D, fbWidth, fbHeight,
											  ZImageUsageFlags(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, VK_IMAGE_USAGE_TRANSFER_SRC_BIT));
	ZImageView				colorView	= createImageView(colorImage);
	ZFramebuffer			colorFB		= createFramebuffer(colorRP, fbWidth, fbHeight, { colorView });

	ZShaderModule			vertShader, fragShader;
	buildGraphicsShaders(canvas, params, vertShader, fragShader);
	ZPipeline				graphPipeline = createGraphicsPipeline(pLayoutG, colorRP, vertShader, fragShader,
                                                                    makeExtent2D(fbWidth, fbHeight));

	// Host-visible target for reading the storage image back after the window closes.
	ZBuffer					imageReadBuffer = createBuffer(stoImage, ZBufferUsageStorageFlags, ZMemoryPropertyHostFlags);

	ZRenderPass				presentRP	= canvas.createSinglePresentationRenderPass(clearColor);

	auto onCommandRecording = [&](add_ref<Canvas>, add_cref<Canvas::Swapchain>, ZCommandBuffer cmd, ZFramebuffer framebuffer)
	{
		ZDescriptorSet dsG = lmG.getDescriptorSet(dsLayoutG);
		lmG.updateDescriptorSet(dsG, 0u, stoView, sampler);

		commandBufferBegin(cmd);

		// Phase 1: uniform -> storage buffer copy + addend, through heap A.
		commandBufferBindPipeline(cmd, pipeline1, false);
		commandBufferBindResourceHeap(cmd, heapA);
		commandBufferPushData(cmd, 0u, pushData.data(), pushData.size() * sizeof(uint32_t));
		commandBufferDispatch(cmd, UVec3(groupCountX, 1u, 1u));

		// Make phase-1 output visible to phase 2, and move the image into GENERAL for imageStore.
		commandBufferPipelineBarriers(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
			ZBufferMemoryBarrier(outBuffer, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT),
			ZImageMemoryBarrier(stoImage, VK_ACCESS_NONE, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL));

		// Phase 2: read the storage buffer, write the storage image, through the mixed heap B.
		commandBufferBindPipeline(cmd, pipeline2, false);
		commandBufferBindResourceHeap(cmd, heapB);
		commandBufferDispatch(cmd, UVec3(1u, 1u, 1u));

		// Copy the storage image to a host buffer (also transitions it to SHADER_READ_ONLY for sampling).
		imageCopyToBuffer(cmd, stoImage, imageReadBuffer,
			VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
			VK_ACCESS_NONE, VK_ACCESS_NONE,
			VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

		// Phase 3: sample the heap-written image into an offscreen colour attachment.
		commandBufferBindPipeline(cmd, graphPipeline);
		auto rpbi = commandBufferBeginRenderPass(cmd, colorFB);
			VTF_CALL_CHECK(di.vkCmdDraw, *cmd, 3u, 1u, 0u, 0u);
		commandBufferEndRenderPass(rpbi);

		// Phase 4: blit the colour attachment into the swapchain presentation image.
		ZImage presentImage = framebufferGetImage(framebuffer, 0u);
		commandBufferPipelineBarriers(cmd, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
			ZImageMemoryBarrier(colorImage, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
								VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL),
			ZImageMemoryBarrier(presentImage, VK_ACCESS_NONE, VK_ACCESS_TRANSFER_WRITE_BIT,
								VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL));
		commandBufferBlitImage(cmd, colorImage, presentImage);
		commandBufferMakeImagePresentationReady(cmd, presentImage,
			VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT,
			VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);

		commandBufferEnd(cmd);
	};

	int drawTrigger = 1;
    canvas.events().setDefault(drawTrigger);

	std::cout << "Rendering the descriptor-heap result; close the window to finish." << std::endl;
    const int runResult = canvas.run(onCommandRecording, presentRP, std::ref(drawTrigger));

	// Verification (after the window is closed): the last frame's GPU results are read back to the host.
	std::vector<uint32_t> bufferResult(bufferGetElementCount<uint32_t>(outBuffer), 0u);
	bufferRead(outBuffer, bufferResult);
	for (uint32_t i = 0u; i < kElements; ++i)
	{
		const uint32_t expected = seed[i] + addend;
		if (bufferResult[i] != expected)
		{
			std::cout << "storage buffer mismatch at index " << i
					  << ", expected " << expected << ", got " << bufferResult[i] << std::endl;
			errors += 1;
			break;
		}
	}

	std::vector<uint32_t> imageResult(bufferGetElementCount<uint32_t>(imageReadBuffer), 0u);
	bufferRead(imageReadBuffer, imageResult);
	for (uint32_t i = 0u; i < kElements; ++i)
	{
		const uint32_t expected = seed[i] + addend;	// image texel i == phase-1 output[i]
		if (imageResult[i] != expected)
		{
			std::cout << "storage image mismatch at texel " << i
					  << ", expected " << expected << ", got " << imageResult[i] << std::endl;
			errors += 1;
			break;
		}
	}

	errors += runResult;
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
