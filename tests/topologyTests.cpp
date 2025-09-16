#include "topologyTests.hpp"
#include "vtfOptionParser.hpp"
#include "vtfCanvas.hpp"
#include "vtfZImage.hpp"
#include "vtfDSBMgr.hpp"
#include "vtfProgramCollection.hpp"
#include "vtfGlfwEvents.hpp"
#include "vtfZCommandBuffer.hpp"
#include "vtfBacktrace.hpp"
#include "vtfVector.hpp"
#include "vtfMatrix.hpp"
#include "vtfZPipeline.hpp"
#include "vtfStructUtils.hpp"
#include "vtfCopyUtils.hpp"
#include "vtfTemplateUtils.hpp"
#include <array>
#include <bitset>
#include <type_traits>
#include <thread>

namespace
{
using namespace vtf;

struct Params
{
	float		plusX;
	float		plusY;
	float		minusX;
	float		minusY;
	float		pointSize;
	VkBool32	ccwFronFace;
	VkPrimitiveTopology topo;
	std::vector<Vec2> vertices;
	Params ();
	void print (add_ref<std::ostream> log) const;
};
static const std::vector<Vec2> defaultCwVertices = {{ -1, +1 }, { 0, -1 }, { +1, +1 }};
static const std::vector<Vec2> defaultCcwVertices = {{ -1, -1 }, { 0, +1 }, { +1, -1 }};
Params::Params ()
	: plusX			(+1.0f)
	, plusY			(+1.0f)
	, minusX		(-1.0f)
	, minusY		(-1.0f)
	, pointSize		(1.0f)
	, ccwFronFace	(VK_FALSE)
	, topo			(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
	, vertices		(defaultCwVertices)
{
}

void Params::print (add_ref<std::ostream> log) const
{
	log << "Current settings:" << std::endl;
	log << "  Area (-x,-y,+x,+y): (" << minusX << ',' << minusY << ',' << plusX << ',' << plusY << ')' << std::endl;
	log << "  Topology:           " << topo << std::endl;
	log << "  FrontFace:          " << (ccwFronFace ? "VK_FRONT_FACE_COUNTER_CLOCKWISE" : "VK_FRONT_FACE_CLOCKWISE") << std::endl;
	log << "  PointSize:          " << pointSize << std::endl;
	log << "  Vertex count:       " << vertices.size() << ", few first vertices: ";
	for (uint32_t i = 0; i < 5 && i < data_count(vertices); ++i)
	{
		log << vertices[i] << ' ';
	}
	log << std::endl;
}

uint32_t parseVertices (add_cref<strings> cmdLineParams, uint32_t index, add_ref<Params> params, add_ref<std::ostream> log)
{
	bool x = true;
	float f = 0.0;
	uint32_t n = 0;

	for ( ; index < data_count(cmdLineParams); ++index)
	{
		bool status = false;
		add_cref<std::string> s = cmdLineParams[index];
		f = fromText(s, 0.0f, status);
		if (status)
		{
			if (x)
				params.vertices.emplace_back(f, 0.0f);
			else
				params.vertices.back().y(f);
			x ^= true;
			n += 1u;
		}
		else
		{
			log << "Unable to parse " << cmdLineParams[index] << " as float, ignored" << std::endl;
		}
	}
	return n;
}

Params readCommandLine (add_cref<strings> cmdLineParams, add_cref<Params> defaultParams,
						add_ref<std::ostream> log, add_ref<bool> parseStatus)
{
	strings	sink;
	bool	status = false;
	strings	args(cmdLineParams);
	Option	optTopo { "-t", 1 };
	Option	optPlusX { "+x", 1 };
	Option	optPlusY { "+y", 1 };
	Option	optMinusX { "-x", 1 };
	Option	optMinusY { "-y", 1 };
	Option	optPointSize { "-ps", 1 };
	Option	optFronFace { "-ccw", 0 };
	std::vector<Option>	options { optTopo, optPlusX, optPlusY, optMinusX, optMinusY, optPointSize, optFronFace };

	Params	resultParams = defaultParams;

	if (consumeOptions(optFronFace, options, args, sink) > 0)
	{
		resultParams.ccwFronFace = VK_TRUE;
	}

	if (consumeOptions(optTopo, options, args, sink) > 0)
	{
		int topo = fromText(sink.back(), int(defaultParams.topo), status);
		if (status && (topo < 0 || topo > 9))
		{
			log << "Topology must be from range [0,9], applied " << int(defaultParams.topo) << std::endl;
			topo = int(defaultParams.topo);
		}
		else if (!status)
		{
			log << "Unable to parse " << optTopo.name << " parameter, applied " << int(defaultParams.topo) << std::endl;
			topo = int(defaultParams.topo);
		}
		resultParams.topo = VkPrimitiveTopology(topo);
	}

	if (consumeOptions(optPlusX, options, args, sink) > 0)
	{
		resultParams.plusX = fromText(sink.back(), defaultParams.plusX, status);
		if (!status)
			log << "Unable to parse " << optPlusX.name << " parameter, applied " << defaultParams.plusX << std::endl;
	}

	if (consumeOptions(optPlusY, options, args, sink) > 0)
	{
		resultParams.plusY = fromText(sink.back(), defaultParams.plusY, status);
		if (!status)
			log << "Unable to parse " << optPlusY.name << " parameter, applied " << defaultParams.plusY << std::endl;
	}

	if (consumeOptions(optMinusX, options, args, sink) > 0)
	{
		resultParams.minusX = fromText(sink.back(), defaultParams.minusX, status);
		if (!status)
			log << "Unable to parse " << optMinusX.name << " parameter, applied " << defaultParams.minusX << std::endl;
	}

	if (consumeOptions(optMinusY, options, args, sink) > 0)
	{
		resultParams.minusY = fromText(sink.back(), defaultParams.minusY, status);
		if (!status)
			log << "Unable to parse " << optMinusY.name << " parameter, applied " << defaultParams.minusY << std::endl;
	}

	if (consumeOptions(optPointSize, options, args, sink) > 0)
	{
		resultParams.pointSize = fromText(sink.back(), defaultParams.pointSize, status);
		if (!status)
			log << "Unable to parse " << optPointSize.name << " parameter, applied " << defaultParams.pointSize << std::endl;
	}

	resultParams.vertices.clear();
	const uint32_t c = parseVertices(args, 0, resultParams, log);
	if (c == 0u)
	{
		log << "Cannot perform with empty vertices, applied default set" << std::endl;
		resultParams.vertices = resultParams.ccwFronFace ? defaultCcwVertices : defaultCwVertices;
	}

	if (parseStatus = args.empty(); false == parseStatus)
	{
		log << "[ERROR] Unrecognized parameter " << args[0] << std::endl;
	}

	return resultParams;
}

Params printUsage (std::ostream& log)
{
	Params p;
	const char nl = '\n';
	log << "Parameters:\n"
		<< "  [-x float]        x-area range, default is " << p.minusX << nl
		<< "  [+x float]        x-area range, default is " << p.plusX << nl
		<< "  [-y float]        y-area range, default is " << p.minusY << nl
		<< "  [+y float]        y-area range, default is " << p.plusY << nl
		<< "  [-ps float]       point size, default is " << p.pointSize << nl
		<< "  [-ccw] (standalone) use VK_FRONT_FACE_COUNTER_CLOCKWISE\n"
		<< "                      default is VK_FRONT_FACE_CLOCKWISE\n"
		<< "  [-t topology]     topology, default is " << int(p.topo) << nl
		<< "     VK_PRIMITIVE_TOPOLOGY_POINT_LIST                    = 0\n"
		<< "     VK_PRIMITIVE_TOPOLOGY_LINE_LIST                     = 1\n"
		<< "     VK_PRIMITIVE_TOPOLOGY_LINE_STRIP                    = 2\n"
		<< "     VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST                 = 3\n"
		<< "     VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP                = 4\n"
		<< "     VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN                  = 5\n"
		<< "     VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY      = 6\n"
		<< "     VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY     = 7\n"
		<< "     VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY  = 8\n"
		<< "     VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY = 9\n"
		<< "  [float...]        vertices, alternately x and y\n"
		<< "                    all standalone values will be treated as vertex\n"
		<< "                    if y component is missing, then use default 0.0\n"
		<< "Navigation keys:\n"
		<< "  Mouse Click: add new point\n"
		<< "  Mouse Move: track position to add new point\n"
		<< "  Ctrl+z:    remote last point\n"
		<< "  S:         print pipeline statistics\n"
		<< "             vertex and primitive count\n"
		<< "  Escape: quit this app\n"
		<< "Overall purpose:\n"
		<< "  Click everywhere on the surface then move the mice cursor\n"
		<< "  wherever you want with holding left key pressed concurrently.\n"
		<< "  Program will draw lines, points or triangles according to\n"
		<< "  a topology it used with green border color and fill polygons.\n"
		<< std::endl;
	return p;
}

TriLogicInt runTopologyTestsSingleThread (add_ref<Canvas> canvas, add_cref<std::string> assets, add_cref<Params> params);

bool isTopologyList(const VkPrimitiveTopology topo)
{
	return topo == VK_PRIMITIVE_TOPOLOGY_POINT_LIST
		|| topo == VK_PRIMITIVE_TOPOLOGY_LINE_LIST
		|| topo == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST
		|| topo == VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY
		|| topo == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY;
}

TriLogicInt prepareTests (const TestRecord& record, const strings& cmdLineParams)
{
	add_cref<GlobalAppFlags> gf = getGlobalAppFlags();
	CanvasStyle canvasStyle = Canvas::DefaultStyle;
	canvasStyle.surfaceFormatFlags |= (VK_FORMAT_FEATURE_BLIT_SRC_BIT | VK_FORMAT_FEATURE_BLIT_DST_BIT);

	bool parseResult = true;
	bool featuresAvailable = true;
	std::ostream& log = std::cout;
	const Params defaultParams = printUsage(log);
	const Params params = readCommandLine(cmdLineParams, defaultParams, log, parseResult);
	const bool geometryRequired =	params.topo == VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY
								||	params.topo == VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY
								||	params.topo == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY
								||	params.topo == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY;
	if (false == parseResult)
	{
		return {};
	}

	std::ostringstream							errorCollection;

	auto onEnablingFeatures = [&](add_ref<DeviceCaps> caps) -> void
	{
		if (geometryRequired)
		{
			featuresAvailable &= caps.addUpdateFeatureIf(&VkPhysicalDeviceFeatures::geometryShader);
			if (!featuresAvailable)
				errorCollection << "[ERROR] VkPhysicalDeviceFeatures.geometryShader not supported by device\n";
		}

		if (isTopologyList(params.topo))
		{
			caps.requiredExtension.emplace_back(VK_EXT_PRIMITIVE_TOPOLOGY_LIST_RESTART_EXTENSION_NAME);
			if (!caps.addUpdateFeatureIf(&VkPhysicalDevicePrimitiveTopologyListRestartFeaturesEXT
				::primitiveTopologyListRestart))
				throw std::runtime_error("[ERROR] primitiveTopologyListRestart not supported by device");
		}

		featuresAvailable &= caps.addUpdateFeatureIf(&VkPhysicalDeviceFeatures::pipelineStatisticsQuery);
	};

	Canvas cs(record.name, gf.layers, {}, {}, canvasStyle, onEnablingFeatures, gf.apiVer, gf.debugPrintfEnabled);
	if (!featuresAvailable)
	{
		errorCollection.flush();
		std::cout << errorCollection.str();
		return 1;
	}

	return runTopologyTestsSingleThread (cs, record.assets, params);
}

struct UserData
{
	int			drawTrigger;
	double		windowXXXcursor;
	double		windowYYYcursor;
	double		miceButtonXXXcursor;
	double		miceButtonYYYcursor;
	bool		miceButtonPressed;
	bool		controlKeyPressed;
	float		zoom;
	const float	plusX;
	const float	plusY;
	const float	minusX;
	const float	minusY;
	uint32_t	pointCount;
	ZBuffer		vertexBuffer;
	ZBuffer		indexBuffer;
	uint64_t	queryResult[2];
	UserData (const Params& params, ZBuffer vertexBuff, ZBuffer indexBuffer);
	void updateLastPointCoordinates (float width, float height);
	void addConsecutivePoint (float width, float height, bool restart);
	void removeLastPoint (float width, float height, std::function<void(float x, float y)> changeMousePos);
};
UserData::UserData (const Params& params, ZBuffer vertexBuff, ZBuffer indexBuff)
	: drawTrigger			(1)
	, windowXXXcursor		(0.0)
	, windowYYYcursor		(0.0)
	, miceButtonXXXcursor	(0.0)
	, miceButtonYYYcursor	(0.0)
	, miceButtonPressed		(false)
	, controlKeyPressed		(false)
	, zoom					(1.0f)
	, plusX					(params.plusX)
	, plusY					(params.plusY)
	, minusX				(params.minusX)
	, minusY				(params.minusY)
	, pointCount			(data_count(params.vertices) + 1u)
	, vertexBuffer			(vertexBuff)
	, indexBuffer			(indexBuff)
	, queryResult			()
{
}
void UserData::updateLastPointCoordinates (float width, float height)
{
	float x = float(windowXXXcursor) / width;
	float y = float(windowYYYcursor) / height;
	float X = x * (plusX - minusX) + minusX;
	float Y = y * (plusY - minusY) + minusY;
	BufferTexelAccess<Vec2> a(vertexBuffer, pointCount, 1u, 1u);
	a.at(pointCount - 1u, 0u, 0u) = Vec2(X, Y);
}
void UserData::addConsecutivePoint (float width, float height, bool restart)
{
	const float x = float(windowXXXcursor) / width;
	const float y = float(windowYYYcursor) / height;
	const float X = x * (plusX - minusX) + minusX;
	const float Y = y * (plusY - minusY) + minusY;

	BufferTexelAccess<Vec2> vertices(vertexBuffer, (pointCount + 1u), 1u, 1u);
	vertices.at(pointCount - 1u, 0u, 0u) = Vec2(X, Y);
	vertices.at(pointCount - 0u, 0u, 0u) = Vec2(X, Y);

	//const uint32_t maxPointCount = bufferGetElementCount<uint32_t>(indexBuffer);
	BufferTexelAccess<uint32_t> indices(indexBuffer, pointCount+1, 1u, 1u);
	indices.at(pointCount - 1u, 0u, 0u) = restart ? INVALID_UINT32 : (pointCount - 1);
	indices.at(pointCount - 0u, 0u, 0u) = pointCount - 1;

	pointCount = pointCount + 1u;
}
void UserData::removeLastPoint (float width, float height, std::function<void(float x, float y)> changeMousePos)
{
	if (pointCount > 1u)
	{
		pointCount = pointCount - 1u;
		if (changeMousePos)
		{
			BufferTexelAccess<Vec2> v(vertexBuffer, pointCount, 1u);
			const Vec2 pt = v.at(pointCount - 1u, 0u, 0u);
			const float xCast = (pt.x() - minusX) / (plusX - minusX);
			const float yCast = (pt.y() - minusY) / (plusY - minusY);
			changeMousePos((xCast * width), (yCast * height));
		}
	}
}

void onResize (add_ref<Canvas>, add_ptr<void> userData, int width, int height)
{
	MULTI_UNREF(width, height);
	if (userData)
	{
		static_cast<add_ptr<UserData>>(userData)->drawTrigger += 1;
	}
}

void onKey (add_ref<Canvas> cs, add_ptr<void> userData, const int key, int scancode, int action, int mods)
{
	MULTI_UNREF(userData, scancode, mods);
	if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
	{
		glfwSetWindowShouldClose(*cs.window, GLFW_TRUE);
	}
	auto ui = static_cast<add_ptr<UserData>>(userData);
	if (key == GLFW_KEY_LEFT_CONTROL)
	{
		ui->controlKeyPressed = (action == GLFW_PRESS);
	}
	if (key == GLFW_KEY_Z && action == GLFW_PRESS)
	{
		auto changeMousePos = [&](float xpos, float ypos) -> void
		{
			glfwSetCursorPos(*cs.window, xpos, ypos);
		};

		if (ui->controlKeyPressed)
		{
			ui->removeLastPoint(float(cs.width), float(cs.height), changeMousePos);
			ui->drawTrigger += 2;
		}
	}
	if (key == GLFW_KEY_S && action == GLFW_PRESS)
	{
		std::cout << "Vertices: " << ui->queryResult[0] << ", Primitives: " << ui->queryResult[1] << std::endl;
	}
}

void onMouseBtn (add_ref<Canvas> cs, add_ptr<void> userData, int button, int action, int mods)
{
	MULTI_UNREF(action, mods);
	if (button == GLFW_MOUSE_BUTTON_LEFT || button == GLFW_MOUSE_BUTTON_RIGHT)
	{
		auto ui = static_cast<add_ptr<UserData>>(userData);
		ui->miceButtonXXXcursor = ui->windowXXXcursor;
		ui->miceButtonYYYcursor = ui->windowYYYcursor;
		ui->miceButtonPressed = (action == GLFW_RELEASE);
		if (!ui->miceButtonPressed)
		{
			ui->addConsecutivePoint(float(cs.width), float(cs.height), button == GLFW_MOUSE_BUTTON_RIGHT);
			++ui->drawTrigger;
		}
	}
}

void onCursorPos (add_ref<Canvas> cs, add_ptr<void> userData, double xpos, double ypos)
{
	auto ui = static_cast<add_ptr<UserData>>(userData);
	ui->windowXXXcursor = xpos;
	ui->windowYYYcursor = ypos;
	++ui->drawTrigger;
	ui->updateLastPointCoordinates(float(cs.width), float(cs.height));

	if (ui->miceButtonPressed)
	{
		const double wx = xpos - ui->miceButtonXXXcursor;
		const double wy = ypos - ui->miceButtonYYYcursor;
		const float dx = float(wx) / float(cs.width);	UNREF(dx);
		const float dy = float(wy) / float(cs.height);	UNREF(dy);
		// here, you can do something with dx and dy
	}
}

void onScroll (add_ref<Canvas>, add_ptr<void> userData, double xScrollOffset, double yScrollOffset)
{
	UNREF(xScrollOffset);
	auto ui = static_cast<add_ptr<UserData>>(userData);
	++ui->drawTrigger;

	if (yScrollOffset > 0.0)
		ui->zoom *= 1.125f;
	else
		ui->zoom /= 1.125f;
}

std::tuple<ZBuffer, ZBuffer> createVertexAndIndexBuffers (add_ref<VertexInput> input, add_cref<Params> params)
{
	typedef collection_element_t<decltype(params.vertices)> vertexType;
	input.binding(0).declareAttributes<Fmt_<vertexType>>();
	const uint32_t vertexCount = data_count(params.vertices) + 256u;
	ZBuffer vertexBuffer = createBuffer(input.device, type_to_vk_format<Fmt_<vertexType>>, vertexCount,
										ZBufferUsageFlags(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT), ZMemoryPropertyHostFlags, ZBufferCreateFlags());
	BufferTexelAccess<vertexType> a(vertexBuffer, vertexCount, 1u, 1u);
	for (uint32_t i = 0; i < data_count(params.vertices); ++i)
	{
		a.at(i, 0u) = params.vertices[i];
	}
	a.at(vertexCount-1u, 0u) = vertexType();

	ZBuffer indexBuffer = createIndexBuffer(input.device, vertexCount, VK_INDEX_TYPE_UINT32);
	BufferTexelAccess<uint32_t> b(indexBuffer, vertexCount, 1u, 1u);
	for (uint32_t i = 0; i < vertexCount; ++i)
	{
		b.at(i, 0u) = i;
	}

	return { vertexBuffer, indexBuffer };
}

std::array<ZShaderModule, 6> buildProgram (ZDevice device, add_cref<std::string> assets)
{
	ProgramCollection			programs(device, assets);
	const std::string			vertexShader("univertex.shader");
	const std::string			fragmentShader("unifragment.shader");
	programs.addFromFile(VK_SHADER_STAGE_VERTEX_BIT, vertexShader);
	programs.addFromFile(VK_SHADER_STAGE_FRAGMENT_BIT, fragmentShader);
	programs.addFromFile(VK_SHADER_STAGE_COMPUTE_BIT, "unicompute.shader");
	const GlobalAppFlags		flags(getGlobalAppFlags());
	programs.buildAndVerify(flags.vulkanVer, flags.spirvVer, flags.spirvValidate, flags.genSpirvDisassembly);

	std::array<ZShaderModule, 6> shaders
	{
		programs.getShader(VK_SHADER_STAGE_VERTEX_BIT),
		programs.getShader(VK_SHADER_STAGE_FRAGMENT_BIT),
		programs.getShader(VK_SHADER_STAGE_COMPUTE_BIT),
	};

	return shaders;
}

TriLogicInt runTopologyTestsSingleThread (Canvas& cs, add_cref<std::string> assets, add_cref<Params> params)
{
	params.print(std::cout);

	LayoutManager				lm(cs.device);

	auto						shaders			= buildProgram(cs.device, assets);
	ZShaderModule				vertShaderModule= shaders[0];
	ZShaderModule				fragShaderModule= shaders[1];

	VertexInput					vertexInput			(cs.device);
	ZBuffer						vertexBuffer, indexBuffer;
	std::tie(vertexBuffer, indexBuffer) = createVertexAndIndexBuffers(vertexInput, params);

	const VkFormat				format				= cs.surfaceFormat;
	const VkClearValue			clearColor			{ { { 0.5f, 0.5f, 0.5f, 0.5f } } };
	/*
	ZRenderPass					renderPass			= createColorRenderPass(cs.device, {format}, {{clearColor}},
														VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
														{
															ZSubpassDependency::makeBegin(),
															ZSubpassDependency(VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
																				VK_ACCESS_COLOR_ATTACHMENT_READ_BIT,
																				VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
																				VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
																				ZSubpassDependency::Between),
															ZSubpassDependency(VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
																				VK_ACCESS_COLOR_ATTACHMENT_READ_BIT,
																				VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
																				VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
																				ZSubpassDependency::Between),
															ZSubpassDependency::makeEnd()
														});*/
	ZRenderPass					renderPass			= createColorRenderPass(cs.device, { format }, { {clearColor} });
	ZRenderPass					renderPassA			= createColorRenderPass(cs.device, { format }, {}, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

#ifdef _MSC_VER
#pragma warning(disable: 4324) // structure was padded due to alignment specifier
#endif
	struct Uniform
	{
		struct alignas(16) { float _; } y;
		Mat4  offset;
		Mat4  scale;
		struct
		{
			Vec4  color;
			struct alignas(16) { float _; } pointSize;
		} x[3];
	}
#ifdef _MSC_VER
#pragma warning(default: 4324) // structure was padded due to alignment specifier
#endif
								uni					{ { 177.03f },
													  Mat4::translate(Vec4(-params.minusX, -params.minusY)),
													  Mat4::scale(Vec4(params.plusX != params.minusX
																	   ? 1.0f / (params.plusX-params.minusX)
																	   : 1.0f,
																	   params.plusY != params.minusY
																	   ? 1.0f / (params.plusY-params.minusY)
																	   : 1.0f)),
													  {
															{ Vec4(1), { params.pointSize } },
															{ Vec4(0,1,0,1), { 1.0f } },
															{ Vec4(1,0,0,1), { 7.0f } }
													  }
													};
	ZBuffer						uniBuffer			= createBuffer<Uniform>(cs.device, 1u, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
	const uint32_t				uniBinding			= lm.addBinding(uniBuffer, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	ZDescriptorSetLayout		dsLayout			= lm.createDescriptorSetLayout();
	ZPipelineLayout				pipelineLayout		= lm.createPipelineLayout({dsLayout});
	lm.writeBinding(uniBinding, uni);
	const ZQueryPool			queryPool			= createQueryPool(cs.device, VK_QUERY_TYPE_PIPELINE_STATISTICS,
																	VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_VERTICES_BIT
																	| VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_PRIMITIVES_BIT);
	ZBuffer						queryResults		= createBuffer<uint64_t[2]>(cs.device);
	bufferWrite<uint64_t>(queryResults, 123, 1);
	bufferWrite<uint64_t>(queryResults, 456, 0);
	uint64_t queryData[2];
	bufferRead(queryResults, queryData);
	std::cout << queryData[0] << ' ' << queryData[1] << std::endl;
	const VkFrontFace			frontFace			= params.ccwFronFace ? VK_FRONT_FACE_COUNTER_CLOCKWISE : VK_FRONT_FACE_CLOCKWISE;
	ZPipeline					primitivePipeline	= createGraphicsPipeline(pipelineLayout, renderPass,
														vertShaderModule, fragShaderModule,
														makeExtent2D(100,100), vertexInput.binding(0), gpp::SubpassIndex(0),
														VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR,
														params.topo, frontFace, gpp::PrimitiveRestart(true));
	ZPipeline					linesPipeline		= createGraphicsPipeline(pipelineLayout, renderPass,
														vertShaderModule, fragShaderModule,
														makeExtent2D(100,100), vertexInput.binding(0), gpp::SubpassIndex(0),
														VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR,
														VK_PRIMITIVE_TOPOLOGY_LINE_STRIP, frontFace);
	ZPipeline					pointsPipeline		= createGraphicsPipeline(pipelineLayout, renderPass,
														vertShaderModule, fragShaderModule,
														makeExtent2D(100,100), vertexInput.binding(0), gpp::SubpassIndex(0),
														VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR,
														VK_PRIMITIVE_TOPOLOGY_POINT_LIST, frontFace);
	UserData userData(params, vertexBuffer, indexBuffer);
	cs.events().cbKey.set(onKey, &userData);
	cs.events().cbScroll.set(onScroll, &userData);
	cs.events().cbWindowSize.set(onResize, &userData);
	cs.events().cbCursorPos.set(onCursorPos, &userData);
	cs.events().cbMouseButton.set(onMouseBtn, &userData);

	auto onCommandRecording = [&](add_ref<Canvas>, add_cref<Canvas::Swapchain> sc, ZCommandBuffer cmdBuffer, ZFramebuffer framebuffer)
	{
		ZImageView			attachmentView	= framebufferGetView(framebuffer);
		ZImage				renderImage		= imageViewGetImage(attachmentView);

		commandBufferBegin(cmdBuffer);
			commandBufferBindVertexBuffers(cmdBuffer, vertexInput, {vertexBuffer});
			commandBufferBindIndexBuffer(cmdBuffer, indexBuffer);
			commandBufferSetScissor(cmdBuffer, sc);
			commandBufferSetViewport(cmdBuffer, sc);
			commandBufferResetQueryPool(cmdBuffer, queryPool);

			auto qpbi = commandBufferBeginQuery(cmdBuffer, queryPool, 0);
				auto rpbi = commandBufferBeginRenderPass(cmdBuffer, framebuffer);
					commandBufferBindPipeline(cmdBuffer, primitivePipeline);
					//vkCmdDraw(*cmdBuffer, userData.pointCount, 1u, 0u, 0u);
					vkCmdDrawIndexed(*cmdBuffer, userData.pointCount, 1, 0, 0, 0);
				commandBufferEndRenderPass(rpbi);
			commandBufferEndQuery(qpbi);

			vkCmdCopyQueryPoolResults(*cmdBuffer, *queryPool, 0, 1, *queryResults, 0, sizeof(uint64_t), 
										VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);
			
			rpbi = commandBufferBeginRenderPass(cmdBuffer, renderPassA, framebuffer);
				commandBufferBindPipeline(cmdBuffer, linesPipeline);
				vkCmdDraw(*cmdBuffer, userData.pointCount, 1u, 0u, 1u);
			commandBufferEndRenderPass(rpbi);

			rpbi = commandBufferBeginRenderPass(cmdBuffer, renderPassA, framebuffer);
				commandBufferBindPipeline(cmdBuffer, pointsPipeline);
				vkCmdDraw(*cmdBuffer, userData.pointCount, 1u, 0u, 2u);
			commandBufferEndRenderPass(rpbi);

			commandBufferMakeImagePresentationReady(cmdBuffer, renderImage);
		commandBufferEnd(cmdBuffer);
	};

	auto onAfterRecording = [&](add_ref<Canvas>)
	{
		bufferRead(queryResults, userData.queryResult);
		/*
		std::vector<uint32_t> x(20);
		bufferRead(indexBuffer, x);
		std::cout << "( " << userData.pointCount << " ) ";
		for (int i = 0; i < 20; ++i)
			//std::cout << (x[i] == INVALID_UINT32 ? make_signed(x[i]) : (-1)) << ' ';
		std::cout << x[i] << ' ';
		std::cout << std::endl;
		*/
	};

	return cs.run(onCommandRecording, renderPass, std::ref(userData.drawTrigger), {}, onAfterRecording);
}

} // unnamed namespace

template<> struct TestRecorder<TOPOLOGY>
{
	static bool record (TestRecord&);
};
bool TestRecorder<TOPOLOGY>::record (TestRecord& record)
{
	record.name = "topology";
	record.call = &prepareTests;
	return true;
}
