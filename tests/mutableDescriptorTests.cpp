#include "mutableDescriptorTests.hpp"
#include "vtfContext.hpp"
#include "vtfBacktrace.hpp"
#include "vtfDSBMgr.hpp"
#include "vtfZImage.hpp"
#include "vtfProgramCollection.hpp"
#include "vtfZPipeline.hpp"
#include "vtfZCommandBuffer.hpp"
#include "vtfCopyUtils.hpp"

namespace
{
using namespace vtf;

struct Params
{
	add_cref<std::string> assets;
	uint32_t subgroupSize = 0;
	Params (add_cref<std::string> assets_);
};
Params::Params (add_cref<std::string> assets_)
	: assets(assets_)
{
}

TriLogicInt prepareTests (add_cref<TestRecord>& record, add_cref<strings> cmdLineParams);
TriLogicInt runTests (add_ref<VulkanContext> ctx, add_cref<Params> params);

TriLogicInt prepareTests (add_cref<TestRecord>& record, add_cref<strings> cmdLineParams)
{
	UNREF(cmdLineParams);
	add_cref<GlobalAppFlags> gf(getGlobalAppFlags());

	Params params(record.assets);

	auto onEnablingFeatures = [&](add_ref<DeviceCaps> caps)
	{
		caps.addUpdateFeatureIf(&VkPhysicalDeviceMutableDescriptorTypeFeaturesEXT::mutableDescriptorType)
			.checkSupported("mutableDescriptorType");
		caps.addExtension(VK_EXT_MUTABLE_DESCRIPTOR_TYPE_EXTENSION_NAME).checkSupported();

		VkPhysicalDeviceSubgroupProperties subgroupProps = makeVkStruct();
		deviceGetPhysicalProperties2(caps.physicalDevice, &subgroupProps);
		params.subgroupSize = subgroupProps.subgroupSize;
	};

	VulkanContext ctx(record.name, gf.layers, {}, {}, onEnablingFeatures, gf.apiVer, gf.debugPrintfEnabled);

	return runTests(ctx, params);
}

std::tuple<ZShaderModule, ZShaderModule> buildShaders (ZDevice device, add_cref<Params> params)
{
	ProgramCollection p(device, params.assets);
	p.addFromFile(VK_SHADER_STAGE_COMPUTE_BIT, "buffer.comp");
	p.addFromFile(VK_SHADER_STAGE_COMPUTE_BIT, "image.comp");
	p.buildAndVerify(true);
	return
	{
		p.getShader(VK_SHADER_STAGE_COMPUTE_BIT, 0u),
		p.getShader(VK_SHADER_STAGE_COMPUTE_BIT, 1u),
	};
}

TriLogicInt runTests (add_ref<VulkanContext> ctx, add_cref<Params> params)
{
	UNREF(params);
	
	ZImage						stoImage	= ctx.createColorImage2D(VK_FORMAT_R32G32B32A32_UINT, 
												params.subgroupSize, 1u);
	ZImageView					stoView		= createImageView(stoImage);
	ZBuffer						stoBuffer	= createBuffer(stoImage);
	ZImage						mutImage	= ctx.createColorImage2D(VK_FORMAT_R32G32B32A32_UINT,
												params.subgroupSize, 1u);
	ZBuffer						mutImageBuffer = createBuffer(mutImage);
	ZImageView					mutView		= createImageView(mutImage);
	ZBuffer						mutBuffer	= createBuffer(mutImage);
	const VkShaderStageFlags	shaderStage	= VK_SHADER_STAGE_COMPUTE_BIT;

	ZCommandPool				cmdPool		= createCommandPool(ctx.device, ctx.computeQueue);
	LayoutManager				lm0			(ctx.device, cmdPool);
	LayoutManager				lm1			(ctx.device, cmdPool);

	// build layout: [STORAGE_BUFFER, (STORAGE_BUFFER | STORAGE_IMAGE), STORAGE_IMAGE]
	uint32_t bindingStoBuffer = lm0.addBinding(stoBuffer, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, shaderStage);
	uint32_t bindingMutable = lm0.addBinding(VK_DESCRIPTOR_TYPE_MUTABLE_EXT, VK_IMAGE_LAYOUT_GENERAL, shaderStage,
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE }, VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT);
	uint32_t bindingStoImage = lm0.addBinding(stoView, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_IMAGE_LAYOUT_GENERAL, shaderStage);

	// build layout: [STORAGE_BUFFER, (STORAGE_BUFFER | STORAGE_IMAGE), STORAGE_IMAGE]
	lm1.addBinding(stoBuffer, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, shaderStage);
	lm1.addBinding(VK_DESCRIPTOR_TYPE_MUTABLE_EXT, VK_IMAGE_LAYOUT_GENERAL, shaderStage,
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE }, VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT);
	lm1.addBinding(stoView, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_IMAGE_LAYOUT_GENERAL, shaderStage);

	ZDescriptorSetLayout dsLayout0 = lm0.createDescriptorSetLayout(true, VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT,
		(VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT | VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT));
	ZDescriptorSetLayout dsLayout1 = lm1.createDescriptorSetLayout(true, VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT,
		(VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT | VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT));

	{
		std::vector<UVec4> v(params.subgroupSize);
		for (uint32_t i = 0u; i < params.subgroupSize; ++i)
			v[i] = UVec4(i + 1u, i + 2u, i + 3u, i + 4u);
		lm0.writeBinding(bindingStoBuffer, v);
		for (add_ref<UVec4> u : v)
			u += u;
		lm0.writeBinding(bindingStoImage, v);
	}

	ZDescriptorSet descriptorSet0 = LayoutManager::getDescriptorSet(dsLayout0);
	ZDescriptorSet descriptorSet1 = LayoutManager::getDescriptorSet(dsLayout1);
	ZPipelineLayout pipelineLayout0 = lm0.createPipelineLayout({ dsLayout0 });
	ZPipelineLayout pipelineLayout1 = lm0.createPipelineLayout({ dsLayout1 });

	auto [bufferShader, imageShader] = buildShaders(ctx.device, params);

	ZPipeline bufferPipeline = createComputePipeline(pipelineLayout0, bufferShader, ZPipelineCache(),
														UVec3(params.subgroupSize, 0u, 0u));
	ZPipeline imagePipeline = createComputePipeline(pipelineLayout1, imageShader, ZPipelineCache(),
														UVec3(params.subgroupSize, 0u, 0u));

	{
		OneShotCommandBuffer cmd(ctx.device, ctx.computeQueue);
		commandBufferBindPipeline(cmd, bufferPipeline, true);
		lm0.updateDescriptorSet(descriptorSet0, bindingMutable, mutBuffer, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
		commandBufferDispatch(cmd);
		commandBufferBindPipeline(cmd, imagePipeline, true);
		lm1.updateDescriptorSet(descriptorSet1, bindingMutable, mutView,
			VK_IMAGE_LAYOUT_GENERAL,
			VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
		commandBufferDispatch(cmd);
		imageCopyToBuffer(cmd, mutImage, mutImageBuffer, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_NONE,
			VK_ACCESS_NONE, VK_ACCESS_NONE, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
			VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
	}

	std::vector<UVec4> x(params.subgroupSize);

	bufferRead(mutBuffer, x);
	for (add_cref<UVec4> u : x)
		std::cout << u << ' ';
	std::cout << std::endl;

	bufferRead(mutImageBuffer, x);
	for (add_cref<UVec4> u : x)
		std::cout << u << ' ';
	std::cout << std::endl;

	return {};
}

} // unnamed namespace

template<> struct TestRecorder<MUTABLE_DESCRIPTOR>
{
	static bool record(TestRecord&);
};
bool TestRecorder<MUTABLE_DESCRIPTOR>::record(TestRecord& record)
{
	record.name = "mutable_descriptor";
	record.call = &prepareTests;
	return true;
}
