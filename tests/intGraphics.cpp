#include "intGraphics.hpp"
#include "vtfOptionParser.hpp"
#include "vtfCanvas.hpp"
#include "vtfZImage.hpp"
#include "vtfDSBMgr.hpp"
#include "vtfProgramCollection.hpp"
#include "vtfGlfwEvents.hpp"
#include "vtfZCommandBuffer.hpp"
#include "vtfBacktrace.hpp"
#include "vtfVector.hpp"
#include "vtfZPipeline.hpp"
#include "vtfCopyUtils.hpp"
#include "vtfStructUtils.hpp"
#include <type_traits>
#include <thread>
#include <charconv>
#include <algorithm>
#include <array>

namespace
{
using namespace vtf;
using ostream_ref = add_ref<std::ostream>;

struct TestParams
{
	struct {
		uint32_t	help				: 1;
		uint32_t	brief				: 1;
		uint32_t	printParams			: 1;
		uint32_t	warning				: 1;
		uint32_t	error				: 1;
		uint32_t	buildAlways			: 1;
		uint32_t	singleTriangle		: 1;
		uint32_t	visual				: 1;
		uint32_t	tessellation		: 1;
		uint32_t	geometry			: 1;
		uint32_t	hasMatchAll			: 1;
		uint32_t	hasParticularMatch	: 1;
		uint32_t	hasAttributes		: 1;

		uint32_t	shaderInt64			: 1;
		uint32_t	shaderFloat64		: 1;
		uint32_t	shaderInt16			: 1;
		uint32_t	demoteInvocation	: 1;
		uint32_t	terminateInvocation : 1;

		uint32_t	pad					: 12;
	} flags { };
	static_assert(sizeof(flags) == sizeof(uint32_t), "???");
	static	auto parseCmdLine	(add_cref<std::string> assets, add_cref<strings> cmdLineParams) -> std::tuple<TestParams, std::string>;
	static	void printHelp		(add_cref<std::string> assets, ostream_ref str);
	static	void printBrief		(add_cref<std::string> assets, ostream_ref str);
			void print			(ostream_ref str) const;
	Vec4						matchAll;
	std::map<uint32_t, Vec4>	particularMatch;
	std::vector<Vec4>			attributes;
	VkExtent2D					extent = { 8u, 8u };
	Vec4						tol		= Vec4(0.001f, 0.001f, 0.001f, 0.001f);
	std::string					vert	= "shader.vert";
	std::string					tesc	= "shader.tesc";
	std::string					tese	= "shader.tese";
	std::string					geom	= "shader.geom";
	std::string					frag	= "shader.frag";
};
struct Tolerance
{
	Tolerance (add_cref<Vec4> tol) : m_tol(tol) {}
	inline bool matches (add_cref<Vec4> range, add_cref<Vec4> value) const
	{
		return	((value.x() >= (range.x() - m_tol.x())) && (value.x() <= (range.x() + m_tol.x())))
			&&	((value.y() >= (range.y() - m_tol.y())) && (value.y() <= (range.y() + m_tol.y())))
			&&	((value.z() >= (range.z() - m_tol.z())) && (value.z() <= (range.z() + m_tol.z())))
			&&	((value.w() >= (range.w() - m_tol.w())) && (value.w() <= (range.w() + m_tol.w())));
	}
protected:
	const Vec4 m_tol;
};

void TestParams::print (ostream_ref str) const
{
	const char newLine = '\n';

	str << "help:               " << boolean(flags.help) << newLine;
	str << "brief:              " << boolean(flags.brief) << newLine;
	str << "printParams:        " << boolean(flags.printParams) << newLine;
	str << "warning:            " << boolean(flags.warning) << newLine;
	str << "error:              " << boolean(flags.error) << newLine;
	str << "buildAlways:        " << boolean(flags.buildAlways) << newLine;
	str << "singleTriangle:     " << boolean(flags.singleTriangle) << newLine;
	str << "visual:             " << boolean(flags.visual) << newLine;
	str << "tessellation:       " << boolean(flags.tessellation) << newLine;
	str << "geometry:           " << boolean(flags.geometry) << newLine;
	str << "hasMatchAll:        " << boolean(flags.hasMatchAll) << newLine;
	str << "hasParticularMatch: " << boolean(flags.hasParticularMatch) << newLine;
	str << "hasAttributes:      " << boolean(flags.hasAttributes) << newLine;
	str << "width:              " << extent.width << newLine;
	str << "height:             " << extent.height << newLine;

	if (flags.hasMatchAll != 0)
	{
		str << "match-all:          " << matchAll << newLine;
	}

	if (flags.hasParticularMatch != 0)
	{
		const uint32_t particularMatchMax = 5u;
		auto iter = particularMatch.cbegin();
		str << "matchN (count):     " << data_count(particularMatch) << newLine;
		for (uint32_t i = 0u; i < particularMatchMax && i < data_count(particularMatch); ++i)
		{
			auto match = *iter++;
			const std::size_t nWidth = (match.first == 0u) ? 1ul : std::size_t(std::floor(std::log10(double(match.first))));
			str << "  match" << match.first << ':' << std::string((11u - nWidth), ' ') << match.second << newLine;
		}
	}

	str << "toll:               " << tol << newLine;

	if (flags.hasAttributes != 0)
	{
		str << "Attributes:" << newLine;
		for (uint32_t i = 0u; i < data_count(attributes); ++i)
			str << "  attr" << i << ":            " << attributes.at(i) << newLine;
	}

	str << "vert:               " << std::quoted(vert) << newLine;
	if (flags.tessellation != 0)
	{
		str << "tesc:               " << std::quoted(tesc) << newLine;
		str << "tese:               " << std::quoted(tese) << newLine;
	}
	if (flags.geometry != 0)
	{
		str << "geom:               " << std::quoted(geom) << newLine;
	}
	str << "frag:               " << std::quoted(frag) << newLine;
}
void TestParams::printBrief (add_cref<std::string> assets, ostream_ref str)
{
	bool status = true;
	const char* shortHelpFile = "brief.txt";
	const std::string content = readFile((fs::path(assets) / shortHelpFile).string(), &status);
	if (status)
		str << content << std::endl;
	else str << "[WARNING] Unable to open " << shortHelpFile << std::endl;
}
void TestParams::printHelp (add_cref<std::string> assets, ostream_ref str)
{
	bool status = true;
	const char* shortHelpFile = "help.txt";
	const std::string content = readFile((fs::path(assets) / shortHelpFile).string(), &status);
	if (status)
		str << content << std::endl;
	else str << "[WARNING] Unable to open " << shortHelpFile << std::endl;
}
std::tuple<TestParams, std::string> TestParams::parseCmdLine (add_cref<std::string> assets, add_cref<strings> cmdLineParams)
{
	bool				status;
	std::array<bool, 4> vecStatus;
	strings				sink;
	strings				args(cmdLineParams);
	std::ostringstream	messages;
	TestParams			res;

	{
		Option optHelp1	{ "-h", 0 };
		Option optHelp2	{ "--help", 0 };
		Option optHelp3 { "-print-help", 0 };
		Option optHelp4	{ "--print-help", 0 };
		std::vector opts{ optHelp1, optHelp2, optHelp3, optHelp4 };
		if ((consumeOptions(optHelp1, opts, args, sink) > 0)
			|| (consumeOptions(optHelp2, opts, args, sink) > 0)
			|| (consumeOptions(optHelp3, opts, args, sink) > 0)
			|| (consumeOptions(optHelp4, opts, args, sink) > 0))
		{
			res.flags.help = 1;
			return { res, std::string() };
		}

		Option optBrief	{ "-brief", 0 };
		opts.push_back(optBrief);
		if (consumeOptions(optBrief, opts, args, sink) > 0)
		{
			res.flags.brief = 1;
			return { res, std::string() };
		}
	}

	Option optPrintParams		{ "-print-params", 0 };
	Option optBuildAlways		{ "-build-always", 0 };
	Option optSingleTriangle	{ "-single-triangle", 0 };
	Option optDisableValidation	{ "-disable_validation", 0 };
	Option optVisualMode		{ "-visual", 0 };
	Option optVerifyAlways		{ "-verify-always", 0 };
	Option optTesselation		{ "-tessellation", 0 };
	Option optGeometry			{ "-geometry", 0 };
	Option optWidth				{ "-width", 1 };
	Option optHeight			{ "-height", 1 };
	Option optMatchAll			{ "-match-all", 1 };
	Option optTol				{ "-tol", 1 };
	Option optVert				{ "-vert", 1 };
	Option optTesc				{ "-tesc", 1 };
	Option optTese				{ "-tese", 1 };
	Option optGeom				{ "-geom", 1 };
	Option optFrag				{ "-frag", 1 };
	std::vector<Option> opts {	optBuildAlways, optSingleTriangle, optDisableValidation,
								optVisualMode, optVerifyAlways, optPrintParams,
								optTesselation, optGeometry, optVert, optTesc, optTese, optGeom, optFrag,
								optMatchAll, optTol };

	res.flags.printParams		= (consumeOptions(optPrintParams, opts, args, sink) > 0);
	res.flags.buildAlways		= (consumeOptions(optBuildAlways, opts, args, sink) > 0);
	res.flags.visual			= (consumeOptions(optVisualMode, opts, args, sink) > 0);
	res.flags.tessellation		= (consumeOptions(optTesselation, opts, args, sink) > 0);
	res.flags.geometry			= (consumeOptions(optGeometry, opts, args, sink) > 0);

	if (consumeOptions(optWidth, opts, args, sink) > 0)
	{
		const int width = fromText(sink.back(), make_signed(res.extent.width), status);
		if (false == status)
			messages << "[WARINNG] Unable to parse " << optWidth.name << ", default " << res.extent.width << " applied" << std::endl;
		if (width < 1 || width > 256)
			messages << "[WARINNG] " << optWidth.name << "must be between <1,256>, default " << res.extent.width << " applied" << std::endl;
		res.extent.width = (width >= 1 && width <= 256) ? make_unsigned(width) : res.extent.width;
		res.flags.warning = 1;
	}
	if (consumeOptions(optHeight, opts, args, sink) > 0)
	{
		const int height = fromText(sink.back(), make_signed(res.extent.height), status);
		if (false == status)
			messages << "[WARINNG] Unable to parse " << optHeight.name << ", default " << res.extent.height << " applied" << std::endl;
		if (height < 1 || height > 256)
			messages << "[WARINNG] " << optHeight.name << "must be between <1,256>, default " << res.extent.height << " applied" << std::endl;
		res.extent.height = (height >= 1 && height <= 256) ? make_unsigned(height) : res.extent.height;
		res.flags.warning = 1;
	}


	if (consumeOptions(optMatchAll, opts, args, sink) > 0)
	{
		res.matchAll = Vec4::fromText(sink.back(), Vec4(), vecStatus, &status);
		res.flags.hasMatchAll = 1;
		if (false == status)
		{
			res.flags.error = 1;
			messages << "[ERROR} Unable to parse " << optMatchAll.name << " from " << std::quoted(sink.back()) << std::endl;
			return { res, messages.str() };
		}
	}

	if (consumeOptions(optTol, opts, args, sink) > 0)
	{
		res.tol = Vec4::fromText(sink.back(), Vec4(), vecStatus, &status);
		if (false == status)
		{
			res.flags.error = 1;
			messages << "[ERROR} Unable to parse " << optTol.name << " from " << std::quoted(sink.back()) << std::endl;
			return { res, messages.str() };
		}
	}

	{
		char name[30];
		Option optMatchNth { name, 1 };
		opts.push_back(optMatchNth);
		const uint32_t maxPixelCount = res.extent.width * res.extent.height;
		for (uint32_t px = 0u; px < maxPixelCount; ++px)
		{
#ifdef _MSC_VER
			sprintf_s(name, "-match%u", px);
#else
			std::sprintf(name, "-match%u", px);
#endif
			if (consumeOptions(optMatchNth, opts, args, sink) > 0)
			{
				const Vec4 match = Vec4::fromText(sink.back(), Vec4(), vecStatus, &status);
				if (status)
				{
					res.flags.hasParticularMatch = 1;
					res.particularMatch[px] = match;
				}
				else
				{
					res.flags.error = 1;
					messages << "[ERROR} Unable to parse " << name << " from " << std::quoted(sink.back()) << std::endl;
					return { res, messages.str() };
				}
			}
		}
		opts.pop_back();
	}

	{
		res.flags.singleTriangle = (consumeOptions(optSingleTriangle, opts, args, sink) > 0);
		Option optAttr0	{ "-attr0", 1 };
		Option optAttr1	{ "-attr1", 1 };
		Option optAttr2	{ "-attr2", 1 };
		opts.push_back(optAttr0);
		opts.push_back(optAttr1);
		opts.push_back(optAttr2);

		uint32_t attrIdx = 0u;
		for (add_cref<Option> attr : { optAttr0, optAttr1, optAttr2 })
		{
			if (consumeOptions(attr, opts, args, sink) > 0)
			{
				res.flags.hasAttributes = 1;

				if (res.flags.singleTriangle)
				{
					if (res.attributes.size() != 3u)
						res.attributes.resize(3u);
				}
				else
				{
					if (res.attributes.size() != 6u)
						res.attributes.resize(6u);
				}

				res.attributes.at(attrIdx) = Vec4::fromText(sink.back(), Vec4(), vecStatus, &status);

				if (false == status)
				{
					res.flags.error = 1;
					messages << "[ERROR} Unable to parse " << attr.name << " from " << std::quoted(sink.back()) << std::endl;
					return { res, messages.str() };
				}
			}
			++attrIdx;
		}

		if (false == res.flags.singleTriangle)
		{
			Option optAttr3	{ "-attr3", 1 };
			Option optAttr4	{ "-attr4", 1 };
			Option optAttr5	{ "-attr5", 1 };
			opts.push_back(optAttr3);
			opts.push_back(optAttr4);
			opts.push_back(optAttr5);

			for (add_cref<Option> attr : { optAttr3, optAttr4, optAttr5 })
			{
				if (consumeOptions(attr, opts, args, sink) > 0)
				{
					res.flags.hasAttributes = 1;

					if (res.attributes.size() != 6u)
						res.attributes.resize(6u);

					res.attributes.at(attrIdx) = Vec4::fromText(sink.back(), Vec4(), vecStatus, &status);

					if (false == status)
					{
						res.flags.error = 1;
						messages << "[ERROR} Unable to parse " << attr.name << " from " << std::quoted(sink.back()) << std::endl;
						return { res, messages.str() };
					}
				}
				++attrIdx;
			}
		}
	}

	{
		if (consumeOptions(optVert, opts, args, sink) > 0)
			res.vert = sink.back();
		else res.vert = (fs::path(assets) / res.vert).string();
		if ( ! fs::exists(fs::path(res.vert)))
		{
			res.flags.error = 1;
			messages << "[ERROR] Vertex shader " << std::quoted(res.vert) << " doesn't exist" << std::endl;
			return { res, messages.str() };
		}

		if (consumeOptions(optTesc, opts, args, sink) > 0)
		{
			res.flags.tessellation = 1;
			res.tesc = sink.back();
		}
		else res.tesc = (fs::path(assets) / res.tesc).string();
		if ( ! fs::exists(fs::path(res.tesc)))
		{
			res.flags.error = 1;
			messages << "[ERROR] Tessellation control shader " << std::quoted(res.tesc) << " doesn't exist" << std::endl;
			return { res, messages.str() };
		}

		if (consumeOptions(optTese, opts, args, sink) > 0)
		{
			res.flags.tessellation = 1;
			res.tese = sink.back();
		}
		else res.tese = (fs::path(assets) / res.tese).string();
		if ( ! fs::exists(fs::path(res.tese)))
		{
			res.flags.error = 1;
			messages << "[ERROR] Tessellation evaluation shader " << std::quoted(res.tese) << " doesn't exist" << std::endl;
			return { res, messages.str() };
		}

		if (consumeOptions(optGeom, opts, args, sink) > 0)
		{
			res.flags.geometry = 1;
			res.geom = sink.back();
		}
		else res.geom = (fs::path(assets) / res.geom).string();
		if ( ! fs::exists(fs::path(res.geom)))
		{
			res.flags.error = 1;
			messages << "[ERROR] Geometry shader " << std::quoted(res.geom) << " doesn't exist" << std::endl;
			return { res, messages.str() };
		}

		if (consumeOptions(optFrag, opts, args, sink) > 0)
			res.frag = sink.back();
		else res.frag = (fs::path(assets) / res.frag).string();
		if ( ! fs::exists(fs::path(res.frag)))
		{
			res.flags.error = 1;
			messages << "[ERROR] Vertex shader " << std::quoted(res.frag) << " doesn't exist" << std::endl;
			return { res, messages.str() };
		}
	}

	if ( ! args.empty())
	{
		res.flags.error = 1;
		messages << "[ERROR] Unknown option " << std::quoted(args.at(0u)) << std::endl;
	}

	return { res, messages.str() };
}
TriLogicInt runTests (std::shared_ptr<VulkanContext> ctx, add_cref<TestParams> params);
TriLogicInt prepareTests (const TestRecord& record, const strings& cmdLineParams)
{
	const auto [prms, messages]	= TestParams::parseCmdLine(record.assets, cmdLineParams);
	add_cref<TestParams> params(prms);
	if (params.flags.printParams)
	{
		params.print(std::cout);
	}
	if (params.flags.help)
	{
		TestParams::printHelp(record.assets, std::cout);
		return {};
	}
	if (params.flags.brief)
	{
		TestParams::printBrief(record.assets, std::cout);
		return {};
	}
	if (params.flags.error)
	{
		std::cout << messages << std::endl;
		return {};
	}

	auto onEnablingFeatures = [&](add_ref<DeviceCaps> caps)
	{
		if (params.flags.terminateInvocation)
		{
			const auto f = &VkPhysicalDeviceShaderTerminateInvocationFeatures::shaderTerminateInvocation;
			caps.addUpdateFeatureIf(f).checkSupported("shaderTerminateInvocation");
		}
		if (params.flags.demoteInvocation)
		{
			const auto f = &VkPhysicalDeviceShaderDemoteToHelperInvocationFeatures::shaderDemoteToHelperInvocation;
			caps.addUpdateFeatureIf(f).checkSupported("shaderDemoteToHelperInvocation");
		}

		if (params.flags.tessellation)
		{
			caps.addUpdateFeatureIf(&VkPhysicalDeviceFeatures::tessellationShader)
				.checkSupported("tessellationShader");
		}
		if (params.flags.geometry)
		{
			caps.addUpdateFeatureIf(&VkPhysicalDeviceFeatures::geometryShader)
				.checkSupported("geometryShader");
		}
		if (params.flags.shaderFloat64)
		{
			caps.addUpdateFeatureIf(&VkPhysicalDeviceFeatures::shaderFloat64)
				.checkSupported("shaderFloat64");
		}
		if (params.flags.shaderInt64)
		{
			caps.addUpdateFeatureIf(&VkPhysicalDeviceFeatures::shaderInt64)
				.checkSupported("shaderInt64");
		}
		if (params.flags.shaderInt16)
		{
			caps.addUpdateFeatureIf(&VkPhysicalDeviceFeatures::shaderInt16)
				.checkSupported("shaderInt16");
		}
	};
	add_cref<GlobalAppFlags>	gf			= getGlobalAppFlags();
	CanvasStyle					canvasStyle = Canvas::DefaultStyle;
	canvasStyle.surfaceFormatFlags |= (VK_FORMAT_FEATURE_BLIT_SRC_BIT | VK_FORMAT_FEATURE_BLIT_DST_BIT);

	const Version usedApiVer = (gf.apiVer < Version(1, 2)) ? Version(1, 2) : gf.apiVer;
	std::shared_ptr<VulkanContext> ctx = (params.flags.visual != 0)
		? std::make_shared<Canvas>(record.name, gf.layers, strings(), strings(), canvasStyle, onEnablingFeatures, usedApiVer)
		: std::make_shared<VulkanContext>(record.name, gf.layers, strings(), strings(), onEnablingFeatures, usedApiVer);
	if (params.flags.warning)
	{
		std::cout << messages << std::endl;
	}
	return runTests(ctx, params);
}
std::array<ZShaderModule, 5> buildProgram (ZDevice device, add_cref<TestParams> params)
{
	const GlobalAppFlags	flags	(getGlobalAppFlags());
	ProgramCollection		program	(device);
	program.addFromFile(VK_SHADER_STAGE_VERTEX_BIT, params.vert);
	program.addFromFile(VK_SHADER_STAGE_FRAGMENT_BIT, params.frag);
	if (params.flags.tessellation)
	{
		program.addFromFile(VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT, params.tesc);
		program.addFromFile(VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT, params.tese);
	}
	if (params.flags.geometry)
	{
		program.addFromFile(VK_SHADER_STAGE_GEOMETRY_BIT, params.geom);
	}
	program.buildAndVerify(flags.vulkanVer, flags.spirvVer, flags.spirvValidate, false, (params.flags.buildAlways != 0));

	return
	{
		program.getShader(VK_SHADER_STAGE_VERTEX_BIT),
		program.getShader(VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT, 0u, params.flags.tessellation != 0),
		program.getShader(VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT, 0u, params.flags.tessellation != 0),
		program.getShader(VK_SHADER_STAGE_GEOMETRY_BIT, 0u, params.flags.geometry != 0),
		program.getShader(VK_SHADER_STAGE_FRAGMENT_BIT),
	};
}
void buildVertices (add_ref<VertexInput> vi, add_cref<TestParams> params)
{
	if (params.flags.singleTriangle)
	{
		const std::vector<Vec2>		vertices{ { -1.0, +3.5 }, { -1.0, -1.0 }, { +3.5, -1.0 } };
		vi.binding(0).addAttributes(vertices);
	}
	else
	{
		const std::vector<Vec2>		vertices{	{ -1, +1 }, { -1, -1 }, { +1, -1 },
												{ +1, -1 }, { +1, +1 }, { -1, +1 } };
		vi.binding(0).addAttributes(vertices);
	}
	if (params.flags.hasAttributes)
	{
		vi.binding(0).addAttributes(params.attributes);
	}
}

TriLogicInt runTests (std::shared_ptr<VulkanContext> ctx, add_cref<TestParams> params)
{
	LayoutManager					pl				(ctx->device);
	std::array<ZShaderModule, 5>	shaders			= buildProgram(ctx->device, params);

	VertexInput					vertexInput			(ctx->device);
	buildVertices(vertexInput, params);

	const VkFormat				renderFormat		= VK_FORMAT_R32G32B32A32_SFLOAT;
	const VkClearValue			clearColor			{ { { 0.5f, 0.5f, 0.5f, 0.5f } } };
	ZRenderPass					renderRenderPass	= createColorRenderPass(ctx->device, { renderFormat }, { {clearColor} });
	const uint64_t				size				= (params.extent.width * params.extent.height);
	ZImage						renderImage			= ctx->createColorImage2D(renderFormat, params.extent.width, params.extent.height);
	ZImageView					renderView			= createImageView(renderImage);
	ZFramebuffer				renderFramebuffer	= createFramebuffer(renderRenderPass, params.extent, { renderView });
	ZPipelineLayout				pipelineLayout		= pl.createPipelineLayout();
	const VkPrimitiveTopology	topology			= params.flags.tessellation
														? VK_PRIMITIVE_TOPOLOGY_PATCH_LIST
														: VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	const uint32_t				patchControlPoints	= params.flags.tessellation ? 3u : 0u;
	ZPipeline					renderPipeline		= createGraphicsPipeline(pipelineLayout, renderRenderPass,
																params.extent, vertexInput,
																topology, gpp::PatchControlPoints(patchControlPoints),
																shaders.at(0u), shaders.at(1u), shaders.at(2u),
																shaders.at(3u), shaders.at(4u));
	ZBuffer						buffer				= createBuffer(renderImage,
																ZBufferUsageFlags(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT),
																ZMemoryPropertyHostFlags);
	ZCommandPool				renderCmdPool		= createCommandPool(ctx->device, ctx->graphicsQueue);
	ZCommandBuffer				renderCmd			= allocateCommandBuffer(renderCmdPool);

	ZImageMemoryBarrier			storeBarrier		(renderImage, VK_ACCESS_COLOR_ATTACHMENT_READ_BIT, VK_ACCESS_HOST_READ_BIT,
																	VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

	commandBufferBegin(renderCmd);
		commandBufferBindPipeline(renderCmd, renderPipeline);
		commandBufferBindVertexBuffers(renderCmd, vertexInput);
		auto rpbi = commandBufferBeginRenderPass(renderCmd, renderFramebuffer);
			vkCmdDraw(*renderCmd, vertexInput.getVertexCount(0), 1, 0, 0);
		commandBufferEndRenderPass(rpbi);
		commandBufferPipelineBarriers(renderCmd, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_HOST_BIT, storeBarrier);
		imageCopyToBuffer(renderCmd, renderImage, buffer, VK_ACCESS_HOST_READ_BIT, VK_ACCESS_NONE,
															VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_NONE,
															VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
	commandBufferEnd(renderCmd);
	commandBufferSubmitAndWait(renderCmd);

	if (params.flags.visual != 0)
	{
		int							displayTrigger		= 1;
		add_ref<Canvas>				canvas				= *std::static_pointer_cast<Canvas>(ctx);
		const VkFormat				displayFormat		= canvas.surfaceFormat;
		ZRenderPass					displayRenderPass	= createColorRenderPass(ctx->device, { displayFormat }, { {clearColor} });

		auto						onKey				= [](add_ref<Canvas> cs, void*, const int key, int, int action, int) -> void
															{
																if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
																	glfwSetWindowShouldClose(*cs.window, GLFW_TRUE);
															};
		auto						onResize			= [](add_ref<Canvas>, void* userData, int, int) -> void
															{
																*((int*)userData) += 1;
															};

		canvas.events().cbKey.set(onKey, nullptr);
		canvas.events().cbWindowSize.set(onResize, &displayTrigger);

		auto onCommandRecording = [&](add_ref<Canvas>, add_cref<Canvas::Swapchain>,
										ZCommandBuffer displayCmd, ZFramebuffer displayFramebuffer)
		{
			ZImage				displayImage	= framebufferGetImage(displayFramebuffer);
			ZImageMemoryBarrier	srcPreBlit		(renderImage, VK_ACCESS_NONE, VK_ACCESS_TRANSFER_READ_BIT,
																VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
			ZImageMemoryBarrier	dstPreBlit		(displayImage, VK_ACCESS_NONE, VK_ACCESS_TRANSFER_WRITE_BIT,
																VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
			commandBufferBegin(displayCmd);
				commandBufferPipelineBarriers(displayCmd,
												VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
												srcPreBlit, dstPreBlit);
				commandBufferBlitImage(displayCmd, renderImage, displayImage, VK_FILTER_NEAREST);
				commandBufferMakeImagePresentationReady(displayCmd, displayImage);
			commandBufferEnd(displayCmd);
		};

		canvas.run(onCommandRecording, displayRenderPass, std::ref(displayTrigger));
	}

	TriLogicInt result{ /* intentionally empty */ };

	if ((params.flags.hasMatchAll != 0) || (params.flags.hasParticularMatch != 0))
	{
		std::vector<Vec4>	content		(bufferGetElementCount<Vec4>(buffer));
		ASSERTMSG((content.size() == size), "Framebuffer output size mismatch");
		const auto			readSize	= bufferRead(buffer, content) / sizeof(Vec4);
		ASSERTMSG((readSize == size), "Framebuffer content output size mismatch");

		uint32_t					allMismatchIdx			= INVALID_UINT32;
		uint32_t					allMismatchCount		= 0u;
		Vec4						allMismatchValue		= {};
		uint32_t					particularMismatchCount	= 0u;
		const uint32_t				particularMismatchMax	= 5u;
		uint32_t					particularMismatchIdx	= 0u;
		std::map<uint32_t, Vec4>	particularMismatchMap;
		const Tolerance				tol(params.tol);

		for (uint32_t i = 0u; i < size; ++i)
		{
			bool matches = false;
			bool checked = false;
			add_cref<Vec4> value = content.at(i);

			if (params.flags.hasParticularMatch != 0)
			{
				if (mapHasKey(params.particularMatch, i))
				{
					checked = true;
					if (matches = tol.matches(params.particularMatch.at(i), value); !matches)
					{
						particularMismatchCount += 1u;

						if (particularMismatchIdx++ < particularMismatchMax)
							particularMismatchMap[i] = value;
					}
				}
			}
			if (false == checked && params.flags.hasMatchAll != 0)
			{
				if (matches = tol.matches(params.matchAll, value); !matches)
				{
					allMismatchCount += 1;

					if (allMismatchIdx == INVALID_UINT32)
					{
						allMismatchIdx = i;
						allMismatchValue = value;
					}
				}
			}
		}

		result = 0;

		if (allMismatchCount > 0)
		{
			result = 1;
			ctx->logger << "First mismatch pixel from all at: " << allMismatchIdx
				<< " from " << allMismatchCount
				<< ", expected: " << params.matchAll
				<< " but got: " << allMismatchValue << std::endl;
		}
		if (particularMismatchCount > 0)
		{
			result = 1;
			ctx->logger << "Found " << ((allMismatchCount > 0) ? "also " : "")
				<< particularMismatchCount << " particular mismatches"
				<< " (showed " << particularMismatchMax << "):" << std::endl;
			for (add_cref<std::pair<const uint32_t, Vec4>> item : particularMismatchMap)
			{
				ctx->logger << "  At: " << item.first
					<< " expected: " << params.particularMatch.at(item.first)
					<< " but got: " << item.second << std::endl;
			}
		}
	}

	return result;
}

} // unnamed namespace

template<> struct TestRecorder<INT_GRAPHICS>
{
	static bool record (TestRecord&);
};
bool TestRecorder<INT_GRAPHICS>::record (TestRecord& record)
{
	record.name = "int_graphics";
	record.call = &prepareTests;
	return true;
}
