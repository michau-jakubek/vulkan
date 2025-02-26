#include "descriptorBufferTests.hpp"
#include "vtfBacktrace.hpp"
#include "vtfCanvas.hpp"
#include "vtfLayoutManager.hpp"
#include "vtfProgramCollection.hpp"
#include "vtfZCommandBuffer.hpp"

namespace
{
using namespace vtf;

struct Params
{
	add_cref<std::string> m_assets;
	Params(add_cref<std::string> assets, add_cref<strings> cmdLineParams)
		: m_assets(assets)
	{
		UNREF(cmdLineParams);
	}
};

TriLogicInt runTests (Canvas& canvas, add_cref<Params> params);

TriLogicInt prepareTests (const TestRecord& record, const strings& cmdLineParams)
{
	Params p(record.assets, cmdLineParams);

	auto onGetEnabledFeatures = [&](add_ref<DeviceCaps> caps)
	{
		caps.addUpdateFeatureIf<VkPhysicalDeviceBufferDeviceAddressFeatures>(
			&VkPhysicalDeviceBufferDeviceAddressFeatures::bufferDeviceAddress)
			.checkNotSupported(&VkPhysicalDeviceBufferDeviceAddressFeatures::bufferDeviceAddress, true,
				"bufferDeviceAddress is not supported by device");

		caps.addUpdateFeatureIf<VkPhysicalDeviceDescriptorBufferFeaturesEXT>(
			&VkPhysicalDeviceDescriptorBufferFeaturesEXT::descriptorBuffer)
			.checkNotSupported(&VkPhysicalDeviceDescriptorBufferFeaturesEXT::descriptorBuffer, true,
				"descriptorBuffer is not supported by device");

		caps.requiredExtension.push_back(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME);
		caps.requiredExtension.push_back(VK_EXT_DESCRIPTOR_BUFFER_EXTENSION_NAME);
		caps.requiredExtension.push_back(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME);
		caps.requiredExtension.push_back(VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME);
	};

	add_cref<GlobalAppFlags> gf = getGlobalAppFlags();
	Canvas	cs(record.name, gf.layers, {}, {}, Canvas::DefaultStyle, onGetEnabledFeatures, gf.apiVer);
	return runTests(cs, p);
}

TriLogicInt runTests (add_ref<Canvas> canvas, add_cref<Params> params)
{
	MULTI_UNREF(canvas, params);
	return 1;
	/*
	add_cref<GlobalAppFlags> gf = getGlobalAppFlags();

	ProgramCollection		prg(canvas.device, params.m_assets);
	prg.addFromFile(VK_SHADER_STAGE_COMPUTE_BIT, "shader.comp");
	prg.buildAndVerify(gf.vulkanVer, gf.spirvVer, gf.spirvValidate, gf.genSpirvDisassembly);

	ZShaderModule			compShader = prg.getShader(VK_SHADER_STAGE_COMPUTE_BIT);

	LayoutManager lm(canvas.device);
	lm.addBindingAsVector<uint32_t>(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 256);
	ZBuffer descBuffer = lm.createDescriptorBuffer();

	add_cref<ZDeviceInterface> di = canvas.device.getInterface();
	OneShotCommandBuffer shot(canvas.device, canvas.computeQueue);
	ZCommandBuffer cmd = shot.commandBuffer;
	commandBufferBindDescriptorBuffer(cmd, descBuffer);
	return 1;
	*/
}

} // unnamed namespace

template<> struct TestRecorder<DESCRIPTOR_BUFFER>
{
	static bool record(TestRecord&);
};
bool TestRecorder<DESCRIPTOR_BUFFER>::record(TestRecord& record)
{
	record.name = "descriptor_buffer";
	record.call = &prepareTests;
	return true;
}
