#include "variablePointersTests.hpp"
#include "vtfContext.hpp"
#include "vtfBacktrace.hpp"
#include "vtfDSBMgr.hpp"
#include "vtfZImage.hpp"
#include "vtfProgramCollection.hpp"
#include "vtfZPipeline.hpp"
#include "vtfZCommandBuffer.hpp"
#include "vtfCopyUtils.hpp"
#include "vtfOptionParser.hpp"

namespace
{
using namespace vtf;

constexpr Option optionEnableBase64Shader	{ "--enable-base64-shader", 0 };
constexpr Option optionAlwaysBuildShaders	{ "--always-build-shaders", 0 };
struct Params
{
	add_cref<std::string> assets;
	uint32_t subgroupSize = 0;
	Params(add_cref<std::string> assets_);
	OptionParser<Params> getParser();
	bool enableBase6Shader = false;
	bool alwaysBuildShaders = false;
};
Params::Params(add_cref<std::string> assets_)
	: assets(assets_)
{
}
OptionParser<Params> Params::getParser()
{
	OptionFlags flags{ OptionFlag::None };
	OptionParser<Params> p(*this);
	p.addOption(&Params::enableBase6Shader, optionEnableBase64Shader,
		"Enable Base64 shader instead of SPIR-V assembly", { false }, flags);
	p.addOption(&Params::alwaysBuildShaders, optionAlwaysBuildShaders,
		"Force to (re)build shaders every time the application is run", { false }, flags);
	return p;
}

TriLogicInt prepareTests(add_cref<TestRecord>& record, add_cref<strings> cmdLineParams);
TriLogicInt runTests(add_ref<VulkanContext> ctx, add_cref<Params> params);

TriLogicInt prepareTests(add_cref<TestRecord>& record, add_cref<strings> cmdLineParams)
{
	add_cref<GlobalAppFlags> gf(getGlobalAppFlags());

	Params params(record.assets);
	auto parser = params.getParser();
	parser.parse(cmdLineParams);
	add_cref<OptionParserState> state = parser.getState();
	if (state.hasHelp)
	{
		parser.printOptions(std::cout);
		return {};
	}
	if (state.hasErrors)
	{
		std::cout << "[ERROR] " << state.messagesText() << std::endl;
		return {};
	}

	auto onEnablingFeatures = [&](add_ref<DeviceCaps> caps)
	{
		caps.addUpdateFeatureIf(&VkPhysicalDeviceMutableDescriptorTypeFeaturesEXT::mutableDescriptorType)
			.checkSupported("mutableDescriptorType");
		caps.addExtension(VK_EXT_MUTABLE_DESCRIPTOR_TYPE_EXTENSION_NAME).checkSupported();

		caps.addUpdateFeatureIf(&VkPhysicalDeviceScalarBlockLayoutFeaturesEXT::scalarBlockLayout)
			.checkSupported("scalarBlockLayout");
		caps.addExtension(VK_EXT_SCALAR_BLOCK_LAYOUT_EXTENSION_NAME).checkSupported();

		caps.addUpdateFeatureIf(&VkPhysicalDeviceVariablePointerFeaturesKHR::variablePointers)
			.checkSupported("variablePointers");
		caps.addUpdateFeatureIf(&VkPhysicalDeviceVariablePointerFeaturesKHR::variablePointersStorageBuffer)
			.checkSupported("variablePointersStorageBuffer");
		caps.addExtension(VK_KHR_VARIABLE_POINTERS_EXTENSION_NAME).checkSupported();

		// VkPhysicalDeviceBufferAddressFeaturesEXT
		// VkPhysicalDeviceBufferDeviceAddressFeaturesEXT
		//caps.addUpdateFeatureIf(&VkPhysicalDeviceBufferDeviceAddressFeaturesEXT::bufferDeviceAddress)
		//	.checkSupported("bufferDeviceAddress");
		//caps.addExtension(VK_EXT_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME);

		//caps.addUpdateFeatureIf(&VkPhysicalDeviceFeatures::shaderInt64)
		//	.checkSupported("shaderInt64");

		VkPhysicalDeviceSubgroupProperties subgroupProps = makeVkStruct();
		deviceGetPhysicalProperties2(caps.physicalDevice, &subgroupProps);
		params.subgroupSize = subgroupProps.subgroupSize;
	};

	VulkanContext ctx(record.name, gf.layers, {}, {}, onEnablingFeatures, gf.apiVer, gf.debugPrintfEnabled);

	return runTests(ctx, params);
}

std::tuple<ZShaderModule, ZShaderModule> buildShaders(ZDevice device, add_cref<Params> params)
{
	ZShaderModule shader;
	if (params.enableBase6Shader)
	{
		std::vector<char> base64code;
		readFile(fs::path(params.assets) / "s.base64", base64code);
		shader = createShaderModule(device, VK_SHADER_STAGE_COMPUTE_BIT, base64code);
	}
	else
	{
		ProgramCollection p(device, params.assets);
		p.addFromFile(VK_SHADER_STAGE_COMPUTE_BIT, "s.spvasm");
		p.buildAndVerify(params.alwaysBuildShaders);
		shader = p.getShader(VK_SHADER_STAGE_COMPUTE_BIT, 0u);
	}
	return
	{
		ZShaderModule(),
		shader
	};
}

TriLogicInt runTests(add_ref<VulkanContext> ctx, add_cref<Params> params)
{
	UNREF(params);

	const VkShaderStageFlags	shaderStage		= VK_SHADER_STAGE_COMPUTE_BIT;
	struct SmallPC
	{
		uint32_t x, y, z, w;
	};
	struct HugePC : SmallPC
	{
		VkDeviceAddress a;
		uint64_t b;
	};
	const ZPushRange<SmallPC>	small_pushc		(shaderStage);
	const ZPushRange<HugePC>	huge_pushc		(shaderStage); UNREF(huge_pushc);
	auto [glslShader, spirvShader] = buildShaders(ctx.device, params);
	const uint32_t				pc0set[4]		{ 0u, 3u, 7u, 5u };
	const uint32_t				pc1set[4]		{ 1u, 8u, 5u, 11u };
	const uint32_t				elementCount	= ROUNDUP(std::max(pc0set[1], pc1set[1]) + 10u, 16u);
	const ZBufferUsageFlags		usage			(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
	ZBuffer						fooBuffer		= createBuffer(ctx.device, VK_FORMAT_R32G32B32A32_UINT, elementCount,
																usage, ZMemoryPropertyHostFlags,
																ZBufferCreateFlags());
	ZBuffer						barBuffer		= createBuffer(ctx.device, VK_FORMAT_R32G32B32A32_UINT, elementCount,
																ZBufferUsageStorageFlags, ZMemoryPropertyHostFlags,
																ZBufferCreateFlags());
	ZCommandPool			cmdPool			= createCommandPool(ctx.device, ctx.computeQueue);
	LayoutManager			lmFoo			(ctx.device, cmdPool);
	LayoutManager			lmBar			(ctx.device, cmdPool);

	// build layout: [ MUTABLE_EXT=(STORAGE_BUFFER)]
	const uint32_t			bindingFoo		= lmFoo.addBinding(VK_DESCRIPTOR_TYPE_MUTABLE_EXT, VK_IMAGE_LAYOUT_GENERAL,
															shaderStage, { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER });
	const uint32_t			bindingBar		= lmFoo.addBinding(VK_DESCRIPTOR_TYPE_MUTABLE_EXT, VK_IMAGE_LAYOUT_GENERAL,
															shaderStage, { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER });
											lmBar.addBinding(VK_DESCRIPTOR_TYPE_MUTABLE_EXT, VK_IMAGE_LAYOUT_GENERAL,
															shaderStage, { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER });
											lmBar.addBinding(VK_DESCRIPTOR_TYPE_MUTABLE_EXT, VK_IMAGE_LAYOUT_GENERAL,
															shaderStage, { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER });

	ZDescriptorSetLayout	dsFooLayout		= lmFoo.createDescriptorSetLayout();
	ZDescriptorSetLayout	dsBarLayout		= lmBar.createDescriptorSetLayout();
	ZDescriptorSet			descSetFoo		= LayoutManager::getDescriptorSet(dsFooLayout);
	ZDescriptorSet			descSetBar		= LayoutManager::getDescriptorSet(dsBarLayout);
	ZPipelineLayout			plFooLayout		= lmFoo.createPipelineLayout({ dsFooLayout }, small_pushc);
	ZPipelineLayout			plBarLayout		= lmBar.createPipelineLayout({ dsBarLayout }, small_pushc);
	ZPipeline				pipelineFoo		= createComputePipeline(plFooLayout, spirvShader, {}, UVec3(1u));
	ZPipeline				pipelineBar		= createComputePipeline(plBarLayout, spirvShader, {}, UVec3(1u));

	lmFoo.updateDescriptorSet(descSetFoo, bindingFoo, fooBuffer, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
	lmFoo.updateDescriptorSet(descSetFoo, bindingBar, barBuffer, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
	lmBar.updateDescriptorSet(descSetBar, bindingFoo, barBuffer, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
	lmBar.updateDescriptorSet(descSetBar, bindingBar, fooBuffer, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);

	const SmallPC small_pcFoo{ pc0set[0], pc0set[1], pc0set[2], pc0set[3] };
	const SmallPC small_pcBar{ pc1set[0], pc1set[1], pc1set[2], pc1set[3] };

	const VkPipelineStageFlags	barrierStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
	ZBufferMemoryBarrier barrierFoo(fooBuffer, VK_ACCESS_SHADER_WRITE_BIT,
												(VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT));
	ZBufferMemoryBarrier barrierBar(barBuffer, VK_ACCESS_SHADER_WRITE_BIT,
												(VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT));
	{
		OneShotCommandBuffer cmd(cmdPool);
		commandBufferBindPipeline(cmd, pipelineFoo, true);
		commandBufferPushConstants(cmd, plFooLayout, small_pcFoo);
		commandBufferDispatch(cmd);
		commandBufferPipelineBarriers(cmd, barrierStage, barrierStage, barrierFoo, barrierBar);
		commandBufferBindPipeline(cmd, pipelineBar, true);
		commandBufferPushConstants(cmd, plBarLayout, small_pcBar);
		commandBufferDispatch(cmd);
	}

	bool condFoo = false;
	bool condBar = false;
	const uint32_t sumFoo = small_pcFoo.x + small_pcFoo.y + small_pcFoo.z + small_pcFoo.w;
	const uint32_t sumBar = small_pcBar.x + small_pcBar.y + small_pcBar.z + small_pcBar.w;
	const uint32_t count = std::min(std::max(small_pcFoo.y, small_pcBar.y) + 1u, elementCount);
	{
		std::vector<UVec4> foo;
		bufferRead(fooBuffer, foo);
		std::cout << "Foo buffer:" << std::endl;
		for (uint32_t i = 0; i < count; ++i)
			std::cout << foo[i] << ' ';
		std::cout << std::endl;

		const UVec4 cmp0(sumFoo, small_pcFoo.y, small_pcFoo.z, small_pcFoo.w);
		const UVec4 cmp1(sumBar, small_pcBar.y, small_pcBar.z, small_pcBar.w);

		condFoo = foo[small_pcFoo.y] == cmp0 && foo[small_pcBar.y] == cmp1;
		if (false == condFoo)
		{
			std::vector<UVec4> tmp(count);
			tmp[small_pcFoo.y] = cmp0;
			tmp[small_pcBar.y] = cmp1;
			std::cout << "Expected Foo buffer:" << std::endl;
			for (uint32_t i = 0; i < count; ++i)
				std::cout << tmp[i] << ' ';
			std::cout << std::endl;
		}
	}
	{
		std::vector<UVec3> bar;
		bufferRead(barBuffer, bar);
		std::cout << "Bar buffer:" << std::endl;
		for (uint32_t i = 0; i < count; ++i)
			std::cout << bar[i] << ' ';
		std::cout << std::endl;

		const UVec3 cmp0(sumFoo, small_pcFoo.y, small_pcFoo.z);
		const UVec3 cmp1(sumBar, small_pcBar.y, small_pcBar.z);

		condBar = bar[small_pcFoo.y] == cmp0 && bar[small_pcBar.y] == cmp1;
		if (false == condBar)
		{
			std::vector<UVec3> tmp(count);
			tmp[small_pcFoo.y] = cmp0;
			tmp[small_pcBar.y] = cmp1;
			std::cout << "Expected Bar buffer:  " << std::endl;
			for (uint32_t i = 0; i < count; ++i)
				std::cout << tmp[i] << ' ';
			std::cout << std::endl;
		}
	}

	return (condFoo && condBar) ? 0 : 1;
}

} // unnamed namespace

template<> struct TestRecorder<VARIABLE_POINTERS>
{
	static bool record(TestRecord&);
};
bool TestRecorder<VARIABLE_POINTERS>::record(TestRecord& record)
{
	record.name = "variable_pointers";
	record.call = &prepareTests;
	return true;
}
