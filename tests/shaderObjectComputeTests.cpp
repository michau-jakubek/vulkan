#include "shaderObjectComputeTests.hpp"
#include "vtfBacktrace.hpp"
#include "vtfOptionParser.hpp"
#include "vtfShaderObjectCollection.hpp"
#include "vtfContext.hpp"
#include "vtfZUtils.hpp"
#include "vtfStructUtils.hpp"
#include "vtfZCommandBuffer.hpp"
#include "vtfLayoutManager.hpp"
#include "vtfZPipeline.hpp"

#include <utility>

namespace
{
using namespace vtf;

struct Params
{
	add_cref<std::string>	assets;
	bool					dontIgnoreSoExtension;
	bool					useGlobalVulkan;
	bool					useGlobalApi;
	bool					useGlobalSpirv;
	bool					dontEnableSoLayer;
	bool					buildAlways;
	bool					mixture;
	bool					structPCobject;
	bool					structPCmodule;

	VkBool32	shaderObject;

	Version		effectiveApiVersion;
	Version		effectiveVkVersion;
	Version		effectiveSpvVersion;

	Params (add_cref<std::string> anAssets)
		: assets				(anAssets)
		, dontIgnoreSoExtension	(false)
		, useGlobalVulkan		(false)
		, useGlobalApi			(false)
		, useGlobalSpirv		(false)
		, dontEnableSoLayer		(false)
		, buildAlways			(false)
		, mixture				(false)
		, structPCobject		(false)
		, structPCmodule		(false)
		, shaderObject			(VK_FALSE)
		, effectiveApiVersion	(Version(1, 3))
		, effectiveVkVersion	(Version(1, 3))
		, effectiveSpvVersion	(Version(1, 3))	{}

	void updateEffectiveVersions()
	{
		add_cref<GlobalAppFlags> gf = getGlobalAppFlags();
		effectiveVkVersion	= useGlobalVulkan	? gf.vulkanVer	: Version(1, 3);
		effectiveApiVersion	= useGlobalApi		? gf.apiVer		: Version(1, 3);
		effectiveSpvVersion	= useGlobalSpirv	? gf.spirvVer	: Version(1, 3);
	}
	OptionParser<Params>		getParser ();
	void						print (add_ref<std::ostream> str) const;
};
constexpr Option optionIgnoreSoExtension	{ "--dont-ignore-so-extension", 0 };
constexpr Option optionUseGlobalApiVer		{ "--use-global-api", 0 };
constexpr Option optionUseGlobalVkVer		{ "--use-global-vulkan", 0 };
constexpr Option optionUseGlobalSpvVer		{ "--use-global-spirv", 0 };
constexpr Option optionBuildAlways			{ "--build-always", 0 };
constexpr Option optionDontEnableSoLayer	{ "--dont-enable-so-layer", 0 };
constexpr Option optionMixture				{ "--mixture", 0 };
constexpr Option optionStructPCobject		{ "--struct-pc-object", 0 };
constexpr Option optionStructPCmodule		{ "--struct-pc-module", 0 };
OptionParser<Params> Params::getParser ()
{
	OptionFlags				flags(OptionFlag::PrintDefault);
	OptionParser<Params>	parser(*this);

	parser.addOption(&Params::useGlobalApi, optionUseGlobalApiVer,
		"Use global -api param, instead of 1.3 version", { useGlobalApi }, flags);
	parser.addOption(&Params::useGlobalVulkan, optionUseGlobalVkVer,
		"Use global -vulkan param, instead of 1.3 version", { useGlobalVulkan }, flags);
	parser.addOption(&Params::useGlobalSpirv, optionUseGlobalSpvVer,
		"Use global -spirv param, instead of 1.3 version", { useGlobalSpirv }, flags);
	parser.addOption(&Params::dontIgnoreSoExtension, optionIgnoreSoExtension,
		"Don't ignore " VK_EXT_SHADER_OBJECT_EXTENSION_NAME " extension", { dontIgnoreSoExtension }, flags);
	parser.addOption(&Params::dontEnableSoLayer, optionDontEnableSoLayer,
		"Do not enable " VK_LAYER_KHRONOS_SHADER_OBJECT_NAME " layer", { dontEnableSoLayer }, flags);
	parser.addOption(&Params::buildAlways, optionBuildAlways,
		"Force to build the shaders always", { buildAlways }, flags);
	parser.addOption(&Params::mixture, optionMixture,
		"Call compute shader module between shader objects", { mixture }, flags);
	parser.addOption(&Params::structPCobject, optionStructPCobject,
		"Upload push constants as single structure to the shader objects", { structPCobject }, flags);
	parser.addOption(&Params::structPCmodule, optionStructPCmodule,
		"Upload push constants as single structure to the shader module", { structPCmodule }, flags);
	return parser;
}
void Params::print (add_ref<std::ostream> str) const
{
	{
		OptionParser<Params> parser = remove_const(*this).getParser();
		const auto options = parser.getOptions();
		for (const auto& option : options)
		{
			str << option->getName() << ": " << option->defaultWriter() << std::endl;
		}
	}

	add_cref<Params> params(*this);

	str << VK_EXT_SHADER_OBJECT_EXTENSION_NAME
		<< " enabled: " << boolean(params.shaderObject != VK_FALSE, true) << std::endl;

	str << "Effective Api version: " << params.effectiveApiVersion << std::endl;
	str << "Effective Vulkan version: " << params.effectiveVkVersion << std::endl;
	str << "Effective SPIR-V version: " << params.effectiveSpvVersion << std::endl;
}

TriLogicInt prepareTests (add_cref<TestRecord> record, add_cref<strings> cmdLineParams);
TriLogicInt performTests (add_ref<VulkanContext> ctx, add_cref<Params> params);

TriLogicInt prepareTests (add_cref<TestRecord> record, add_cref<strings> cmdLineParams)
{
	Params					params	(record.assets);
	OptionParser<Params>	parser	= params.getParser();
	parser.parse(cmdLineParams);
	OptionParserState		state	= parser.getState();

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

	params.updateEffectiveVersions();

	auto onEnablingFeatures = [&](add_ref<DeviceCaps> caps)
	{
		auto so = caps.addUpdateFeatureIf(&VkPhysicalDeviceShaderObjectFeaturesEXT::shaderObject);
		params.shaderObject = so.checkSupported("shaderObject");
		if (params.dontIgnoreSoExtension)
		{
			caps.addExtension(VK_EXT_SHADER_OBJECT_EXTENSION_NAME).checkSupported();
		}
		else
		{
			caps.removeFeature<VkPhysicalDeviceShaderObjectFeaturesEXT>();
		}
	};

	strings instanceLayers;
	if (false == params.dontEnableSoLayer)
	{
		instanceLayers.push_back(VK_LAYER_KHRONOS_SHADER_OBJECT_NAME);
	}

	VulkanContext ctx(record.name, instanceLayers, {}, {}, onEnablingFeatures, params.effectiveApiVersion);

	return performTests(ctx, params);
}

typedef ShaderObjectCollection::ShaderLink ShaderLink;
struct PushConstant
{
	UVec4		v3;
	int			y;
	uint32_t	a;
	UVec2		v2;
	uint32_t	b;
};
void printPushConstantInfo (add_ref<std::ostream> str)
{
	str
		<< "PushConstant { // sizeof " << sizeof(PushConstant) << std::endl
		<< "   vec4    v3; // offset " << offsetof(PushConstant, v3) << std::endl
		<< "   int     y;  // offset " << offsetof(PushConstant, y) << std::endl
		<< "   uint    a;  // offset " << offsetof(PushConstant, a) << std::endl
		<< "   uvec2   v2; // offset " << offsetof(PushConstant, v2) << std::endl
		<< "   uint    b;  // offset " << offsetof(PushConstant, b) << std::endl
		<< "}\n";
}
std::bitset<8> verifyPushConstant(add_cref<PushConstant> pc)
{
	std::bitset<8> fields;

	auto cmp = [&](add_cptr<void> p, auto ptr, uint32_t bit)
	{
		fields.set(bit, (std::memcmp(p, ptr, sizeof(*ptr)) == 0));
	};

	uint32_t field = 0u;
	auto p = reinterpret_cast<add_cptr<uint8_t>>(&pc);

	cmp(&p[offsetof(PushConstant, v3)],	&pc.v3,	field++);
	cmp(&p[offsetof(PushConstant, y)],	&pc.y,	field++);
	cmp(&p[offsetof(PushConstant, a)],	&pc.a,	field++);
	cmp(&p[offsetof(PushConstant, v2)], &pc.v2,	field++);
	cmp(&p[offsetof(PushConstant, b)],	&pc.b,	field++);

	return fields;
}
void printPushConstantStatus(add_cref<PushConstant> pc, add_ref<std::ostream> str)
{
	const auto status = verifyPushConstant(pc);
	str << "Verify host push_constant: ";
	if (status == std::bitset<8>(31))
		str << "OK";
	else str << status.to_ulong() << ", " << status;
	str << std::endl;
}

TriLogicInt performTests (add_ref<VulkanContext> ctx, add_cref<Params> params)
{
	params.print(std::cout);
	printPushConstantInfo(std::cout);

	LayoutManager			lm				(ctx.device);
	add_cref<GlobalAppFlags> gf				= getGlobalAppFlags();
	const uint32_t			binding1buffer	= lm.addBindingAsVector<uint32_t>(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 32);
	const uint32_t			binding2buffer	= lm.addBindingAsVector<uint32_t>(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 32);
	const uint32_t			binding3buffer	= lm.addBindingAsVector<uint32_t>(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 32);
	ZDescriptorSetLayout	dsLayout		= lm.createDescriptorSetLayout(true);

	const int		c0_1	= 44;	const int		c0_2	= 77;	const int		c0_3	= 777;
	const int		c1_1	= 55;	const int		c1_2	= 88;	const int		c1_3	= 888;
	const int		c2_1	= 66;	const int		c2_2	= 99;	const int		c2_3	= 999;
	const int		c3_1	= 1;	const int		c3_2	= 2;	const int		c3_3	= 1;
	const int		c4_1	= 2;	const int		c4_2	= 4;	const int		c4_3	= 2;
	const int		c5_1	= 4;	const int		c5_2	= 1;	const int		c5_3	= 1;
	const uint32_t	fe_1	= 111;	const uint32_t	fe_2	= 222;	const uint32_t	fe_3	= 333;
	const uint32_t	v3x_1	= 31;	const uint32_t	v3x_2	= 11;	const uint32_t	v3x_3	= 1;
	const uint32_t	v3y_1	= 32;	const uint32_t	v3y_2	= 22;	const uint32_t	v3y_3	= 2;
	const uint32_t	v3z_1	= 33;	const uint32_t	v3z_2	= 23;	const uint32_t	v3z_3	= 3;
	const int		y_1		= 34;	const int		y_2		= 24;	const int		y_3		= 4;
	const uint32_t	a_1		= 35;	const uint32_t	a_2		= 25;	const uint32_t	a_3		= 5;
	const uint32_t	v2x_1	= 36;	const uint32_t	v2x_2	= 26;	const uint32_t	v2x_3	= 6;
	const uint32_t	v2y_1	= 37;	const uint32_t	v2y_2	= 27;	const uint32_t	v2y_3	= 7;
	const uint32_t	b_1		= 38;	const uint32_t	b_2		= 28;	const uint32_t	b_3		= 8;

	const PushConstant		pc1				{ UVec4(v3x_1, v3y_1, v3z_1, 0),
												y_1, a_1, UVec2(v2x_1, v2y_1), b_1 };
	const PushConstant		pc2				{ UVec4(v3x_2, v3y_2, v3z_2, 0),
												y_2, a_2, UVec2(v2x_2, v2y_2), b_2 };
	const PushConstant		pc3				{ UVec4(v3x_3, v3y_3, v3z_3, 0),
												y_3, a_3, UVec2(v2x_3, v2y_3), b_3 };
	ZPipelineLayout			pipelineLayout = params.structPCmodule
		? lm.createPipelineLayout({ dsLayout }, ZPushRange<PushConstant>(VK_SHADER_STAGE_COMPUTE_BIT))
		: lm.createPipelineLayout({ dsLayout }, ZPushRange<UVec4>(VK_SHADER_STAGE_COMPUTE_BIT),
												ZPushRange<int>(VK_SHADER_STAGE_COMPUTE_BIT),
												ZPushRange<uint32_t>(VK_SHADER_STAGE_COMPUTE_BIT),
												ZPushRange<UVec2>(VK_SHADER_STAGE_COMPUTE_BIT),
												ZPushRange<uint32_t>(VK_SHADER_STAGE_COMPUTE_BIT));
	ShaderObjectCollection	coll			(ctx.device, params.assets);
	ShaderLink				l1				= coll.addFromFile(VK_SHADER_STAGE_COMPUTE_BIT, "compute1.glsl",
																ShaderLink(), { "." }, "main1");
	ShaderLink				l2				= coll.addFromFile(VK_SHADER_STAGE_COMPUTE_BIT, "compute2.glsl",
																l1, { "." }, "main2");
	coll.updateShaders({ l1, l2 }, { dsLayout });
	coll.updateShaders({ l1, l2 }, params.buildAlways, gf.spirvValidate, gf.genSpirvDisassembly);
	if (params.structPCobject)
		coll.updateShaders({ l1, l2 }, ZPushRange<PushConstant>());
	else coll.updateShaders({ l1, l2 }, ZPushRange<UVec4>(), ZPushRange<int>(), ZPushRange<uint32_t>(),
										ZPushRange<UVec2>(), ZPushRange<uint32_t>());

	coll.updateShader(l1, ZSpecEntry<int>(c0_1), ZSpecEntry<int>(c1_1), ZSpecEntry<int>(c2_1),
							ZSpecEntry<uint32_t>(c3_1), ZSpecEntry<uint32_t>(c4_1), ZSpecEntry<uint32_t>(c5_1));
	coll.updateShader(l2, ZSpecEntry<int>(c0_2), ZSpecEntry<int>(c1_2), ZSpecEntry<int>(c2_2),
							ZSpecEntry<uint32_t>(c3_2), ZSpecEntry<uint32_t>(c4_2), ZSpecEntry<uint32_t>(c5_2));
	coll.buildAndVerify(params.effectiveVkVersion, params.effectiveSpvVersion);

	ZShaderObject			s1				= coll.getShader(VK_SHADER_STAGE_COMPUTE_BIT, 0);
	ZShaderObject			s2				= coll.getShader(VK_SHADER_STAGE_COMPUTE_BIT, 1);

	ZSpecializationInfo		specInfo		{ZSpecEntry<int>(c0_3), ZSpecEntry<int>(c1_3), ZSpecEntry<int>(c2_3),
											 ZSpecEntry<int>(c3_3), ZSpecEntry<int>(c4_3), ZSpecEntry<int>(c5_3)};
	ProgramCollection		programs		(ctx.device, params.assets);
	programs.addFromFile(VK_SHADER_STAGE_COMPUTE_BIT, "compute3.glsl");
	programs.buildAndVerify(params.effectiveVkVersion, params.effectiveSpvVersion,
							gf.spirvValidate, gf.genSpirvDisassembly, params.buildAlways);
	ZShaderModule			s3				= programs.getShader(VK_SHADER_STAGE_COMPUTE_BIT);
	ZPipeline				pipeline		= createComputePipeline(pipelineLayout, s3, specInfo);

	ZCommandPool			cmdPool			= ctx.createComputeCommandPool();

	lm.fillBinding(binding1buffer);
	lm.fillBinding(binding2buffer);
	lm.fillBinding(binding3buffer);

	printPushConstantStatus(pc3, std::cout);

	if (params.mixture == false)
	{
		OneShotCommandBuffer	records(cmdPool);
		ZCommandBuffer cmd = records.commandBuffer;
		commandBufferBindPipeline(cmd, pipeline);
		if (params.structPCmodule)
			commandBufferPushConstants(cmd, pipelineLayout, pc3);
		else commandBufferPushConstants(cmd, pipelineLayout,
			UVec4(v3x_3, v3y_3, v3z_3, 0), y_3, a_3, UVec2(v2x_3, v2y_3), b_3);
		commandBufferDispatch(cmd);
	}

	{
		OneShotCommandBuffer	records(cmdPool);
		ZCommandBuffer cmd = records.commandBuffer;

		commandBufferBindDescriptorSets(cmd, pipelineLayout, VK_PIPELINE_BIND_POINT_COMPUTE);

		if (params.structPCobject)
			commandBufferPushConstants(cmd, pipelineLayout, { s1 }, pc1);
		else commandBufferPushConstants(cmd, pipelineLayout, { s1 },
										UVec4(v3x_1, v3y_1, v3z_1, 0), y_1, a_1, UVec2(v2x_1, v2y_1), b_1);
		commandBufferBindShaders(cmd, { s1 });
		commandBufferDispatch(cmd);

		if (params.mixture)
		{
			commandBufferBindPipeline(cmd, pipeline);
			if (params.structPCmodule)
				commandBufferPushConstants(cmd, pipelineLayout, pc3);
			else commandBufferPushConstants(cmd, pipelineLayout,
				UVec4(v3x_3, v3y_3, v3z_3, 0), y_3, a_3, UVec2(v2x_3, v2y_3), b_3);
			commandBufferDispatch(cmd);
		}

		if (params.structPCobject)
			commandBufferPushConstants(cmd, pipelineLayout, { s2 }, pc2);
		else commandBufferPushConstants(cmd, pipelineLayout, { s2 },
										UVec4(v3x_2, v3y_2, v3z_2, 0), y_2, a_2, UVec2(v2x_2, v2y_2), b_2);
		commandBufferBindShaders(cmd, { s2 });
		commandBufferDispatch(cmd);
	}

	int result = 0;

	{
		const std::vector<uint32_t> expected{ c3_1, c4_1, c5_1, c0_1, c1_1, c2_1, fe_1,
												v3x_1, v3y_1, v3z_1, y_1, a_1, v2x_1, v2y_1, b_1 };
		const uint32_t content1size = lm.getBindingElementCount(binding1buffer);
		std::vector<uint32_t>	content1buffer(content1size);
		lm.readBinding(binding1buffer, content1buffer);

		uint32_t mismatchCount = 0u;
		uint32_t firstMismatch = INVALID_UINT32;
		std::cout << "Shader object 1:\n  Expected: ";
		for (uint32_t i = 0u; i < expected.size(); ++i)
			std::cout << std::setw(4) << expected.at(i);
		std::cout << std::endl << "  Received: ";
		for (uint32_t i = 0u; i < expected.size(); ++i)
		{
			if (expected.at(i) != content1buffer.at(i))
			{
				if (INVALID_UINT32 == firstMismatch)
					firstMismatch = i;
				mismatchCount = mismatchCount + 1u;
			}
			std::cout << std::setw(4) << content1buffer.at(i);
		}
		if (0u == mismatchCount)
			std::cout << std::endl << "OK, all " << expected.size() << " matches";
		else
			std::cout << std::endl << "Wrong, first mismatch at " << firstMismatch
			<< ", mismatched values " << mismatchCount;
		std::cout << std::endl;
		result = result + make_signed(mismatchCount);
	}

	{
		const std::vector<uint32_t> expected{ c3_2, c4_2, c5_2, c0_2, c1_2, c2_2, fe_2,
												v3x_2, v3y_2, v3z_2, y_2, a_2, v2x_2, v2y_2, b_2 };
		const uint32_t content2size = lm.getBindingElementCount(binding2buffer);
		std::vector<uint32_t>	content2buffer(content2size);
		lm.readBinding(binding2buffer, content2buffer);

		uint32_t mismatchCount = 0u;
		uint32_t firstMismatch = INVALID_UINT32;
		std::cout << "Shader object 2:\n  Expected: ";
		for (uint32_t i = 0u; i < expected.size(); ++i)
			std::cout << std::setw(4) << expected.at(i);
		std::cout << std::endl << "  Received: ";
		for (uint32_t i = 0u; i < expected.size(); ++i)
		{
			if (expected.at(i) != content2buffer.at(i))
			{
				if (INVALID_UINT32 == firstMismatch)
					firstMismatch = i;
				mismatchCount = mismatchCount + 1u;
			}
			std::cout << std::setw(4) << content2buffer.at(i);
		}
		if (0u == mismatchCount)
			std::cout << std::endl << "OK, all " << expected.size() << " matches";
		else
			std::cout << std::endl << "Wrong, first mismatch at " << firstMismatch
			<< ", mismatched values " << mismatchCount;
		std::cout << std::endl;
		result = result + make_signed(mismatchCount);
	}

	{
		const std::vector<uint32_t> expected{ c3_3, c4_3, c5_3, c0_3, c1_3, c2_3, fe_3,
												v3x_3, v3y_3, v3z_3, y_3, a_3, v2x_3, v2y_3, b_3 };
		const uint32_t content3size = lm.getBindingElementCount(binding3buffer);
		std::vector<uint32_t>	content3buffer(content3size);
		lm.readBinding(binding3buffer, content3buffer);

		uint32_t mismatchCount = 0u;
		uint32_t firstMismatch = INVALID_UINT32;
		std::cout << "Shader module:\n  Expected: ";
		for (uint32_t i = 0u; i < expected.size(); ++i)
			std::cout << std::setw(4) << expected.at(i);
		std::cout << std::endl << "  Received: ";
		for (uint32_t i = 0u; i < expected.size(); ++i)
		{
			if (expected.at(i) != content3buffer.at(i))
			{
				if (INVALID_UINT32 == firstMismatch)
					firstMismatch = i;
				mismatchCount = mismatchCount + 1u;
			}
			std::cout << std::setw(4) << content3buffer.at(i);
		}
		if (0u == mismatchCount)
			std::cout << std::endl << "OK, all " << expected.size() << " matches";
		else
			std::cout << std::endl << "Wrong, first mismatch at " << firstMismatch
				<< ", mismatched values " << mismatchCount;
		std::cout << std::endl;
		result = result + make_signed(mismatchCount);
	}

	return result;
}

} // unnamed namespace

template<> struct TestRecorder<SHADER_OBJECT_COMPUTE>
{
	static bool record(TestRecord&);
};
bool TestRecorder<SHADER_OBJECT_COMPUTE>::record(TestRecord& record)
{
	record.name = "shader_object_compute";
	record.call = &prepareTests;
	return true;
}
