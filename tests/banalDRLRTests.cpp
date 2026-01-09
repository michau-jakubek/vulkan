#include "banalDRLRTests.hpp"
#include "vtfCUtils.hpp"
#include "vtfBacktrace.hpp"
#include "vtfShaderObjectCollection.hpp"
#include "vtfDSBMgr.hpp"
#include "vtfZPipeline.hpp"
#include "vtfZCommandBuffer.hpp"
#include "vtfGlfwEvents.hpp"
#include "vtfCanvas.hpp"
#include "vtfContext.hpp"
#include "vtfStructUtils.hpp"
#include "vtfCopyUtils.hpp"
#include "vtfStructUtils.hpp"
#include "vtfCommandLine.hpp"
#include "vtfPrettyPrinter.hpp"

#include <numeric>

namespace
{

using namespace vtf;

struct Params;

std::vector<uint32_t> prepareIndices (add_cref<Params> params, add_cptr<std::vector<uint32_t>> source);

struct Params
{
	add_cref<std::string>	assets;
	const std::string		testName;
	uint32_t	attachmentCount		= 4u;
	uint32_t	gapAttachmentIndex	= 42u;
	uint32_t	frameSize			= 4u;
	static inline const uint32_t	rangeAttachments = 16u;
	VecX<uint32_t, rangeAttachments>	attachmentLocations;
	VecX<uint32_t, rangeAttachments>	inputAttachmentIndices;
	bool		locationsEnable			= false;
	bool		indicesEnable			= false;
	bool		writePipeDisable		= false;
	bool		buildAlways				= false;
	bool		synchronization2		= false;
	bool		useShaderObjects		= false;
	bool		useDescriptorBuffer		= false;
	bool		printParams				= false;
	bool		printShaders			= false;
	bool		changeInputBindings		= false;
	bool		printSystemLimits		= false;
	bool		KHR						= true;
	bool		discardInFragment		= false;
	Params (add_cref<std::string> testName_, add_cref<std::string> assets_);
	OptionParser<Params> getParser ();
	void updateLoactionsAndIndices (add_cref<OptionParser<Params>> parser);
	void printParamsIf (add_ref<std::ostream> str) const;
	std::vector<uint32_t> getAttachmentLocations () const;
	std::vector<uint32_t> getInputAttachmentIndices () const;
	bool verify(add_ref<std::string> msg) const;
};
constexpr Option optionLocationsEnable			{ "--enable-locations", 0 };
constexpr Option optionIndicesEnable			{ "--enable-indices", 0 };
constexpr Option optionWritePipelineDisable		{ "--disable-write-pipeline", 0 };
constexpr Option optionChangeInputBindings		{ "--change-input-bindings", 0 };
constexpr Option optionBuildAlways				{ "--build-always", 0 };
constexpr Option optionPrintParams				{ "--print-params", 0 };
constexpr Option optionPrintShaders				{ "--print-shaders", 0 };
constexpr Option optionPrintSystemLimits		{ "--print-limits", 0 };
constexpr Option optionSynchronization2			{ "--enable-synchronization2", 0 };
constexpr Option optionUseShaderObjects			{ "--use-shader-objects", 0 };
constexpr Option optionUseDescriptorBuffer		{ "--use-descriptor-buffer", 0 };
constexpr Option optionAttachmentCount			{ "-attachment-count", 1 };
constexpr Option optionGapAttachmentIndex		{ "-gap-attachment-index", 1 };
constexpr Option optionAttachmentLocations		{ "-attachment-locations", 1 };
constexpr Option optionInputAttachmentIndices	{ "-input-attachment-indices", 1 };
Params::Params(add_cref<std::string> testName_, add_cref<std::string> assets_)
	: assets(assets_)
	, testName(testName_) { }
OptionParser<Params> Params::getParser()
{
	auto printAttachmentMap = [this](add_cref<OptionT<decltype(attachmentLocations)>> opt) -> std::string
		{
			std::ostringstream os;
			os << '[';
			for (uint32_t i = 0u; i < attachmentCount; ++i)
			{
				if (i) os << ", ";
				os << (*opt.m_default)[i];
			}
			os << ']';
			os.flush();
			return os.str();
		};

	//OptionFlags				flags(OptionFlag::None);
	OptionFlags				flagsDef(OptionFlag::PrintValueAsDefault);
	add_ref<Params>			params = *this;
	OptionParser<Params>	parser(params);

	parser.addOption(&Params::attachmentCount, optionAttachmentCount,
		"Set attachment count used by the test from range [1, 16). "
		"If greater than system possibilities then it will truncated",
		{ params.attachmentCount }, flagsDef)->setParamName("attachmentCount");

	parser.addOption(&Params::gapAttachmentIndex, optionGapAttachmentIndex,
		"Set index of gap in attachment locations and input attachment indices lists. "
		"Value outside [0..{attachments - 1}) means the the test doesn't use gap",
		{ params.gapAttachmentIndex }, flagsDef)->setParamName("gapAttachmentIndex");

	parser.addOption(&Params::attachmentLocations, optionAttachmentLocations,
		"Set attachment location list used by vkCmdSetRenderingAttachmentLocations() call. "
		"Pass in form: 0,1,2,... , indices that exceed attachment count will be ignored",
		{ params.attachmentLocations }, flagsDef, nullptr, printAttachmentMap)
		->setTypeName("uvec")->setParamName("attachmentLocations");

	parser.addOption(&Params::inputAttachmentIndices, optionInputAttachmentIndices,
		"Set attachment location list used by vkCmdSetRenderingInputAttachmentIndicesKHR() call. "
		"Pass in form: 0,1,2,... , indices that exceed attachment count will be ignored",
		{ params.inputAttachmentIndices }, flagsDef, nullptr, printAttachmentMap)
		->setTypeName("uvec")->setParamName("inputAttachmentIndices");

	parser.addOption(&Params::locationsEnable, optionLocationsEnable,
		"Use vkCmdSetRenderingAttachmentLocations() call", { false }, flagsDef)
		->setParamName("locationsEnable");

	parser.addOption(&Params::indicesEnable, optionIndicesEnable,
		"Use vkCmdSetRenderingInputAttachmentIndicesKHR() call", { false }, flagsDef)
		->setParamName("indicesEnable");

	parser.addOption(&Params::writePipeDisable, optionWritePipelineDisable,
		"Disable write pipeline so color attachments will be valid. "
		"Option " + std::string(optionLocationsEnable.name) + " makes no sense",
		{ false }, flagsDef)->setParamName("writePipelineDisable");

	parser.addOption(&Params::changeInputBindings, optionChangeInputBindings,
		"Change input bindings like should be done by "
		"vkCmdSetRenderingInputAttachmentIndicesKHR() call", { false }, flagsDef)
		->setParamName("changeInputBindings");

	parser.addOption(&Params::synchronization2, optionSynchronization2,
		"Use synchronization2 feature", { false }, flagsDef)
		->setParamName("synchronization2");

	parser.addOption(&Params::useShaderObjects, optionUseShaderObjects,
		"Use shader objects instead of classic pipelines", { false }, flagsDef)
		->setParamName("useShaderObjects");

	parser.addOption(&Params::useDescriptorBuffer, optionUseDescriptorBuffer,
		"Use descriptor buffer instead of classic descriptor set", { false }, flagsDef)
		->setParamName("useDescriptorBuffer");

	parser.addOption(&Params::buildAlways, optionBuildAlways,
		"Rebuild the shaders each time you run application", { false }, flagsDef)
		->setParamName("buildAlways");

	parser.addOption(&Params::printSystemLimits, optionPrintSystemLimits,
		"Print max attachment count to be used by the test", { false },
		OptionFlags(OptionFlag::DontPrintAsParams));

	parser.addOption(&Params::printParams, optionPrintParams,
		"Print app params to the console", { false }, OptionFlags(OptionFlag::DontPrintAsParams));

	parser.addOption(&Params::printShaders, optionPrintShaders,
		"Print shaders to the console", { false }, OptionFlags(OptionFlag::DontPrintAsParams));

	return parser;
}
void Params::updateLoactionsAndIndices (add_cref<OptionParser<Params>> parser)
{
	std::vector<uint32_t> map0 = prepareIndices(*this, nullptr);

	if (false == parser.getOptionByName(optionAttachmentLocations)->getTouched())
	{
		for (uint32_t i = 0u; i < attachmentCount; ++i)
			attachmentLocations[i] = map0[i];
	}
	else map0 = getAttachmentLocations();

	if (false == parser.getOptionByName(optionInputAttachmentIndices)->getTouched())
	{
		const std::vector<uint32_t> map1 = prepareIndices(*this, &map0);
		for (uint32_t i = 0u; i < attachmentCount; ++i)
			inputAttachmentIndices[i] = map1[i];
	}
}
void Params::printParamsIf (add_ref<std::ostream> str) const
{
	if (printParams)
	{
		Params p(*this);
		const auto parser = p.getParser();
		const std::string hdr = p.testName + " parameters";
		parser.printParams(hdr, str);
	}
}
std::vector<uint32_t> Params::getAttachmentLocations () const
{
	std::vector<uint32_t> v(attachmentCount);
	for (uint32_t i = 0u; i < attachmentCount; ++i)
		v[i] = attachmentLocations[i];
	return v;
}
std::vector<uint32_t> Params::getInputAttachmentIndices () const
{
	std::vector<uint32_t> v(attachmentCount);
	for (uint32_t i = 0u; i < attachmentCount; ++i)
		v[i] = inputAttachmentIndices[i];
	return v;
}
bool Params::verify (add_ref<std::string> msg) const
{
	for (uint32_t i = 0u; i < attachmentCount; ++i)
	{
		if (locationsEnable)
		{
			if (attachmentLocations[i] >= attachmentCount)
			{
				msg = "Attachment location must be from range [0, attachmentCount): "
					"index " + std::to_string(i);
				return false;
			}
			if (gapAttachmentIndex < attachmentCount
				&& gapAttachmentIndex == i
				&& attachmentLocations[i] != i)
			{
				msg = "If gap attachment index is less than attachment count then "
					"attachment locations at gap must be equal to gap: index " + std::to_string(i);
				return false;
			}
		}
		if (indicesEnable)
		{
			if (inputAttachmentIndices[i] >= attachmentCount)
			{
				msg = "Input attachment indices must be from range [0, attachmentCount): "
					"verify index " + std::to_string(i);
				return false;
			}
			if (gapAttachmentIndex < attachmentCount
				&& gapAttachmentIndex == i
				&& inputAttachmentIndices[i] != i)
			{
				msg = "If gap attachment index is less than attachment count then "
					"input attachment indices at gap must be equal to gap: index " + std::to_string(i);
				return false;
			}
		}

		for (uint32_t j = i + 1u; j < attachmentCount; ++j)
		{
			if (locationsEnable)
			{
				if (attachmentLocations[i] == attachmentLocations[j])
				{
					msg = "Attachment locations must be unique: "
						"index " + std::to_string(i) + " and " + std::to_string(j);
					return false;
				}
			}

			if (indicesEnable)
			{
				if (inputAttachmentIndices[i] == inputAttachmentIndices[j])
				{
					msg = "Input attachment indices must be unique: "
						"index " + std::to_string(i) + " and " + std::to_string(j);
					return false;
				}
			}
		}
	}

	return true;
}

TriLogicInt runTests (add_ref<VulkanContext> canvas, add_cref<Params> params);
TriLogicInt prepareTests (add_cref<TestRecord> record, add_cref<strings> cmdLineParams);
std::tuple<ZShaderModule, ZShaderModule, ZShaderModule,
			ZDistType<SomeZero, std::string>, ZDistType<SomeOne, std::string>>
	buildShaderModules (ZDevice device, add_cref<Params> params);
std::tuple<ZShaderObject, ZShaderObject, ZShaderObject, ZShaderObject,
	ZDistType<SomeZero, std::string>, ZDistType<SomeOne, std::string>>
	buildShaderObjects (ZDevice, ZDescriptorSetLayout, add_cref<ZPushRange<uint32_t>>, add_cref<Params>);
void genVertices (VertexInput& vi);

TriLogicInt prepareTests (add_cref<TestRecord> record, add_ref<CommandLine> cmdLine)
{
	add_cref<GlobalAppFlags> gf(getGlobalAppFlags());

	Params params(record.name, record.assets);
	OptionParser<Params> parser = params.getParser();
	parser.parse(cmdLine);
	OptionParserState state = parser.getState();

	if (state.hasHelp)
	{
		parser.printOptions(std::cout, 60);
		return {};
	}

	if (state.hasErrors || state.hasWarnings)
	{
		std::cout << state.messages.str() << std::endl;
		if (state.hasErrors) return {};
	}

	uint32_t maxColorAttachments = 0u;
	uint32_t maxPerStageDescriptorInputAttachments = 0u;

	auto onEnablingFeatures = [&](add_ref<DeviceCaps> caps)
	{
		add_cref<VkPhysicalDeviceProperties> props = deviceGetPhysicalProperties(caps.physicalDevice);
		const uint32_t major = VK_VERSION_MAJOR(props.apiVersion);
		const uint32_t minor = VK_VERSION_MINOR(props.apiVersion);
		params.KHR = major == 1u && minor < 4u;

		if (params.useShaderObjects)
		{
			caps.addUpdateFeatureIf(&VkPhysicalDeviceShaderObjectFeaturesEXT::shaderObject)
				.checkSupported("shaderObject");
			caps.addExtension(VK_EXT_SHADER_OBJECT_EXTENSION_NAME).checkSupported();
		}

		if (params.KHR)
		{
			caps.addUpdateFeatureIf(&VkPhysicalDeviceDynamicRenderingLocalReadFeatures::dynamicRenderingLocalRead)
				.checkSupported("dynamicRenderingLocalRead");
			caps.addUpdateFeatureIf(&VkPhysicalDeviceDynamicRenderingFeatures::dynamicRendering)
				.checkSupported("dynamicRendering");
			caps.addUpdateFeatureIf(&VkPhysicalDeviceExtendedDynamicStateFeaturesEXT::extendedDynamicState)
				.checkSupported("extendedDynamicState");
			caps.addUpdateFeatureIf(&VkPhysicalDeviceExtendedDynamicState2FeaturesEXT::extendedDynamicState2)
				.checkSupported("extendedDynamicState2");
			//caps.addUpdateFeatureIf(&VkPhysicalDeviceExtendedDynamicState3FeaturesEXT::)
			//	.checkSupported("extendedDynamicState");

			if (params.synchronization2)
			{
				caps.addUpdateFeatureIf(&VkPhysicalDeviceSynchronization2Features::synchronization2)
					.checkSupported("synchronization2");
				caps.addExtension(VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME).checkSupported();
			}
		}
		else
		{
			caps.addUpdateFeatureIf(&VkPhysicalDeviceVulkan14Features::dynamicRenderingLocalRead)
				.checkSupported("dynamicRenderingLocalRead");
			caps.addUpdateFeatureIf(&VkPhysicalDeviceVulkan13Features::dynamicRendering)
				.checkSupported("dynamicRendering");
			caps.addUpdateFeatureIf(&VkPhysicalDeviceVulkan13Features::maintenance4)
				.checkSupported("maintenance4");

			if (params.synchronization2)
			{
				caps.addUpdateFeatureIf(&VkPhysicalDeviceVulkan13Features::synchronization2)
					.checkSupported("synchronization2");
				caps.addExtension(VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME).checkSupported();
			}
		}
		caps.addExtension(VK_KHR_DYNAMIC_RENDERING_LOCAL_READ_EXTENSION_NAME).checkSupported();
		caps.addExtension(VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME).checkSupported();

		// Validation Error : [VUID - vkCreateDevice - ppEnabledExtensionNames - 01387]
		// vkCreateDevice() : pCreateInfo->ppEnabledExtensionNames[1]
		// Missing extension required by the device extension VK_KHR_dynamic_rendering : VK_KHR_depth_stencil_resolve.
		caps.addExtension(VK_KHR_DEPTH_STENCIL_RESOLVE_EXTENSION_NAME).checkSupported();

		// Validation Error : [VUID - vkCreateDevice - ppEnabledExtensionNames - 01387]
		// vkCreateDevice() : pCreateInfo->ppEnabledExtensionNames[2]
		// Missing extension required by the device extension VK_KHR_depth_stencil_resolve : VK_KHR_create_renderpass2.
		caps.addExtension(VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME);

		add_cref<VkPhysicalDeviceLimits> limits = deviceGetPhysicalLimits(caps.physicalDevice);
		maxColorAttachments = limits.maxColorAttachments;
		maxPerStageDescriptorInputAttachments = limits.maxPerStageDescriptorInputAttachments;
		const uint32_t attachmentsLimit = std::min(maxColorAttachments, maxPerStageDescriptorInputAttachments);
		params.attachmentCount = std::clamp(params.attachmentCount, 2u, attachmentsLimit);
	};

	VulkanContext ctx(record.name, gf.layers, {}, {}, onEnablingFeatures, Version(1,3), gf.debugPrintfEnabled);

	if (params.printSystemLimits)
	{
		std::cout << "min(maxColorAttachments (" << maxColorAttachments << "), "
			<< "maxPerStageDescriptorInputAttachments (" << maxPerStageDescriptorInputAttachments << ") = "
			<< std::min(maxColorAttachments, maxPerStageDescriptorInputAttachments) << std::endl;
		return {};
	}

	std::string verifyMessage;
	params.updateLoactionsAndIndices(parser);
	params.printParamsIf(std::cout);
	if (false == params.verify(verifyMessage))
	{
		std::cout << verifyMessage << std::endl;
		return {};
	}

	return runTests(ctx, params);
}

void genVertices (VertexInput& vi)
{
	const std::vector<Vec2> pos{ { -1, +1 }, { -1, -1 }, { +1, +1 }, { +1, -1 } };
	vi.binding(0).addAttributes(pos, pos);
}

std::pair<std::string, std::string> genShaderSources (add_cref<Params> params)
{
	std::ostringstream wFrag, wrFrag;

	wFrag << "#version 450\n";
	wrFrag << "#version 450\n";
	for (uint32_t i = 0u; i < params.attachmentCount; ++i)
	{
		if (params.gapAttachmentIndex == i)
			continue;

		wFrag << "layout(location = " << i << ") out uint color" << i << ";\n";

		wrFrag << "layout(binding = " << i << ", input_attachment_index = " << i << ") "
			"uniform usubpassInput inColor" << i << ";\n";
		/*
		rwFrag << "layout(r32ui, binding = " << binding++ << ") "
			"uniform uimage2D outColor" << i << ";\n";
		*/
		wrFrag << "layout(binding = " << (params.attachmentCount + i) << ") "
			"buffer OutColor" << i << " { uint outColor" << i << "[]; };\n";
		if (params.discardInFragment)
		{
			wrFrag << "layout(location = " << i << ") out uint output" << i << ";\n";
		}
	}
	wFrag << "layout(push_constant) uniform PC { uint width; };\n";
	wrFrag << "layout(push_constant) uniform PC { uint width; };\n";
	wFrag << "void main() {\n";
	wFrag << "  uvec2 k = uvec2(gl_FragCoord);\n";
	wrFrag << "void main() {\n";
	const double x = double((params.attachmentCount * 4 + 1) + // clearColors
		(params.frameSize * params.frameSize * params.attachmentCount));
	const uint32_t locStep = uint32_t(std::pow(10.0, std::ceil(std::log10(x))));
	for (uint32_t i = 0u; i < params.attachmentCount; ++i)
	{
		if (params.gapAttachmentIndex == i)
			continue;

		wFrag << "  color" << i << " = "
			"k.y * width + k.x + " << ((i + 1) * locStep) << ";\n";
		wrFrag << "uint color" << i << " = "
			"subpassLoad(inColor" << i << ").x + " << ((i + 1) * locStep * 10) << ";\n";
		//rwFrag << "imageStore(outColor" << i << ", ivec2(gl_FragCoord.xy), uvec4(color" << i << ", 0, 0, 0));\n";
		wrFrag << "outColor" << i << "[uint(gl_FragCoord.y) * width + uint(gl_FragCoord.x)] = color" << i << ";\n";
		if (params.discardInFragment)
		{
			wrFrag << "discard;\n";
		}
	}
	wFrag << "}\n";
	wrFrag << "}\n";
	wFrag.flush();
	wrFrag.flush();

	return { wFrag.str(), wrFrag.str() };
}

std::tuple<ZShaderObject, ZShaderObject, ZShaderObject, ZShaderObject,
	ZDistType<SomeZero, std::string>, ZDistType<SomeOne, std::string>>
buildShaderObjects (
	ZDevice	device,
	ZDescriptorSetLayout dsLayout,
	add_cref<ZPushRange<uint32_t>> push_const,
	add_cref<Params> params)
{
	using Link = ShaderObjectCollection::ShaderLink;

	ZShaderObject so1;
	std::cout << so1.use_count() << std::endl;
	ZShaderObject so2 = so1;
	std::cout << so2.use_count() << std::endl;

	std::string wFrag, wrFrag;
	std::tie(wFrag, wrFrag) = genShaderSources(params);

	ShaderObjectCollection coll(device, params.assets);
	Link linkWVert = coll.addFromFile(VK_SHADER_STAGE_VERTEX_BIT, "shader.vert");
	Link linkWRVert = coll.addFromFile(VK_SHADER_STAGE_VERTEX_BIT, "shader.vert");
	Link linkWFrag = coll.addFromText(VK_SHADER_STAGE_FRAGMENT_BIT, wFrag, linkWVert);
	Link linkWRFrag = coll.addFromText(VK_SHADER_STAGE_FRAGMENT_BIT, wrFrag, linkWRVert);
	add_cref<GlobalAppFlags> gf = getGlobalAppFlags();
	coll.updateShaders({ linkWVert, linkWRVert, linkWFrag, linkWRFrag },
		params.buildAlways, gf.spirvValidate, gf.genSpirvDisassembly,
		gf.vulkanVer, gf.spirvVer);
	coll.updateShaders({ linkWVert, linkWRVert, linkWFrag, linkWRFrag }, push_const);
	coll.updateShaders({ linkWRVert, linkWRFrag }, { dsLayout });
	coll.buildAndVerify();
	return
	{
		coll.getShader(linkWVert),
		coll.getShader(linkWFrag),
		coll.getShader(linkWRVert),
		coll.getShader(linkWRFrag),
		coll.getShaderFile(VK_SHADER_STAGE_FRAGMENT_BIT, 0u),
		coll.getShaderFile(VK_SHADER_STAGE_FRAGMENT_BIT, 1u),
	};
}

std::tuple<ZShaderModule, ZShaderModule, ZShaderModule,
	ZDistType<SomeZero, std::string>, ZDistType<SomeOne, std::string>>
	buildShaderModules (ZDevice device, add_cref<Params> params)
{
	std::string wFrag, wrFrag;
	std::tie(wFrag, wrFrag) = genShaderSources(params);

	ProgramCollection progs(device, params.assets);
	progs.addFromFile(VK_SHADER_STAGE_VERTEX_BIT, "shader.vert");
	progs.addFromText(VK_SHADER_STAGE_FRAGMENT_BIT, wFrag);
	progs.addFromText(VK_SHADER_STAGE_FRAGMENT_BIT, wrFrag);
	progs.buildAndVerify(params.buildAlways);

	return
	{
		progs.getShader(VK_SHADER_STAGE_VERTEX_BIT),
		progs.getShader(VK_SHADER_STAGE_FRAGMENT_BIT, 0u),
		progs.getShader(VK_SHADER_STAGE_FRAGMENT_BIT, 1u),
		progs.getShaderFile(VK_SHADER_STAGE_FRAGMENT_BIT, 0u),
		progs.getShaderFile(VK_SHADER_STAGE_FRAGMENT_BIT, 1u),
	};
}

void printShaders (add_ref<std::ostream> str, add_cref<std::string> wFragFile, add_cref<std::string> wrFragFile)
{
	std::vector<char> buffer;
	add_cref<GlobalAppFlags> gf = getGlobalAppFlags();
	const fs::path tmpPath = std::strlen(gf.tmpDir) ? fs::path(gf.tmpDir) : fs::temp_directory_path();

	if (auto file = tmpPath / wFragFile; fs::exists(file))
	{
		readFile(file, buffer);
		str << "Write fragment shader source:\n"
			   "-----------------------------\n";
		std::copy(buffer.begin(), buffer.end(), std::ostream_iterator<char>(str));
	}
	else str << "Unable to read " << std::quoted(wFragFile) << std::endl;

	if (auto file = tmpPath / wrFragFile; fs::exists(file))
	{
		readFile(file, buffer);
		str << "Write-read fragment shader source:\n"
			   "----------------------------------\n";
		std::copy(buffer.begin(), buffer.end(), std::ostream_iterator<char>(str));
	}
	else str << "Unable to read " << std::quoted(wrFragFile) << std::endl;
}

std::vector<uint32_t> prepareIndices (add_cref<Params> params, add_cptr<std::vector<uint32_t>> source)
{
	auto prepare = [&](add_ref<std::vector<uint32_t>> indices, uint32_t elemCount, bool preinitialized)
	{
		if (false == preinitialized)
		{
			std::iota(indices.begin(), indices.end(), 0u);
		}

		for (uint32_t i = 0u; i < elemCount; ++i)
		{
			if (params.gapAttachmentIndex == i)
				continue;

			const uint32_t k = indices[i];
			indices[i] = indices[elemCount - 1u];
			indices[elemCount - 1u] = k;
		}
	};

	if (source == nullptr)
	{
		std::vector<uint32_t> indices(params.attachmentCount);
		prepare(indices, params.attachmentCount, false);
		return indices;
	}

	std::vector<uint32_t> indices = *source;
	prepare(indices, uint32_t(indices.size()), true);

	return indices;
}

void printMap (std::ostream& str, add_cref<std::string> desc,
				add_cref<std::vector<uint32_t>> m, const uint32_t select)
{
	str << desc << " [";
	for (uint32_t i = 0u; i < m.size(); ++i)
	{
		if (i)
			str << ", ";
		if (select == i) str << '(';
		str << m[i];
		if (select == i) str << ')';
	}
	str << ']' << std::endl;
};

void printAttachment (std::ostream& str, add_cref<std::string> desc, uint32_t index,
	add_cref<std::vector<uint32_t>> buffer, add_cref<Params> params)
{
	str << desc << " (" << index << ')' << std::endl;
	for (uint32_t Y = 0; Y < params.frameSize; ++Y)
	{
		for (uint32_t X = 0; X < params.frameSize; ++X)
		{
			const uint32_t c = buffer[Y * params.frameSize + X];
			if (c > std::numeric_limits<uint32_t>::max() - 100)
				str << (std::numeric_limits<uint32_t>::max() - c);
			else
				str << c;
			str << ' ';
		}
		str << std::endl;
	}
}

void printAttachment (std::ostream& str, add_cref<std::string> desc, uint32_t index,
	ZBuffer buffer, add_cref<Params> params)
{
	str << desc << " (" << index << ')' << std::endl;
	BufferTexelAccess<uint32_t> a(buffer, params.frameSize, params.frameSize);
	for (uint32_t Y = 0; Y < params.frameSize; ++Y)
	{
		for (uint32_t X = 0; X < params.frameSize; ++X)
		{
			const uint32_t c = a.at(X, Y);
			if (c > std::numeric_limits<uint32_t>::max() - 100)
				str << (std::numeric_limits<uint32_t>::max() - c);
			else
				str << c;
			str << ' ';
		}
		str << std::endl;
	}
}

bool compareAttachments (ZBuffer buffer0, add_cref<std::vector<uint32_t>> buffer1, add_cref<Params> params)
{
	ASSERTION(bufferGetElementCount<uint32_t>(buffer0) == buffer1.size());
	const BufferTexelAccess<uint32_t> a(buffer0, params.frameSize, params.frameSize);
	const uint32_t renderSize = params.frameSize * params.frameSize;
	for (uint32_t i = 0; i < renderSize; ++i)
		if (a.at(i, 0u, 0u) != buffer1[i])
			return false;
	return true;
}

using PixelType = uint32_t;
UNUSED UVec2 verifyWriteImages(
	const uint32_t renderSize,
	add_cref<std::vector<std::vector<PixelType>>> writeImages,
	const uint32_t gapAttachmentIndex,
	add_cref<std::vector<PixelType>> clearColors,
	add_cref<std::vector<uint32_t>> writeAttachmentLocations,
	const bool enableWriteAttachmentLocations);
UNUSED UVec2 verifyReadImages(
	const uint32_t renderSize,
	add_cref<std::vector<std::vector<PixelType>>> writeImages,
	add_cref<std::vector<std::vector<PixelType>>> readImages,
	const uint32_t gapAttachmentIndex,
	add_cref<std::vector<uint32_t>> readAttachmentLocations,
	add_cref<std::vector<uint32_t>> readInputAttachmentIndices,
	const bool enableReadAttachmentLocations,
	const bool enableReadInputAttachmentIndices);
std::vector<std::vector<std::vector<PixelType>>>
offlineGenerator(
	const uint32_t renderSize,
	const uint32_t colorAttachmentCount,
	const uint32_t gapAttachmentIndex,
	add_cref<std::vector<PixelType>> writeClearColors,
	add_cref<std::vector<PixelType>> readClearColors,
	add_cref<std::vector<uint32_t>> writeAttachmentLocations,
	add_cref<std::vector<uint32_t>> readAttachmentLocations,
	add_cref<std::vector<uint32_t>> readInputAttachmentIndices,
	const bool enableWritePipeline,
	const bool enableWriteAttachmentLocations,
	const bool enableReadAttachmentLocations,
	const bool enableReadInputAttachmentIndices);

TriLogicInt runTests (add_ref<VulkanContext> ctx, add_cref<Params> params)
{
	add_cref<ZDeviceInterface> di = ctx.device.getInterface(); UNREF(di);

	VertexInput	vertexInput(ctx.device);
	genVertices(vertexInput);

	auto [vert, wFrag, wrFrag, wFragFile, wrFragFile] = buildShaderModules(ctx.device, params);

	ZCommandPool				cmdPool	= createCommandPool(ctx.device, ctx.graphicsQueue);
	LayoutManager				lm0		(ctx.device, cmdPool);
	LayoutManager				lm1		(ctx.device, cmdPool);

	std::vector<ZImage>			inputImages(params.attachmentCount);
	std::vector<ZImageView>		inputAttachments(params.attachmentCount);
	std::vector<ZBuffer>		inputBuffers(params.attachmentCount);
	std::vector<ZBuffer>		outputBuffers(params.attachmentCount);

	std::vector<gpp::Attachment>	inputRenderingAttachments(params.attachmentCount);
	std::vector<VkClearValue>		clearColors(params.attachmentCount);
	std::vector<gpp::BlendAttachmentState> readPipelineBlendStates(params.attachmentCount);

	std::vector<ZImageMemoryBarrier>	transitGeneralInputBarriers(params.attachmentCount);
	std::vector<ZImageMemoryBarrier>	transitLocalReadBarriers(params.attachmentCount);
	std::vector<ZImageMemoryBarrier2>	transitLocalReadBarriers2(params.attachmentCount);

	using S = ZBarrierConstants::Stage;
	using A = ZBarrierConstants::Access;
	ZMemoryBarrier2	memoryLocalReadBarrier2	(
		A::SHADER_WRITE_BIT, S::FRAGMENT_SHADER_BIT,
		A::INPUT_ATTACHMENT_READ_BIT, S::FRAGMENT_SHADER_BIT);
	ZMemoryBarrier	memoryLocalReadBarrier	(VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_INPUT_ATTACHMENT_READ_BIT);
	UNREF(memoryLocalReadBarrier2);
	UNREF(memoryLocalReadBarrier);

	// Validation Error : [VUID - vkCmdSetRenderingAttachmentLocations - pLocationInfo - 09510] | MessageID = 0xd264d82f
	// vkCmdSetRenderingAttachmentLocations() : pLocationInfo->colorAttachmentCount(2)
	// is not equal to count specified in VkRenderingInfo(4).
	// The Vulkan spec states : pLocationInfo->colorAttachmentCount must be equal to the
	// value of VkRenderingInfo::colorAttachmentCount used to begin the current render pass instance
	// (https ://vulkan.lunarg.com/doc/view/1.4.321.1/windows/antora/spec/latest/chapters/interfaces.html#VUID-vkCmdSetRenderingAttachmentLocations-pLocationInfo-09510)
	//
	// Validation Error : [VUID - vkCmdDraw - None - 09548] | MessageID = 0x6888c0e8
	// vkCmdDraw() : The pipeline VkRenderingAttachmentLocationInfo::colorAttachmentCount is 4
	// but vkCmdSetRenderingAttachmentLocations last set colorAttachmentCount to 2.
	// The Vulkan spec states : If the current render pass was begun with vkCmdBeginRendering,
	// and there is no shader object bound to any graphics stage, the value of each element of
	// VkRenderingAttachmentLocationInfo::pColorAttachmentLocations set by vkCmdSetRenderingAttachmentLocations
	// must match the value set for the corresponding element in the bound pipeline
	// (https ://vulkan.lunarg.com/doc/view/1.4.321.1/windows/antora/spec/latest/chapters/drawing.html#VUID-vkCmdDraw-None-09548)

	std::vector<uint32_t> renderingNullAttachmentLocations(params.attachmentCount);
	std::iota(renderingNullAttachmentLocations.begin(), renderingNullAttachmentLocations.end(), 0u);

	std::vector<uint32_t>	renderingAttachmentLocations = params.getAttachmentLocations();

	std::vector<uint32_t>	renderingInputAttachmentIndices = params.getInputAttachmentIndices();

	const bool verbose = getGlobalAppFlags().verbose != 0u;
	const bool renderingAttachmentLocationsEnabled = params.locationsEnable;
	const bool renderingInputAttachmentIndicesEnabled = params.indicesEnable;

	// If vkCmdPipelineBarrier is called within a render pass instance, the oldLayout and newLayout
	// members of any image memory barrier included in this command must be equal
	//
	// If vkCmdPipelineBarrier is called within a render pass instance started with vkCmdBeginRendering,
	// and the image member of any image memory barrier is used as an attachment in the current render
	// pass instance, it must be in the VK_IMAGE_LAYOUT_RENDERING_LOCAL_READ or VK_IMAGE_LAYOUT_GENERAL layout

	const VkImageLayout inputAttachmentsLayout = params.writePipeDisable
													? VK_IMAGE_LAYOUT_GENERAL
													: VK_IMAGE_LAYOUT_RENDERING_LOCAL_READ;
	for (uint32_t i = 0u; i < params.attachmentCount; ++i)
	{		
		clearColors[i] = makeClearColor(UVec4(i + 1u, i + 2u, i + 3u, i + 4u));

		auto blendAttachmentState = gpp::defaultBlendAttachmentState;
		blendAttachmentState.colorWriteMask = 0u;

		if (params.gapAttachmentIndex == i)
		{
			inputRenderingAttachments[i] = gpp::Attachment(ZImageView(), gpp::AttachmentDesc::Color);
			readPipelineBlendStates[i] = gpp::BlendAttachmentState(std::make_pair(i, blendAttachmentState));
			continue;
		}

		inputImages[i] = ctx.createColorImage2D(VK_FORMAT_R32_UINT, params.frameSize, params.frameSize);
		inputBuffers[i] = createBuffer(inputImages[i]);
		inputBuffers[i].name("inputBuffer " + std::to_string(i));
		inputBuffers[i].verbose(verbose);
		inputAttachments[i] = createImageView(inputImages[i]);

		outputBuffers[i] = createBuffer(inputImages[i]);

		inputRenderingAttachments[i] = gpp::Attachment(inputAttachments[i], gpp::AttachmentDesc::Color);
		readPipelineBlendStates[i] = gpp::BlendAttachmentState(std::make_pair(i, blendAttachmentState));

		transitGeneralInputBarriers[i] = ZImageMemoryBarrier(inputImages[i],
											VK_ACCESS_NONE, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
											inputAttachmentsLayout);
		// transitLocalReadBarriers not used if write pipeline is disabled
		transitLocalReadBarriers[i] = ZImageMemoryBarrier(inputImages[i],
											VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_INPUT_ATTACHMENT_READ_BIT,
											VK_IMAGE_LAYOUT_RENDERING_LOCAL_READ);
		transitLocalReadBarriers2[i] = ZImageMemoryBarrier2(inputImages[i],
											A::COLOR_ATTACHMENT_WRITE_BIT, S::COLOR_ATTACHMENT_OUTPUT_BIT,
											A::INPUT_ATTACHMENT_READ_BIT, S::FRAGMENT_SHADER_BIT,
											VK_IMAGE_LAYOUT_RENDERING_LOCAL_READ);
	}

	std::vector<uint32_t> inputBindingMap = renderingNullAttachmentLocations;
	if (params.changeInputBindings)
	{
		for (uint32_t i = 0u; i < data_count(inputBindingMap); ++i)
		{
			inputBindingMap[renderingInputAttachmentIndices[i]] = i;
		}
	}

	for (uint32_t i = 0; i < params.attachmentCount; ++i)
	{
		if (params.gapAttachmentIndex == i) continue;

		lm1.addBinding(i, inputAttachments[inputBindingMap[i]], VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
			inputAttachmentsLayout, shaderGetStage(wrFrag));

		const uint32_t bufferBinding = lm1.addBinding(
			(params.attachmentCount + i),
			outputBuffers[i], VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, shaderGetStage(wrFrag));
		lm1.fillBinding(bufferBinding, clearColors[i].color.uint32[0]);
		outputBuffers[i].name("outputBuffer " + std::to_string(i) + ", binding " + std::to_string(bufferBinding));
		outputBuffers[i].verbose(verbose);
	}

	ZPushRange<uint32_t>		push_const	(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);

	// Validation Error : [VUID - vkCmdDraw - colorAttachmentCount - 06179] | MessageID = 0x94a972a0
	// vkCmdDraw() : Currently bound pipeline VkPipeline VkPipelineRenderingCreateInfo::colorAttachmentCount(2)
	// must be equal to VkRenderingInfo::colorAttachmentCount(4).
	// The Vulkan spec states : If the dynamicRenderingUnusedAttachments feature is not enabled and the current
	// render pass instance was begun with vkCmdBeginRendering, the bound graphics pipeline must have been created
	// with a VkPipelineRenderingCreateInfo::colorAttachmentCount equal to VkRenderingInfo::colorAttachmentCount
	// (https ://vulkan.lunarg.com/doc/view/1.4.321.1/windows/antora/spec/latest/chapters/drawing.html#VUID-vkCmdDraw-colorAttachmentCount-06179)

	add_cptr<std::vector<uint32_t>> writePipelineAttachmentLocations =
										renderingAttachmentLocationsEnabled
											? &renderingAttachmentLocations
											: nullptr;
	ZPipelineLayout				wLayout		= params.writePipeDisable ? ZPipelineLayout()
											  : lm0.createPipelineLayout(push_const);
	ZPipeline					wPipeline	= params.writePipeDisable ? ZPipeline()
											  : createGraphicsPipeline(wLayout, vert, wFrag, vertexInput,
													inputRenderingAttachments,
													gpp::RenderingAttachmentLocations(writePipelineAttachmentLocations),
													gpp::SubpassIndex(0),
													makeExtent2D(params.frameSize, params.frameSize),
													VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP);

	add_cptr<std::vector<uint32_t>> readPipelineAttachmentLocations =
										renderingAttachmentLocationsEnabled
											? &renderingNullAttachmentLocations
											: nullptr;
	add_cptr<std::vector<uint32_t>> readPipelineInputAttachmentIndices =
										renderingInputAttachmentIndicesEnabled
										? &renderingInputAttachmentIndices
										: nullptr;
	ZDescriptorSetLayout		dsLayout	= lm1.createDescriptorSetLayout();
	ZPipelineLayout				rLayout		= lm1.createPipelineLayout({ dsLayout }, push_const);
	ZPipeline					rPipeline	= createGraphicsPipeline(rLayout, vert, wrFrag, vertexInput,
													inputRenderingAttachments,
													gpp::RenderingAttachmentLocations(readPipelineAttachmentLocations),
													gpp::RenderingInpuAttachmentIndices(readPipelineInputAttachmentIndices),
													gpp::SubpassIndex(INVALID_UINT32), readPipelineBlendStates,
													makeExtent2D(params.frameSize, params.frameSize),
													VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP);

	ZShaderObject shoWVert, shoWFrag, shoWRVert, shoWRFrag;
	if (params.useShaderObjects)
	{
		std::tie(shoWVert, shoWFrag, shoWRVert, shoWRFrag,
					wFragFile, wrFragFile) = buildShaderObjects(ctx.device, dsLayout, push_const, params);
		ASSERTMSG(shoWVert.has_handle(), "Vertex shader must have valid handle");
		ASSERTMSG(shoWRVert.has_handle(), "Fragment shader must have valid handle");
	}

	if (params.printShaders)
	{
		printShaders(std::cout, wFragFile, wrFragFile);
	}

	{
		OneShotCommandBuffer	cmd(cmdPool);

		commandBufferBindVertexBuffers(cmd, vertexInput);

		commandBufferPipelineBarrierVecs(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, transitGeneralInputBarriers);
		commandBufferBindDescriptorSets(cmd, rLayout, VK_PIPELINE_BIND_POINT_GRAPHICS);

		if (params.useShaderObjects)
		{
			commandBufferSetDefaultDynamicStates(cmd, vertexInput, makeViewport(params.frameSize, params.frameSize),
												params.attachmentCount);
			di.vkCmdSetPrimitiveTopologyEXT(**cmd, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP);
		}

		commandBufferBeginRendering(cmd, params.frameSize, params.frameSize, { &inputRenderingAttachments }, clearColors);

		if (false == params.writePipeDisable)
		{
			if (params.useShaderObjects)
				commandBufferBindShaders(cmd, { shoWVert, shoWFrag });
			else commandBufferBindPipeline(cmd, wPipeline, false);
			commandBufferPushConstants(ZCommandBuffer(cmd), wLayout, params.frameSize);

			if (renderingAttachmentLocationsEnabled)
			{
				if (verbose) {
					printMap(std::cout, '[' + params.testName + "] vkCmdSetRenderingAttachmentLocations()",
										renderingAttachmentLocations, params.gapAttachmentIndex);
				}
				//(*di.vkCmdSetRenderingAttachmentLocations)(**cmd, &rali);
				commandBufferSetRenderingAttachmentLocations(*cmd, renderingAttachmentLocations, params.KHR);
			}
			vkCmdDraw(**cmd, vertexInput.getVertexCount(0), 1u, 0u, 0u);

			commandBufferPipelineBarrierVecs(cmd, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
				VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, transitLocalReadBarriers);
		}

		if (params.useShaderObjects)
		{
			commandBufferBindDescriptorSets(cmd, rLayout, VK_PIPELINE_BIND_POINT_GRAPHICS);
			commandBufferBindShaders(cmd, { shoWRVert, shoWRFrag });
		}
		else commandBufferBindPipeline(cmd, rPipeline, false);
		commandBufferPushConstants(cmd, rLayout, params.frameSize);

		if (params.useShaderObjects)
		{
			VkColorComponentFlags blendWriteMasks[Params::rangeAttachments];
			std::fill_n(std::begin(blendWriteMasks), params.attachmentCount, 0u);
			di.vkCmdSetColorWriteMaskEXT(**cmd, 0u, params.attachmentCount, blendWriteMasks);
		}

		if ((false == params.writePipeDisable) && renderingAttachmentLocationsEnabled)
		{
			if (verbose) {
				printMap(std::cout, '[' + params.testName + "] vkCmdSetRenderingAttachmentLocations()",
									renderingNullAttachmentLocations, params.gapAttachmentIndex);
			}
			//(*di.vkCmdSetRenderingAttachmentLocations)(**cmd, &raliNull);
			commandBufferSetRenderingAttachmentLocations(cmd, renderingNullAttachmentLocations, params.KHR);
		}
		if (renderingInputAttachmentIndicesEnabled)
		{
			if (verbose) {
				printMap(std::cout, '[' + params.testName + "] vkCmdSetRenderingInputAttachmentIndices()",
									renderingInputAttachmentIndices, params.gapAttachmentIndex);
			}
			//(*di.vkCmdSetRenderingInputAttachmentIndices)(**cmd, &riai);
			commandBuffervSetRenderingInputAttachmentIndices(cmd, renderingInputAttachmentIndices, {}, {}, params.KHR);
		}
		vkCmdDraw(**cmd, vertexInput.getVertexCount(0), 1u, 0u, 0u);
		commandBufferEndRendering(cmd);
	}

	{
		OneShotCommandBuffer	cmd(cmdPool);
		for (uint32_t i = 0u; i < params.attachmentCount; ++i)
		{
			if (params.gapAttachmentIndex == i)
				continue;

			imageCopyToBuffer(cmd, inputImages[i], inputBuffers[i],
				VK_ACCESS_NONE, VK_ACCESS_NONE, VK_ACCESS_NONE, VK_ACCESS_NONE,
				VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
				VK_IMAGE_LAYOUT_GENERAL);
		}
	}

	PrettyPrinter printer;
	std::cout << "------\n";
	if (params.changeInputBindings)
	{
		printMap(std::cout, "Changed input binding map", inputBindingMap, params.gapAttachmentIndex);
	}
	printMap(std::cout, "renderingAttachmentLocations",
				renderingAttachmentLocations, params.gapAttachmentIndex);
	printMap(std::cout, "renderingInputAttachmentIndices",
				renderingInputAttachmentIndices, params.gapAttachmentIndex);
	for (uint32_t i = 0u; i < params.attachmentCount; ++i)
	{
		if (params.gapAttachmentIndex == i)
			continue;

		printAttachment(printer.getCursor(0), "Input attachment", i, inputBuffers[i], params);
		printAttachment(printer.getCursor(1), "Color attachment", i, outputBuffers[i], params);
	}

	std::vector<uint32_t> writeClearColors(params.attachmentCount);
	std::vector<uint32_t> readClearColors(params.attachmentCount);
	for (uint32_t i = 0u; i < params.attachmentCount; ++i)
	{
		writeClearColors[i] = clearColors[i].color.uint32[0];
		readClearColors[i] = clearColors[i].color.uint32[0];
	}
	std::vector<std::vector<std::vector<PixelType>>> ref =
		offlineGenerator(params.frameSize, params.attachmentCount, params.gapAttachmentIndex,
			writeClearColors, readClearColors,
			std::vector<uint32_t>(
				renderingAttachmentLocations.data(),
				renderingAttachmentLocations.data() + params.attachmentCount),
			{}, 
			std::vector<uint32_t>(
				renderingInputAttachmentIndices.data(),
				renderingInputAttachmentIndices.data() + params.attachmentCount),
			params.writePipeDisable == false,
			renderingAttachmentLocationsEnabled,
			false, (renderingInputAttachmentIndicesEnabled || params.changeInputBindings));

	bool attachmentStatus = true;
	bool allAttachmentStatus = true;

	for (uint32_t i = 0u; i < params.attachmentCount; ++i)
	{
		if (params.gapAttachmentIndex == i)
			continue;

		printAttachment(printer.getCursor(2), "Expected Input", i, ref[0][i], params);
		printAttachment(printer.getCursor(3), "Expected Color", i, ref[1][i], params);

		printer.getCursor(4) << "Compare Input (" << i << ')' << std::endl;
		attachmentStatus = compareAttachments(inputBuffers[i], ref[0][i], params);
		allAttachmentStatus &= attachmentStatus;
		printer.getCursor(4) << std::endl << (attachmentStatus ? "      OK" : "    Mismatch") << std::endl;

		printer.getCursor(5) << "Compare Color (" << i << ')' << std::endl;
		attachmentStatus = compareAttachments(outputBuffers[i], ref[1][i], params);
		allAttachmentStatus &= attachmentStatus;
		printer.getCursor(5) << std::endl << (attachmentStatus ? "      OK" : "    Mismatch") << std::endl;
		for (uint32_t j = 2; j < params.frameSize; ++j)
		{
			printer.getCursor(4) << std::endl;
			printer.getCursor(5) << std::endl;
		}
	}

	printer.merge({ 0, 1, 2, 3, 4, 5 }, std::cout);

	std::cout << "Done..." << std::endl;

	return allAttachmentStatus ? 0 : 1;
}

using PixelType = uint32_t;
UVec2 verifyWriteImages(
	const uint32_t renderSize,
	add_cref<std::vector<std::vector<PixelType>>> writeImages,
	const uint32_t gapAttachmentIndex,
	add_cref<std::vector<PixelType>> clearColors,
	add_cref<std::vector<uint32_t>> writeAttachmentLocations,
	const bool enableWriteAttachmentLocations)
{
	uint32_t mismatchMargin = 0u;
	uint32_t mismatchColor = 0u;
	const uint32_t colorAttachmentCount = data_count(writeImages);

	const double x = double((colorAttachmentCount * 4 + 1) + // clearColors
		(renderSize * renderSize * colorAttachmentCount));
	const uint32_t locStep = uint32_t(std::pow(10.0, std::ceil(std::log10(x))));

	for (uint32_t loc = 0u; loc < colorAttachmentCount; ++loc)
	{
		if (gapAttachmentIndex == loc)
			continue;

		const uint32_t A = enableWriteAttachmentLocations ? writeAttachmentLocations[loc] : loc;

		for (uint32_t Y = 0u; Y < renderSize; ++Y)
		for (uint32_t X = 0u; X < renderSize; ++X)
		{
			const uint32_t K = Y * renderSize + X;
			if (X >= (renderSize / 4) && X < ((3 * renderSize) / 4)
				&& Y >= (renderSize / 4) && Y < ((3 * renderSize) / 4))
			{
				//wFrag << "  color" << i << " = "
				//	<< "k.y * width + k.x + " << ((i + 1) * locStep) << ";\n";
				const PixelType ref = K + ((loc + 1u) * locStep);;
				if (writeImages[A][K] != ref)
					++mismatchColor;
			}
			else
			{
				if (writeImages[loc][K] != clearColors[loc])
					++mismatchMargin;
			}
		}
	}

	return UVec2(mismatchColor, mismatchMargin);
}

UVec2 verifyReadImages(
	const uint32_t renderSize,
	add_cref<std::vector<std::vector<PixelType>>> writeImages,
	add_cref<std::vector<std::vector<PixelType>>> readImages,
	const uint32_t gapAttachmentIndex,
	add_cref<std::vector<uint32_t>> readAttachmentLocations,
	add_cref<std::vector<uint32_t>> readInputAttachmentIndices,
	const bool enableReadAttachmentLocations,
	const bool enableReadInputAttachmentIndices)
{
	uint32_t mismatchMargin = 0u;
	uint32_t mismatchColor = 0u;
	const uint32_t colorAttachmentCount = data_count(writeImages);

	const double x = double((colorAttachmentCount * 4 + 1) + // clearColors
		(renderSize * renderSize * colorAttachmentCount));
	const uint32_t locStep = uint32_t(std::pow(10.0, std::ceil(std::log10(x))));

	for (uint32_t loc = 0u; loc < colorAttachmentCount; ++loc)
	{
		if (gapAttachmentIndex == loc)
			continue;

		const uint32_t I = enableReadInputAttachmentIndices ? readInputAttachmentIndices[loc] : loc;
		const uint32_t A = enableReadAttachmentLocations ? readAttachmentLocations[loc] : loc;

		for (uint32_t Y = 0u; Y < renderSize; ++Y)
		for (uint32_t X = 0u; X < renderSize; ++X)
		{
			const uint32_t K = Y * renderSize + X;

			if (X >= (renderSize / 4) && X < ((3 * renderSize) / 4)
				&& Y >= (renderSize / 4) && Y < ((3 * renderSize) / 4))
			{
				// rwFrag << "outColor" << i << " = "
				//	<< "subpassLoad(inColor" << i << ").x + " << ((i + 1) * locStep * 10) << ";\n";
				const PixelType color = writeImages[I][K] + ((loc + 1u) * locStep * 10u);
				if (readImages[A][K] != color)
					++mismatchColor;
			}
			else
			{
				if (readImages[loc][K] != writeImages[loc][K])
					++mismatchMargin;
			}
		}
	}

	return UVec2(mismatchColor, mismatchMargin);
}

std::vector<std::vector<std::vector<PixelType>>>
offlineGenerator (
	const uint32_t renderSize,
	const uint32_t colorAttachmentCount,
	const uint32_t gapAttachmentIndex,
	add_cref<std::vector<PixelType>> writeClearColors,
	add_cref<std::vector<PixelType>> readClearColors,
	add_cref<std::vector<uint32_t>> writeAttachmentLocations,
	add_cref<std::vector<uint32_t>> readAttachmentLocations,
	add_cref<std::vector<uint32_t>> readInputAttachmentIndices,
	const bool enableWritePipeline,
	const bool enableWriteAttachmentLocations,
	const bool enableReadAttachmentLocations,
	const bool enableReadInputAttachmentIndices)
{
	std::vector<std::vector<PixelType>> writeImages(colorAttachmentCount);
	std::vector<std::vector<PixelType>> readImages(colorAttachmentCount);
	for (uint32_t a = 0u; a < colorAttachmentCount; ++a)
	{
		if (gapAttachmentIndex != a)
		{
			writeImages[a].resize(renderSize * renderSize);
			readImages[a].resize(renderSize * renderSize);
		}
	}

	const double x = double((colorAttachmentCount * 4 + 1) + // clearColors
						(renderSize * renderSize * colorAttachmentCount));
	const uint32_t locStep = uint32_t(std::pow(10.0, std::ceil(std::log10(x))));

	// Generate input images
	for (uint32_t loc = 0u; loc < colorAttachmentCount; ++loc)
	{
		if (gapAttachmentIndex == loc)
			continue;

		const uint32_t data = enableWriteAttachmentLocations ? writeAttachmentLocations[loc] : loc;

		for (uint32_t Y = 0u; Y < renderSize; ++Y)
		for (uint32_t X = 0u; X < renderSize; ++X)
		{
			const uint32_t K = Y * renderSize + X;
			if (X >= (renderSize / 4) && X < ((3 * renderSize) / 4)
				&& Y >= (renderSize / 4) && Y < ((3 * renderSize) / 4))
			{
				//wFrag << "  color" << i << " = "
				//	<< "k.y * width + k.x + " << ((i + 1) * locStep) << ";\n";
				if (enableWritePipeline)
					writeImages[loc][K] = K + ((data + 1u) * locStep);
				else writeImages[loc][K] = writeClearColors[loc];
			}
			else
			{
				writeImages[loc][K] = writeClearColors[loc];
			}
		}
	}

	std::vector<uint32_t> invIndicesMap(readInputAttachmentIndices.size());
	for (uint32_t k = 0u; k < colorAttachmentCount; ++k)
	{
		invIndicesMap[readInputAttachmentIndices[k]] = k;
	}

	for (uint32_t loc = 0u; loc < colorAttachmentCount; ++loc)
	{
		if (gapAttachmentIndex == loc)
			continue;

		const uint32_t I = enableReadInputAttachmentIndices ? invIndicesMap[loc] : loc;
		const uint32_t data = enableReadAttachmentLocations ? readAttachmentLocations[loc] : loc;

		for (uint32_t Y = 0u; Y < renderSize; ++Y)
		for (uint32_t X = 0u; X < renderSize; ++X)
		{
			const uint32_t K = Y * renderSize + X;

			if (X >= (renderSize / 4) && X < ((3 * renderSize) / 4)
				&& Y >= (renderSize / 4) && Y < ((3 * renderSize) / 4))
			{
				// rwFrag << "outColor" << i << " = "
				//	<< "subpassLoad(inColor" << i << ").x + " << ((i + 1) * locStep * 10) << ";\n";
				const uint32_t color = writeImages[I][K] + ((data + 1u) * locStep * 10u);
				readImages[loc][K] = color;
			}
			else
			{
				readImages[loc][K] = readClearColors[loc];
			}
		}
	}

	return { writeImages, readImages };
}

} // unnamed namespace

template<> struct TestRecorder<BANAL_DRLR>
{
	static bool record(TestRecord&);
};
bool TestRecorder<BANAL_DRLR>::record(TestRecord& record)
{
	record.name = "banal_drlr";
	record.call = &prepareTests;
	return true;
}
