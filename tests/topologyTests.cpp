#include "topologyTests.hpp"
#include "vtfCommandLine.hpp"
#include "vtfCanvas.hpp"
#include "vtfZRenderPass.hpp"
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
	VkBool32	ctsCases;
	VkBool32	restart;
	VkBool32	printIndices;
	VkPrimitiveTopology topo;
	std::vector<Vec2> vertices;
	uint32_t	width = 16;
	uint32_t	height = 16;
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
	, ctsCases		(VK_FALSE)
	, restart		(VK_FALSE)
	, printIndices	(VK_FALSE)
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
	Option	optFronFace { "--ccw", 0 };
	Option  optCtsCases { "--cts", 0 };
	Option  optRestart { "--restart", 0 };
	Option	optPrintIndices{ "--print-indices", 0 };
	std::vector<Option>	options { optTopo, optPlusX, optPlusY, optMinusX, optMinusY,
									optPointSize, optFronFace, optCtsCases, optRestart,
									optPrintIndices };

	Params	resultParams = defaultParams;

	if (consumeOptions(optCtsCases, options, args, sink) > 0)
	{
		resultParams.ctsCases = VK_TRUE;
	}

	if (consumeOptions(optRestart, options, args, sink) > 0)
	{
		resultParams.restart = VK_TRUE;
	}

	if (consumeOptions(optPrintIndices, options, args, sink) > 0)
	{
		resultParams.printIndices = VK_TRUE;
	}

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
		<< "  [--ccw]           use VK_FRONT_FACE_COUNTER_CLOCKWISE\n"
		<< "                      default is VK_FRONT_FACE_CLOCKWISE\n"
		<< "  [--cts]           enable CTS cases\n"
		<< "  [--restart]       enable primitive restart\n"
		<< "  [--print-indices]\n"
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

TriLogicInt runTopologyTests (add_ref<Canvas> canvas, add_cref<std::string> assets, add_cref<Params> params);
TriLogicInt runCtsCaseSecondaryInherited (add_ref<Canvas> canvas, add_cref<std::string> assets, add_cref<Params> params);

bool isTopologyList (const VkPrimitiveTopology topo)
 {
	return topo == VK_PRIMITIVE_TOPOLOGY_POINT_LIST
		|| topo == VK_PRIMITIVE_TOPOLOGY_LINE_LIST
		|| topo == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST
		|| topo == VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY
		|| topo == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY;
}
TriLogicInt prepareTests (const TestRecord& record, add_ref<CommandLine> cmdLine)
{
	add_cref<GlobalAppFlags> gf = getGlobalAppFlags();
	CanvasStyle canvasStyle = Canvas::DefaultStyle;
	canvasStyle.surfaceFormatFlags |= (VK_FORMAT_FEATURE_BLIT_SRC_BIT | VK_FORMAT_FEATURE_BLIT_DST_BIT);

	bool parseResult = true;
	std::ostream& log = std::cout;
	const Params defaultParams = printUsage(log);
	const strings cmdLineParams = cmdLine.getUnconsumedTokens();
	const Params params = readCommandLine(cmdLineParams, defaultParams, log, parseResult);
	const bool geometryRequired =	params.topo == VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY
								||	params.topo == VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY
								||	params.topo == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY
								||	params.topo == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY;
	if (false == parseResult)
	{
		return {};
	}

	auto onEnablingFeatures = [&](add_ref<DeviceCaps> caps) -> void
	{
		if (geometryRequired)
		{
			caps.addUpdateFeatureIf(&VkPhysicalDeviceFeatures::geometryShader).checkSupported("geometryShader");
		}

		if (params.restart)
		{
			if (isTopologyList(params.topo))
			{
				caps.requiredExtension.emplace_back(VK_EXT_PRIMITIVE_TOPOLOGY_LIST_RESTART_EXTENSION_NAME);
				caps.addUpdateFeatureIf(&VkPhysicalDevicePrimitiveTopologyListRestartFeaturesEXT
					::primitiveTopologyListRestart).checkSupported("primitiveTopologyListRestart");
			}
		}

		caps.addUpdateFeatureIf(&VkPhysicalDeviceFeatures::pipelineStatisticsQuery).checkSupported("pipelineStatisticsQuery");
	};

	Canvas cs(record.name, gf.layers, {}, {}, canvasStyle, onEnablingFeatures, gf.apiVer, gf.debugPrintfEnabled);

	return params.ctsCases
		? runCtsCaseSecondaryInherited(cs, record.assets, params)
		: runTopologyTests (cs, record.assets, params);
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

TriLogicInt runTopologyTests (add_ref<Canvas> cs, add_cref<std::string> assets, add_cref<Params> params)
{
	params.print(std::cout);

	add_cref<ZDeviceInterface>	di				(cs.device.getInterface());
	LayoutManager				lm				(cs.device);

	auto						shaders			= buildProgram(cs.device, assets);
	ZShaderModule				vertShaderModule= shaders[0];
	ZShaderModule				fragShaderModule= shaders[1];

	VertexInput					vertexInput			(cs.device);
	ZBuffer						vertexBuffer, indexBuffer;
	std::tie(vertexBuffer, indexBuffer) = createVertexAndIndexBuffers(vertexInput, params);

	const VkFormat				format		= cs.surfaceFormat;
	const VkClearValue			clearColor	{ { { 0.5f, 0.5f, 0.5f, 0.5f } } };
	// Define framebuffer attachment layout for drawing primitives
	const std::vector<RPA>		colors		{ RPA(AttachmentDesc::Presentation, format, clearColor,
												VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL) };
	// Define framebuffer attachment layout for drawing points and lines
	const std::vector<RPA>		colorsA		{ RPA(AttachmentDesc::Presentation, format, clearColor,
												VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR) };
	// Subpass used two times
	const ZSubpassDescription2	subpass		({ RPAR(0u, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) });
	ZSubpassDependency2 dep					(VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
	ZRenderPass					renderPass	= createRenderPass(cs.device, ZAttachmentPool(colors), subpass, dep, subpass);
	ZRenderPass					renderPassA	= createRenderPass(cs.device, ZAttachmentPool(colorsA), subpass, dep, subpass);

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
	const VkFrontFace			frontFace			= params.ccwFronFace ? VK_FRONT_FACE_COUNTER_CLOCKWISE : VK_FRONT_FACE_CLOCKWISE;
	ZPipeline					primitivePipeline	= createGraphicsPipeline(pipelineLayout, renderPass,
														vertShaderModule, fragShaderModule,
														makeExtent2D(100,100), vertexInput.binding(0), gpp::SubpassIndex(0),
														VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR,
														params.topo, frontFace, gpp::PrimitiveRestart(params.restart));
	ZPipeline					linesPipeline		= createGraphicsPipeline(pipelineLayout, renderPassA,
														vertShaderModule, fragShaderModule,
														makeExtent2D(100,100), vertexInput.binding(0), gpp::SubpassIndex(0),
														VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR,
														VK_PRIMITIVE_TOPOLOGY_LINE_STRIP, frontFace);
	ZPipeline					pointsPipeline		= createGraphicsPipeline(pipelineLayout, renderPassA,
														vertShaderModule, fragShaderModule,
														makeExtent2D(100,100), vertexInput.binding(0), gpp::SubpassIndex(1),
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
			commandBufferSetViewportAndScissor(cmdBuffer, sc);
			commandBufferResetQueryPool(cmdBuffer, queryPool);

			auto qpbi = commandBufferBeginQuery(cmdBuffer, queryPool, 0);
				auto rpbi = commandBufferBeginRenderPass(cmdBuffer, framebuffer);
					commandBufferBindPipeline(cmdBuffer, primitivePipeline);
					//vkCmdDraw(*cmdBuffer, userData.pointCount, 1u, 0u, 0u);
					VTF_CALL_CHECK(di.vkCmdDrawIndexed, *cmdBuffer, userData.pointCount, 1u, 0u, 0, 0u);
					// Skip next subpass
				commandBufferEndRenderPass(rpbi);
			commandBufferEndQuery(qpbi);

			VTF_CALL_CHECK(di.vkCmdCopyQueryPoolResults, *cmdBuffer,
								*queryPool, 0, 1, *queryResults, 0, sizeof(uint64_t),
								VkFlags(VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT));
			
			rpbi = commandBufferBeginRenderPass(cmdBuffer, renderPassA, framebuffer);
				commandBufferBindPipeline(cmdBuffer, linesPipeline);
				VTF_CALL_CHECK(di.vkCmdDraw, *cmdBuffer, userData.pointCount, 1u, 0u, 1u);
				commandBufferNextSubpass(rpbi);
				commandBufferBindPipeline(cmdBuffer, pointsPipeline);
				VTF_CALL_CHECK(di.vkCmdDraw, *cmdBuffer, userData.pointCount, 1u, 0u, 2u);
			commandBufferEndRenderPass(rpbi);

		commandBufferEnd(cmdBuffer);
	};

	auto onAfterRecording = [&](add_ref<Canvas>)
	{
		bufferRead(queryResults, userData.queryResult);
	};

	return cs.run(onCommandRecording, renderPass, std::ref(userData.drawTrigger), {}, onAfterRecording);
}

ZBuffer generatePrimitives(add_ref<VertexInput> vi, add_cref<Params> params)
{
	const bool isPoints = (params.topo == VK_PRIMITIVE_TOPOLOGY_POINT_LIST);
	const bool isLineStripAdj =
		(params.topo == VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY);
	const bool isTriStripAdj =
		(params.topo == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY);
	const bool isLines =
		(params.topo == VK_PRIMITIVE_TOPOLOGY_LINE_LIST ||
			params.topo == VK_PRIMITIVE_TOPOLOGY_LINE_STRIP || isLineStripAdj);
	const bool isTriFan = (params.topo == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN);
	const float quarterWidth = (2.0f / static_cast<float>(params.width)) * 0.25f;
	const float quarterHeight = (2.0f / static_cast<float>(params.height)) * 0.25f;
	const float marginW = ((isPoints || isLines) ? quarterWidth : 0.0f);
	const float marginH = (isPoints ? quarterHeight : 0.0f);

	const uint32_t m_blockCount = 1u;
	// These coordinates will be used with different topologies, so we try to avoid drawing points on the edges.
	const float left = -1.0f + marginW;
	const float right = 1.0f - marginW;
	const float center = (left + right) / 2.0f;
	const float top = -1.0f + marginH;
	const float bottom = -1.0f + 2.0f / static_cast<float>(m_blockCount) - marginH;
	const float middle = (top + bottom) / 2.0f;

	const auto red = Vec4(1, 0, 0, 1); // tcu::RGBA::red().toVec();
	const auto green = Vec4(0, 1, 0, 1); // tcu::RGBA::green().toVec();
	const auto blue = Vec4(0, 0, 1, 1); //  tcu::RGBA::blue().toVec();
	const auto gray = Vec4(1, 1, 0, 1); // tcu::RGBA::gray().toVec();

	const bool triListSkip = false; // (params.topo == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST && params.clearOp == CLEAR_SKIP);

	std::vector<Vec4> v, c;
	// --- TOP LEFT VERTICES ---
	// For line strips with adjacency, everything is drawn with a single draw call, but we add a first and a last
	// adjacency point so the strip looks like the non-adjacency case.
	if (isLineStripAdj)
		v.emplace_back(-2.0f, -2.0f, 1.0f, 1.0f), c.push_back(red);
	v.emplace_back(left, top, 1.0f, 1.0f), c.push_back(red);
	v.emplace_back(left, middle, 1.0f, 1.0f), c.push_back(red);
	// For triangle fans we'll revert the order of the first 2 vertices in each quadrant so they form a proper fan
	// covering the whole quadrant.
	if (isTriFan)
	{
		std::swap(v.at(v.size() - 1), v.at(v.size() - 2));
		std::swap(c.at(c.size() - 1), c.at(c.size() - 2));
	}
	v.emplace_back(center, top, 1.0f, 1.0f), c.push_back(red);
	if (triListSkip)
	{
		v.emplace_back(center, top, 1.0f, 1.0f), c.push_back((red));
		v.emplace_back(left, middle, 1.0f, 1.0f), c.push_back((red));
	}
	v.emplace_back(center, middle, 1.0f, 1.0f), c.push_back((red));

	// --- BOTTOM LEFT VERTICES ---
	v.emplace_back(left, middle, 1.0f, 1.0f), c.push_back((green));
	v.emplace_back(left, bottom, 1.0f, 1.0f), c.push_back((green));
	if (isTriFan)
	{
		std::swap(v.at(v.size() - 1), v.at(v.size() - 2));
		std::swap(c.at(c.size() - 1), c.at(c.size() - 2));
	}
	v.emplace_back(center, middle, 1.0f, 1.0f), c.push_back((green));
	if (triListSkip)
	{
		v.emplace_back(center, middle, 1.0f, 1.0f), c.push_back((green));
		v.emplace_back(left, bottom, 1.0f, 1.0f), c.push_back((green));
	}
	v.emplace_back(center, bottom, 1.0f, 1.0f), c.push_back((green));

	// --- TOP RIGHT VERTICES ---
	v.emplace_back(center, top, 1.0f, 1.0f), c.push_back((blue));
	v.emplace_back(center, middle, 1.0f, 1.0f), c.push_back((blue));
	if (isTriFan)
	{
		std::swap(v.at(v.size() - 1), v.at(v.size() - 2));
		std::swap(c.at(c.size() - 1), c.at(c.size() - 2));
	}
	v.emplace_back(right, top, 1.0f, 1.0f), c.push_back((blue));
	if (triListSkip)
	{
		v.emplace_back(right, top, 1.0f, 1.0f), c.push_back((blue));
		v.emplace_back(center, middle, 1.0f, 1.0f), c.push_back((blue));
	}
	v.emplace_back(right, middle, 1.0f, 1.0f), c.push_back((blue));

	// --- BOTTOM RIGHT VERTICES ---
	v.emplace_back(center, middle, 1.0f, 1.0f), c.push_back((gray));
	v.emplace_back(center, bottom, 1.0f, 1.0f), c.push_back((gray));
	if (isTriFan)
	{
		std::swap(v.at(v.size() - 1), v.at(v.size() - 2));
		std::swap(c.at(c.size() - 1), c.at(c.size() - 2));
	}
	v.emplace_back(right, middle, 1.0f, 1.0f), c.push_back((gray));
	if (triListSkip)
	{
		v.emplace_back(right, middle, 1.0f, 1.0f), c.push_back((gray));
		v.emplace_back(center, bottom, 1.0f, 1.0f), c.push_back((gray));
	}
	v.emplace_back(right, bottom, 1.0f, 1.0f), c.push_back((gray));
	if (isLineStripAdj)
		v.emplace_back(2.0f, 2.0f, 1.0f, 1.0f), c.push_back((red));

	vi.binding(0).addAttributes(v, c);

	ZBuffer indexBuffer;
	if (params.restart)
	{
		std::vector<uint32_t> i;
		const uint32_t firstVertex = isLineStripAdj ? 1u : 0u;
		constexpr uint32_t restartVal = INVALID_UINT32;
		const uint32_t segmentCount = 4u;
		const uint32_t vps = 4u;

		for (uint32_t seg = 0; seg < segmentCount; ++seg)
		{
			uint32_t base = firstVertex + seg * vps;

			if (isTriStripAdj)
			{
				// Schema: [A, V0, A, V1, A, V2, A, V3]
				// If A then neighbor (duplicating V)
				i.push_back(base + 0); i.push_back(base + 0); // Adj, Geom
				i.push_back(base + 1); i.push_back(base + 1); // Adj, Geom
				i.push_back(base + 2); i.push_back(base + 2); // Adj, Geom
				i.push_back(base + 3); i.push_back(base + 3); // Adj, Geom
			}
			else if (isLineStripAdj)
			{
				// Schema: [Adj, V0, V1, Adj]
				i.push_back(base - 1); // previous Adj
				i.push_back(base + 0);
				i.push_back(base + 1);
				i.push_back(base + 2);
				i.push_back(base + 3);
				i.push_back(base + 4); // next Adj
			}
			else if (params.topo == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN)
			{
				// Invert pivot - drawing as COUNTER_CLOCKWISE
				i.push_back(base + 1);
				i.push_back(base + 0);
				i.push_back(base + 3);
				i.push_back(base + 2);
			}
			else
			{
				for (uint32_t vx = 0; vx < vps; ++vx)
					i.push_back(base + vx);
			}

			if (seg < segmentCount - 1u)
				i.push_back(restartVal);
		}
		indexBuffer = createBuffer<uint32_t>(vi.device, data_count(i), ZBufferUsageFlags(VK_BUFFER_USAGE_INDEX_BUFFER_BIT));
		bufferWrite(indexBuffer, i);
	}
	return indexBuffer;
}
std::array<ZShaderModule, 2> genShaders(ZDevice device, add_cref<std::string> assets, add_cref<Params>)
{
	ProgramCollection pc(device, assets);
	pc.addFromFile(VK_SHADER_STAGE_VERTEX_BIT, "cts.vert");
	pc.addFromFile(VK_SHADER_STAGE_FRAGMENT_BIT, "cts.frag");
	pc.buildAndVerify(true);
	return
	{
		pc.getShader(VK_SHADER_STAGE_VERTEX_BIT),
		pc.getShader(VK_SHADER_STAGE_FRAGMENT_BIT),
	};
}
void draw(add_ref<Canvas> cs, ZImage img, add_cref<Params> params);
TriLogicInt runCtsCaseSecondaryInherited (add_ref<Canvas> cs, add_cref<std::string> assets, add_cref<Params> params)
{
	const VkPrimitiveTopology restartTopologies[] = {
		VK_PRIMITIVE_TOPOLOGY_LINE_STRIP,
		VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
		VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN,
		VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY,
		VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY,
	};
	if (const auto rtp = std::find(std::begin(restartTopologies), std::end(restartTopologies), params.topo);
		rtp == std::end(restartTopologies))
	{
		std::ostringstream msg;
		msg << "Input topology " << params.topo << " must be one of ";
		for (uint32_t i = 0; i < ARRAY_LENGTH_CAST(restartTopologies, uint32_t); ++i) {
			if (ARRAY_LENGTH_CAST(restartTopologies, uint32_t) - 1 == i)
				msg << " or ";
			else if (i)
				msg << ", ";
			msg << restartTopologies[i];
		}
		msg << std::endl;
		ASSERTFALSE(msg.str());
	}

	add_cref<ZDeviceInterface>	di					= cs.device.getInterface();
	ZCommandPool				cmdPool				= createCommandPool(cs.device, cs.graphicsQueue);
	LayoutManager				lm					(cs.device, cmdPool);
	VertexInput					vi					(cs.device);
	ZBuffer						indexBuffer			= generatePrimitives(vi, params);
	const uint32_t				indexCount			= params.restart ? bufferGetElementCount<uint32_t>(indexBuffer) : 0u;
	auto						[vs, fs]			= genShaders(cs.device, assets, params);

	const uint32_t				queryCount			= 6;
	const VkFormat				format				= VK_FORMAT_R32G32B32A32_SFLOAT;
	const VkClearValue			clearColor			{ { { 0.5f, 0.5f, 0.5f, 0.5f } } };
	const std::vector<RPA>		colors				{ RPA(AttachmentDesc::Color, format, clearColor,
															VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR) };
	const ZAttachmentPool		attachmentPool		(colors);
	const ZSubpassDescription2	subpass				({ RPAR(0u, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) });
	ZRenderPass					renderPass			= createRenderPass(cs.device, attachmentPool, subpass);
	ZImage						image				= cs.createColorImage2D(format, params.width, params.height);
	ZImageView					view				= createImageView(image);
	ZFramebuffer				frameBuffer			= createFramebuffer(renderPass, params.width, params.height, { view });
	ZPipelineLayout				pipelineLayout		= lm.createPipelineLayout();
	const VkFrontFace			frontFace			= params.ccwFronFace ? VK_FRONT_FACE_COUNTER_CLOCKWISE : VK_FRONT_FACE_CLOCKWISE;
	ZPipeline					renderPipeline		= createGraphicsPipeline(pipelineLayout, renderPass,
															vi, vs, fs, makeExtent2D(params.width, params.height),
															params.topo, gpp::PrimitiveRestart(params.restart),
															frontFace);
	ZQueryPool					queryPool			= createQueryPool(cs.device, VK_QUERY_TYPE_PIPELINE_STATISTICS,
														VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_VERTICES_BIT
														| VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_PRIMITIVES_BIT, queryCount);
	ZBuffer						queryResults		= createBuffer<uint64_t[queryCount]>(cs.device);

	ZCommandBuffer				priCmd				= allocateCommandBuffer(cmdPool);
	std::vector<ZCommandBuffer> secCmds				(queryCount);
	for (uint32_t q = 0; q < queryCount; ++q)
	{
		secCmds[q] = allocateCommandBuffer(cmdPool, false);
	}
	{
		std::vector<uint64_t> queryData(queryCount);
		std::fill(queryData.begin(), queryData.end(), INVALID_UINT64);
		bufferWrite(queryResults, queryData);
	}

	commandBufferBegin(priCmd);
	commandBufferBindVertexBuffers(priCmd, vi);
	if (params.restart)
		commandBufferBindIndexBuffer(priCmd, indexBuffer);
	commandBufferBindPipeline(priCmd, renderPipeline);
	commandBufferResetQueryPool(priCmd, queryPool);
		auto qpbi = commandBufferBeginQuery(priCmd, queryPool, 0);
			auto rpbi = commandBufferBeginRenderPass(priCmd, frameBuffer);
				if (params.restart)
					di.vkCmdDrawIndexed(*priCmd, indexCount, 1u, 0u, 0, 0u);
				else
					di.vkCmdDraw(*priCmd, vi.getVertexCount(0), 1u, 0u, 0u);
			commandBufferEndRenderPass(rpbi);
		commandBufferEndQuery(qpbi);
	di.vkCmdCopyQueryPoolResults(
		*priCmd, *queryPool, 0u, 1u, *queryResults, 0u, sizeof(uint64_t),
			uint32_t(VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT));
	commandBufferEnd(priCmd);
	commandBufferSubmitAndWait(priCmd);

	std::cout << "Applied topology: " << params.topo << std::endl;

	if (params.printIndices && params.restart)
	{
		std::vector<VecX<float, 8>> vertices;
		bufferRead(vi.binding(0).getBuffer(), vertices);

		std::vector<uint32_t> indices;
		bufferRead(indexBuffer, indices);
		ASSERTMSG(indices.size() == indexCount, "Invalid index count");
		ASSERTMSG(vertices.size() != 0 && vertices.size() <= indices.size(), "Invalid vertex count");

		std::cout << "Indices that was used to draw (" << indexCount << "):\n";
		for (uint32_t i = 0; i < indexCount; ++i)
		{
			if (i) std::cout << ", ";
			const uint32_t id = indices[i];
			if (id == INVALID_UINT32)
				std::cout << "-1";
			else
			{
				ASSERTMSG(id < vertices.size(), "Index (", id, ") of bounds (", vertices.size(), ")");
				std::cout << '[' << id << "]" << vertices[id].cast<Vec2>()
					<< '{' << vertices[id][4] << ',' << vertices[id][5] << ',' << vertices[id][6] << '}';
			}
		}
		std::cout << std::endl;
	}

	std::vector<uint64_t> queryData;
	bufferRead(queryResults, queryData);
	std::cout << "Vertices: " << queryData[0] << ", primitives: " << queryData[1] << std::endl;

	draw(cs, image, params);

	return queryData[0] ? 0 : 1;
}

void draw(add_ref<Canvas> cs, ZImage img, add_cref<Params> params)
{
	const VkFormat				format			= cs.surfaceFormat;
	const VkClearValue			clearColor		{ { { 0.5f, 0.5f, 0.5f, 0.5f } } };
	const std::vector<RPA>		colors			{ RPA(AttachmentDesc::Presentation, format, clearColor,
														VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR) };
	const ZAttachmentPool		attachmentPool	(colors);
	const ZSubpassDescription2	subpass			({ RPAR(0u, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) });
	ZRenderPass					renderPass		= createRenderPass(cs.device, attachmentPool, subpass);

	{
		std::ostringstream title;
		title << params.topo;
		glfwSetWindowTitle(*cs.window, title.str().c_str());
	}

	auto onCommandRecording = [&](add_ref<Canvas>, add_cref<Canvas::Swapchain>,
		ZCommandBuffer cmd, ZFramebuffer fb)
		{
			commandBufferBegin(cmd);
			commandBufferBlitImageWithBarriers(cmd, img, framebufferGetImage(fb),
				VK_ACCESS_NONE, VK_ACCESS_NONE,
				VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
				VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_FILTER_NEAREST);
			commandBufferEnd(cmd);
		};

	int drawTrigger = 1;
	cs.events().setDefault(drawTrigger);
	cs.run(onCommandRecording, renderPass, std::ref(drawTrigger));
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
