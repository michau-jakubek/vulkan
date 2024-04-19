#include "triangleTests.hpp"
#include "vtfCanvas.hpp"
#include "vtfZImage.hpp"
#include "vtfLayoutManager.hpp"
#include "vtfProgramCollection.hpp"
#include "vtfGlfwEvents.hpp"
#include "vtfZCommandBuffer.hpp"
#include "vtfBacktrace.hpp"
#include "vtfVector.hpp"
#include "vtfZPipeline.hpp"
#include "vtfStructUtils.hpp"

#include <algorithm>
#include <functional>
#include <numeric>
#include <queue>

namespace
{
using namespace vtf;
typedef add_ref<std::ostream> ostream_ref;

ostream_ref operator<< (ostream_ref str, ostream_ref)
{
	return str;
}

struct Boolean { bool value; };
ostream_ref operator<< (ostream_ref str, add_cref<Boolean> value)
{
	return (str << std::boolalpha << value.value << std::noboolalpha);
}
Boolean boolean(bool value) { return Boolean{ value }; }

struct Params
{
	enum Status
	{
		Ok, Warning, Error, Help
	};
	int		width;
	int		height;
	float	gl_PointSize;
	struct Flags {
		uint32_t mBuildAlways       : 1;
		uint32_t mDemoteWholeQuad   : 1;
		uint32_t mSingleTriangle    : 1;
		uint32_t mColorGroup	    : 1;
		uint32_t mUseSimulateHelper : 1;
		uint32_t mPad : 28;
	} flags{ 0, 0, 0, 0, 0, 0 };
	constexpr bool	buildAlways		    () const { return (flags.mBuildAlways		!= 0); }
	constexpr bool	demoteWholeQuad	    () const { return (flags.mDemoteWholeQuad	!= 0); }
	constexpr bool	singleTriangle	    () const { return (flags.mSingleTriangle	!= 0); }
	constexpr bool	useSimulateHelper	() const { return (flags.mUseSimulateHelper	!= 0); }
	constexpr bool	colorGroup		    () const { return (flags.mColorGroup		!= 0); }
	Params ();
	std::tuple<Status, Params, std::string>
	static parseCommandLine (add_cref<strings> cmdLineParams);
	void print (add_ref<std::ostream> log) const;
	void usage (add_ref<std::ostream> log) const;
};

TriLogicInt runDemoteInvocationsSingleThread (Canvas& canvas, const std::string& assets, add_cref<Params> params);
TriLogicInt prepareTests (const TestRecord& record, const strings& cmdLineParams)
{
	const Version reqSpirv(1,3);
	if (getGlobalAppFlags().spirvVer < reqSpirv)
	{
		std::cout << "Application should be run with SPIR-V minimum " << UVec2(reqSpirv.nmajor, reqSpirv.nminor) << std::endl
				  << "Add command line parameter \'-spirv " << reqSpirv.to10xMajorPlusMinor() << "\' or higher." << std::endl;
		return (1);
	}
	VkPhysicalDeviceShaderDemoteToHelperInvocationFeatures demoteFeatures = makeVkStruct();
	VkPhysicalDeviceFeatures2 deviceFeatures = makeVkStruct(&demoteFeatures);
	auto onEnablingFatures = [&](ZPhysicalDevice device, add_ref<strings> /*deviceExtensions*/) -> VkPhysicalDeviceFeatures2
	{
		vkGetPhysicalDeviceFeatures2(*device, &deviceFeatures);
		return deviceFeatures;
	};
	auto [status, params, err] = Params::parseCommandLine(cmdLineParams);
	switch (status)
	{
		case Params::Help:
			decltype(params)().usage(std::cout);
			return (0);
		case Params::Error:
			std::cout << err;
			return (1);
		case Params::Warning:
			std::cout << err;
			break;
		case Params::Ok:
			decltype(params)().usage(std::cout);
			params.print(std::cout);
			break;
	}
    add_cref<GlobalAppFlags> gf = getGlobalAppFlags();
	CanvasStyle canvasStyle = Canvas::DefaultStyle;
	canvasStyle.surfaceFormatFlags |= (VK_FORMAT_FEATURE_BLIT_SRC_BIT | VK_FORMAT_FEATURE_BLIT_DST_BIT);
	Canvas cs(record.name, gf.layers, {}, {}, canvasStyle, onEnablingFatures, gf.apiVer);
	if (!demoteFeatures.shaderDemoteToHelperInvocation)
	{
		std::cout << "ERROR: shaderDemoteToHelperInvocation is not supported by device" << std::endl;
		return (1);
	}
	return runDemoteInvocationsSingleThread(cs, record.assets, params);
}

Params::Params ()
	: width				(16)
	, height			(16)
	, gl_PointSize		(1.0f)
{
}
std::tuple<Params::Status, Params, std::string>
Params::parseCommandLine (add_cref<strings> cmdLineParams)
{
	bool status;
	strings sink;
	strings args(cmdLineParams);
	std::stringstream errorMessage;

	Option	optHelp1	{ "-help", 0 };
	Option	optHelp2	{ "--help", 0 };
	Option	optWidth	{ "-w", 1 };
	Option	optHeight	{ "-h", 1 };
	Option	optPointSize	{"-ps", 1 };
	Option	optBuildAlways	{ "-build-always", 0 };
	Option	optDemoteQuad	{ "-demote-quad", 0 };
	Option	optColorQroup	{ "-color-group", 0 };
	Option	optSingleTriangle	{ "-single-triangle", 0 };
	Option	optUseSimulateHelper{ "-simulate-helper", 0 };
	std::vector<Option> options { optHelp1, optHelp2,
		optPointSize, optBuildAlways, optWidth, optHeight, optDemoteQuad, optUseSimulateHelper, optSingleTriangle, optColorQroup };

	Params result;
	Params::Status parseStatus = Ok;

	if ((consumeOptions(optHelp1, options, args, sink) > 0)
		|| (consumeOptions(optHelp2, options, args, sink) > 0))
	{
		return { Params::Status::Help, result, std::string() };
	}

	result.flags.mBuildAlways		= (consumeOptions(optBuildAlways, options, args, sink) > 0) ? 1 : 0;
	result.flags.mDemoteWholeQuad	= (consumeOptions(optDemoteQuad, options, args, sink) > 0) ? 1 : 0;
	result.flags.mSingleTriangle	= (consumeOptions(optSingleTriangle, options, args, sink) > 0) ? 1 : 0;
	result.flags.mColorGroup		= (consumeOptions(optColorQroup, options, args, sink) > 0) ? 1 : 0;
	result.flags.mUseSimulateHelper	= (consumeOptions(optUseSimulateHelper, options, args, sink) > 0) ? 1 : 0;

	auto consumeOption = [&](auto field, const Option& opt) -> void
	{
		if (consumeOptions(opt, options, args, sink) > 0)
		{
			const auto value = fromText(sink.back(), result.*field, status);
			if (!status)
			{
				parseStatus = Warning;
				errorMessage << "WARNING: Unable to parse " << std::quoted(opt.name)
							 << " from " << std::quoted(sink.back())
							 << ", applied default " << result.*field << std::endl;
			}
			else if (value <= 0)
			{
				parseStatus = Warning;
				errorMessage << "WARNING: " << std::quoted(opt.name) << "must be positive value"
							 << ", applied default " << result.*field << std::endl;
			}
			else result.*field = value;
		}
	};

	consumeOption(&Params::width, optWidth);
	consumeOption(&Params::height, optHeight);
	consumeOption(&Params::gl_PointSize, optPointSize);

	if (!args.empty())
	{
		parseStatus = Error;
		errorMessage << "ERROR: Unknown parameter " << std::quoted(args.at(0)) << std::endl;
	}

	return { parseStatus, result, errorMessage.str() };
}
void Params::print (add_ref<std::ostream> log) const
{
	log << "Params:\n"
		<< "  gl_PointSize:    " << gl_PointSize << std::endl
		<< "  color-group:     " << boolean(colorGroup()) << std::endl
		<< "  build-always:    " << boolean(buildAlways()) << std::endl
		<< "  single-triangle: " << boolean(singleTriangle()) << std::endl
		<< "  demote-quad:     " << boolean(demoteWholeQuad()) << std::endl
		<< "  simulate-helper: " << boolean(useSimulateHelper()) << std::endl
		<< "  width:           " << width << std::endl
		<< "  height:          " << height
		<< std::endl;
}
void Params::usage (add_ref<std::ostream> log) const
{
	log << "Requirements:\n"
		<< "  Minimum SPIR-V 1.3 must be present\n"
		<< "  shaderDemoteToHelperInvocation feature must be enabled\n"
		<< "Parameters:\n"
		<< "  [-help]             print this help\n"
		<< "  [--help]            print this help\n"
		<< "  [-w <width>]        default " << width << std::endl
		<< "  [-h <height>]       default " << height << std::endl
		<< "  [-ps <float>]       gl_PointSize, default " << gl_PointSize << std::endl
		<< "  [-single-triangle]  use single triangle instead of two, default "
								 	<< boolean(singleTriangle()) << std::endl
		<< "  [-color-group]      use color pre subgroup instead of per triangle, default "
								 	<< boolean(colorGroup()) << std::endl
		<< "  [-build-always]     force to build shaders, default "
								 	<< boolean(buildAlways()) << std::endl
		<< "  [-demote-quad]      demote whole quad instead of pixel, default "
								 	<< boolean(demoteWholeQuad()) << std::endl
		<< "  [-simulate-helper]  simulate helper invocation behaviour instead of using\n"
		<< "                      built-in gl_HelperInvocation and helperInvocation(),\n"
		<< "                      default is " << boolean(useSimulateHelper()) << std::endl
		<< "Navigation keys:\n"	  
		<< "  Mouse Left:         select and print info\n"
		<< "  Mouse Right:        select and demote underlying invocation or quad\n"
		<< "  Cursor hover:       dynamically displays informations about fragment\n"
		<< "  Left,Right,Down,Up: change pixel selection\n"
		<< "  u                   undo last selected fragment or quad\n"
		<< "  i                   print info\n"
		<< "  Escape:             quit this app"
		<< std::endl;
}

#define NUM_SUBGROUPS			0
#define SUBGROUP_SIZE			1
#define NON_HELPER_COUNT		2
#define HELPER_COUNT			3

struct Bindings
{
	uint32_t outputP;
	uint32_t inputQuads;
	uint32_t outputQuads;
	uint32_t outputDerivatives;
	uint32_t testH;
};
struct DerivativeEntry;
struct QuadPoints;
struct QuadInfo;
struct Buffers
{
	std::vector<uint32_t>			outputP;
	std::vector<QuadPoints>			inputQuads;
	std::vector<QuadInfo>			outputQuads;
	std::vector<DerivativeEntry>	outputDerivatives;
	std::vector<uint32_t>			testH;
	Buffers (add_ref<LayoutManager>, add_cref<Bindings>);
	void invalidate (add_ref<LayoutManager> lm, add_cref<Bindings>);
};
Buffers::Buffers (add_ref<LayoutManager> lm, add_cref<Bindings> b)
	: outputP			(lm.getBindingElementCount(b.outputP))
	, inputQuads		(lm.getBindingElementCount(b.inputQuads))
	, outputQuads		(lm.getBindingElementCount(b.outputQuads))
	, outputDerivatives	(lm.getBindingElementCount(b.outputDerivatives))
	, testH				(lm.getBindingElementCount(b.testH))
{
	lm.getBinding<ZBuffer>(b.outputP)->name("outputP");
	lm.getBinding<ZBuffer>(b.inputQuads)->name("inputQuads");
	lm.getBinding<ZBuffer>(b.outputQuads)->name("outputQuads");
	lm.getBinding<ZBuffer>(b.outputDerivatives)->name("outputDerivatives");
	lm.getBinding<ZBuffer>(b.testH)->name("testH");
}
void Buffers::invalidate (add_ref<LayoutManager> lm, add_cref<Bindings> b)
{
	lm.readBinding(b.outputP, outputP);
	lm.readBinding(b.inputQuads, inputQuads);
	lm.readBinding(b.outputQuads, outputQuads);
	lm.readBinding(b.outputDerivatives, outputDerivatives);
	lm.readBinding(b.testH, testH);
}

struct PostponedJobs
{
	typedef std::function<void()> Job;
	std::queue<Job> jobs;
	void postpone (Job job) { jobs.push(job); }
	void perform (add_ref<Canvas>) {
		while (!jobs.empty()) {
			jobs.front()(/* performs postponed job */);
			jobs.pop();
		}
	}
};

struct UserData
{
	struct PushConstant
	{
		const uint32_t	width;
		const uint32_t	height;
		const uint32_t	drvSize;
		const float		gl_PointSize;
		const uint32_t	primitiveStride;
		struct Flags {
			uint32_t mPerformDemoting	: 1;
			uint32_t mDemoteWholeQuad	: 1;
			uint32_t mValidQuad			: 1;
			uint32_t mUseSimulateHelper	: 1;
			uint32_t mUseColorGroup		: 1;
			uint32_t mPad				: 27;
			Flags (bool performDemoting,
				   bool demoteWholeQuad,
				   bool useSimulateHelper,
				   bool useColorGroup);
		}			flags;
		static_assert(sizeof(flags) == sizeof(int), "Sizeof of Flags type must be equal to sizeof of int");
		PushConstant (add_cref<Params> params, uint32_t primitiveCount);
	}						mPushConstant;
	int						mDrawTrigger;
	double					xCursor;
	double					yCursor;
	add_cref<Params>		mParams;
	add_ref<LayoutManager>	mLayoutManager;
	add_cref<Bindings>		mBindings;
	add_ref<Buffers>		mBuffers;
	add_ref<PostponedJobs>	mJobs;
	UserData (add_cref<Params>			params,
			  add_ref<LayoutManager>	layoutManager,
			  add_cref<Bindings>		bindings,
			  add_ref<Buffers>			buffers,
			  uint32_t					primitiveCount,
			  add_ref<PostponedJobs>	jobs);
	void generateQuad (uint32_t x, uint32_t y, bool valid);
	std::pair<bool, QuadPoints> generateQuad (int glfwNavigationKey);
	void draw () { mDrawTrigger += 1; }
	void resetQuad ();
};
struct DerivativeEntry
{
	uint32_t entryIndex;
	uint32_t fragmentID;
	uint32_t derivativeHorz;
	uint32_t derivativeVert;
	uint32_t swapHorz;
	uint32_t swapDiag;
	uint32_t swapVert;
	uint32_t broadcastResult;
	uint32_t broadcastValue;
	uint32_t clusteredMin;
	uint32_t clusteredMax;
	uint32_t pad;
	bool valid() const { return (entryIndex != 0u); }
};
static_assert(((sizeof(uint32_t) * 12) == sizeof(DerivativeEntry)), "???");
UserData::PushConstant::Flags::Flags (bool performDemoting,
									  bool demoteWholeQuad,
									  bool useSimulateHelper,
									  bool useColorGroup)
	: mPerformDemoting	    (performDemoting ? 1 : 0)
	, mDemoteWholeQuad	    (demoteWholeQuad ? 1 : 0)
	, mValidQuad		    (false)
	, mUseSimulateHelper	(useSimulateHelper ? 1 : 0)
	, mUseColorGroup	    (useColorGroup ? 1 : 0)
	, mPad				    (0)
{
}
UserData::PushConstant::PushConstant (add_cref<Params> params, uint32_t primitiveCount)
	: width				(make_unsigned(params.width))
	, height			(make_unsigned(params.height))
	, drvSize			(static_cast<uint32_t>(sizeof(DerivativeEntry) / sizeof(uint32_t)))
	, gl_PointSize		(params.gl_PointSize)
	, primitiveStride	(primitiveCount)
	, flags				(false, params.demoteWholeQuad(), params.useSimulateHelper(), params.colorGroup())
{
}
UserData::UserData (add_cref<Params>		params,
					add_ref<LayoutManager>	layoutManager,
					add_cref<Bindings>		bindings,
					add_ref<Buffers>		buffers,
					uint32_t				primitiveCount,
					add_ref<PostponedJobs>	jobs)
	: mPushConstant		(params, primitiveCount)
	, mDrawTrigger		(1)
	, xCursor			()
	, yCursor			()
	, mParams			(params)
	, mLayoutManager	(layoutManager)
	, mBindings			(bindings)
	, mBuffers			(buffers)
	, mJobs				(jobs)
{
	resetQuad();
}
struct PixelInfo
{
	uint32_t	entryIndex;
	uint32_t	fragmentID;
	uint32_t	coordX;
	uint32_t	coordY;
	uint32_t	subgroupInfo;
	uint32_t	derivativeX;
	uint32_t	derivativeY;
	uint32_t	swapHorz;
	uint32_t	swapDiag;
	uint32_t	swapVert;
	uint32_t	broadcastResult;
	uint32_t	broadcastValue;
	uint32_t	clusteredMin;
	uint32_t	clusteredMax;
	uint32_t	quadLeader;
	uint32_t	place1;
	uint32_t	place2;
	uint32_t	place3;
	uint32_t	data1;
	uint32_t	data2;
	uint32_t	data3;
	uint32_t	data4;
	uint32_t	data5;
	uint32_t	data6;
	uint32_t	data7;
	uint32_t	data8;
	uint32_t	pad0;
	uint32_t	pad1;

	enum class Properties
	{
		type,
		scalar,
		vector,

		entryIndex,
		coords,
		fragmentID,
		subgroupInfo,
		subgroupID,
		subgroupInvocationID,
		helper,
		derivative,
		swap,
		broadcast,
		clustered,
		quadLeader,
		data1,
		data2,
	};

	template<Properties> auto property () const;
	bool validEntry() const { return entryIndex != 0u; }
};
static_assert((sizeof(PixelInfo) == (28 * sizeof(uint32_t))), "???");
template<> auto PixelInfo::property<PixelInfo::Properties::fragmentID> () const
{
	return fragmentID;
}
template<> auto PixelInfo::property<PixelInfo::Properties::coords> () const
{
	return UVec2(coordX, coordY);
}
template<> auto PixelInfo::property<PixelInfo::Properties::subgroupID> () const
{
	return uint32_t(((subgroupInfo >> 16) & 0x7FFF) - 1u);
}
template<> auto PixelInfo::property<PixelInfo::Properties::subgroupInvocationID> () const
{
	return uint32_t(subgroupInfo & 0xFFFF);
}
template<> auto PixelInfo::property<PixelInfo::Properties::helper> () const
{
	return bool(((subgroupInfo >> 16) & 0x8000) != 0u);
}
template<> auto PixelInfo::property<PixelInfo::Properties::subgroupInfo> () const
{
	const uint32_t sgid = (((subgroupInfo >> 16) & 0x7FFF) - 1u);
	const uint32_t siid = (subgroupInfo & 0xFFFF);
	return UVec2(sgid, siid);
}
template<> auto PixelInfo::property<PixelInfo::Properties::derivative> () const
{
	return UVec2(derivativeX, derivativeY);
}
template<> auto PixelInfo::property<PixelInfo::Properties::swap>() const
{
	return UVec3(swapHorz, swapVert, swapDiag);
}
template<> auto PixelInfo::property<PixelInfo::Properties::broadcast>() const
{
	return UVec2(broadcastResult, broadcastValue);
}
template<> auto PixelInfo::property<PixelInfo::Properties::clustered>() const
{
	return UVec2(clusteredMin, clusteredMax);
}
template<> auto PixelInfo::property<PixelInfo::Properties::quadLeader>() const
{
	return (quadLeader != 0u);
}
template<> auto PixelInfo::property<PixelInfo::Properties::entryIndex>() const
{
	return entryIndex;
}
template<> auto PixelInfo::property<PixelInfo::Properties::data1>() const
{
	return UVec4(data1, data2, data3, data4);
}
template<> auto PixelInfo::property<PixelInfo::Properties::data2>() const
{
	return UVec4(data5, data6, data7, data8);
}

struct QuadInfo
{
	PixelInfo	base;
	PixelInfo	right;
	PixelInfo	bottom;
	PixelInfo	diagonal;
	add_cref<PixelInfo> at (uint32_t i) const {
		ASSERTION(i < 4);
		const PixelInfo* a[4] { &base, &right, &bottom, &diagonal };
		return *a[i];
	}
	add_cref<PixelInfo> operator[] (uint32_t i) const {
		return at(i);
	}
};
struct QuadPoints
{
	UVec2	base;
	UVec2	right;
	UVec2	bottom;
	UVec2	diagonal;
	QuadPoints ();
	QuadPoints (uint32_t X, uint32_t Y);
	void make (uint32_t X, uint32_t Y);
};
QuadPoints::QuadPoints ()
{
	make(std::numeric_limits<uint32_t>::max() / 2u, std::numeric_limits<uint32_t>::max() / 2u);
}
QuadPoints::QuadPoints (uint32_t X, uint32_t Y)
{
	make(X, Y);
}
void QuadPoints::make (uint32_t X, uint32_t Y)
{
	base.x(X);			base.y(Y);
	right.x(X + 1u);	right.y(Y);
	bottom.x(X);		bottom.y(Y + 1u);
	diagonal.x(X + 1u);	diagonal.y(Y + 1u);
}
void UserData::resetQuad ()
{
	generateQuad(make_unsigned(mParams.width), make_unsigned(mParams.height), false);
}
std::pair<bool, QuadPoints> UserData::generateQuad (int glfwNavigationKey)
{
	bool valid = false;
	const uint32_t inputQuads_size = mLayoutManager.getBindingElementCount(mBindings.inputQuads);
	std::vector<QuadPoints> inputQuads(inputQuads_size);
	mLayoutManager.readBinding(mBindings.inputQuads, inputQuads);
	add_ref<QuadPoints> q(inputQuads.at(0));
	if (make_signed(q.base.x()) >= mParams.width || make_signed(q.base.y()) >= mParams.height)
	{
		q.make(0u, 0u);
		valid = true;
	}
	else
	{
		switch (glfwNavigationKey)
		{
		case GLFW_KEY_RIGHT:
			if ((make_unsigned(mParams.width) - q.base.x()) > 1u)
			{
				q.make(q.base.x() + 1u, q.base.y());
				valid = true;
			}
			break;
		case GLFW_KEY_DOWN:
			if ((make_unsigned(mParams.height) - q.base.y()) > 1u)
			{
				q.make(q.base.x(), q.base.y() + 1u);
				valid = true;
			}
			break;
		case GLFW_KEY_LEFT:
			if (q.base.x() > 0u)
			{
				q.make(q.base.x() - 1u, q.base.y());
				valid = true;
			}
			break;
		case GLFW_KEY_UP:
			if (q.base.y() > 0u)
			{
				q.make(q.base.x(), q.base.y() - 1u);
				valid = true;
			}
			break;
		}
	}
	if (valid)
	{
		mLayoutManager.writeBinding(mBindings.inputQuads, inputQuads);
		mPushConstant.flags.mValidQuad = 1;
	}
	return { valid, q };
}
void UserData::generateQuad (uint32_t x, uint32_t y, bool valid)
{
	const uint32_t inputQuads_size = mLayoutManager.getBindingElementCount(mBindings.inputQuads);
	std::vector<QuadPoints> inputQuads(inputQuads_size);
	mLayoutManager.readBinding(mBindings.inputQuads, inputQuads);
	inputQuads.at(0) = QuadPoints(x, y);
	mLayoutManager.writeBinding(mBindings.inputQuads, inputQuads);
	mPushConstant.flags.mValidQuad = valid ? 1 : 0;
}
void onResize (Canvas& cs, void* userData, int width, int height)
{
	UNREF(cs);
	UNREF(width);
	UNREF(height);
	auto ui = static_cast<add_ptr<UserData>>(userData);
	ui->draw();
}

void printInfo (add_ref<VulkanContext>, add_ref<UserData> userData);
void printHoverInfo (add_ref<VulkanContext>, add_ref<UserData> userData, uint32_t x, uint32_t y, bool force);
void onKey (add_ref<Canvas> cs, void* userData, const int key, int scancode, const int action, int mods)
{
	UNREF(scancode);
	UNREF(mods);
	auto ui = static_cast<add_ptr<UserData>>(userData);
	if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
	{
		glfwSetWindowShouldClose(*cs.window, GLFW_TRUE);
	}
	if (key == GLFW_KEY_U && action == GLFW_PRESS)
	{
		ui->mPushConstant.flags.mPerformDemoting = 0;
		ui->resetQuad();
		ui->draw();
	}
	if (key == GLFW_KEY_I && action == GLFW_PRESS)
	{
		printInfo(cs, *ui);
	}
	if ((action == GLFW_PRESS || action == GLFW_REPEAT) &&
		(key == GLFW_KEY_RIGHT || key == GLFW_KEY_DOWN || key == GLFW_KEY_LEFT || key == GLFW_KEY_UP))
	{
		if (const auto done = ui->generateQuad(key); done.first)
		{
			ui->mJobs.postpone(std::bind(printInfo, std::ref(cs), std::ref(*ui)));
			ui->mJobs.postpone(std::bind(printHoverInfo, std::ref(cs), std::ref(*ui),
				done.second.base.x(), done.second.base.y(), true));
			ui->draw();
		}
	}
}

void onCursorPos (Canvas& cs, void* userData, double XX, double YY)
{
	// x & y are pf integral type
	// std::cout << "x: " << x << ", y: " << y << std::endl;
	auto ui = static_cast<add_ptr<UserData>>(userData);
	ui->xCursor = XX;
	ui->yCursor = YY;
	const uint32_t x = uint32_t((XX / double(cs.width)) * ui->mParams.width);
	const uint32_t y = uint32_t((YY / double(cs.height)) * ui->mParams.height);
	printHoverInfo(cs, *ui, x, y, false);
}

void onMouseBtn (Canvas& cs, void* userData, int button, int action, int mods)
{
	UNREF(button);
	UNREF(action);
	UNREF(mods);
	auto ui = static_cast<add_ptr<UserData>>(userData);
	if (action == GLFW_PRESS)
	{
		const uint32_t x = uint32_t((ui->xCursor / double(cs.width)) * ui->mParams.width);
		const uint32_t y = uint32_t((ui->yCursor / double(cs.height)) * ui->mParams.height);
		ui->generateQuad(x, y,true);
		//std::cout << "Click at " << IVec2(ui->m_pushConstant.pixelX-1u, ui->m_pushConstant.pixelY-1u) << std::endl;
		if (button == GLFW_MOUSE_BUTTON_1)
			ui->mPushConstant.flags.mPerformDemoting = 0;
		else ui->mPushConstant.flags.mPerformDemoting = 1;

		ui->mJobs.postpone(std::bind(printInfo, std::ref(cs), std::ref(*ui)));
		ui->mJobs.postpone(std::bind(printHoverInfo, std::ref(cs), std::ref(*ui), x, y, true));
		ui->draw();
	}
}
static UVec2 hoverInfo(INVALID_UINT32, INVALID_UINT32);
static const std::string hoverOriginInfo(120, 8);
static const std::string hoverClearInfo(120, ' ');
void printHoverInfo (add_ref<VulkanContext>, add_ref<UserData> userData, uint32_t x, uint32_t y, bool force)
{
	if (!(x != hoverInfo.x() || y != hoverInfo.y() || force))
		return;
	hoverInfo.x(x);
	hoverInfo.y(y);

	ostream_ref log(std::cout);
	const uint32_t	primitiveStride = userData.mPushConstant.primitiveStride;
	userData.mBuffers.invalidate(userData.mLayoutManager, userData.mBindings);
	add_cref<std::vector<uint32_t>>			outputP				(userData.mBuffers.outputP);
	add_cref<std::vector<DerivativeEntry>>	outputDerivatives	(userData.mBuffers.outputDerivatives);

	int			primitiveID = 0;
	uint32_t	fragmentID	= (y * make_unsigned(userData.mParams.width) + x) * primitiveStride;
	uint32_t	fragmentID2 = (y * make_unsigned(userData.mParams.width) + x) * primitiveStride + 1u;
	uint32_t	subgroupInfo2,	subgroupInfo;
	uint32_t	sgid2,			sgid		;
	uint32_t	siid2,			siid		;
	bool		help2,			help		;
	if (outputDerivatives.at(fragmentID).valid())
	{
		primitiveID = 0;
		fragmentID = outputDerivatives.at(fragmentID).fragmentID;
		subgroupInfo = outputP.at(fragmentID * primitiveStride);
		sgid = ((subgroupInfo >> 16) & 0x7FFF) - 1u;
		siid = subgroupInfo & 0xFFFF;
		help = ((subgroupInfo >> 16) & 0x8000) != 0u;

		if (primitiveStride > 1u && outputDerivatives.at(fragmentID2).valid())
		{
			primitiveID = (-1);
			fragmentID2 = outputDerivatives.at(fragmentID2).fragmentID;
			subgroupInfo2 = outputP.at(fragmentID2 * primitiveStride + 1u);
			sgid2 = ((subgroupInfo2 >> 16) & 0x7FFF) - 1u;
			siid2 = subgroupInfo2 & 0xFFFF;
			help2 = ((subgroupInfo2 >> 16) & 0x8000) != 0u;
		}
		else
		{
			fragmentID2 = INVALID_UINT32;
			sgid2 = INVALID_UINT32;
			siid2 = INVALID_UINT32;
			help2 = false;
		}
	}
	else
	{
		if (primitiveStride > 1u && outputDerivatives.at(fragmentID2).valid())
		{
			primitiveID = 1;
			fragmentID = outputDerivatives.at(fragmentID2).fragmentID;
			subgroupInfo = outputP.at(fragmentID * primitiveStride + 1u);
			sgid = ((subgroupInfo >> 16) & 0x7FFF) - 1u;
			siid = subgroupInfo & 0xFFFF;
			help = ((subgroupInfo >> 16) & 0x8000) != 0u;
		}
		else
		{
			primitiveID = (-1);
			fragmentID = INVALID_UINT32;
			sgid = INVALID_UINT32;
			siid = INVALID_UINT32;
			help = false;
		}

		fragmentID2 = INVALID_UINT32;
		sgid2 = INVALID_UINT32;
		siid2 = INVALID_UINT32;
		help2 = false;
	}

	log << hoverClearInfo << hoverOriginInfo;

	if (fragmentID != INVALID_UINT32)
	{
		if (fragmentID2 != INVALID_UINT32)
		{
			log << "Triangle: " << primitiveID << ", "
				<< "Fragment: " << UVec2(fragmentID, fragmentID2) << ", "
				<< "Point: " << UVec2(x, y) << ", "
				<< "SGID: " << UVec2(sgid, sgid2) << ", "
				<< "SIID: " << UVec2(siid, siid2) << ", "
				<< "helper: " << '(' << boolean(help) << ", " << boolean(help2) << ')';
		}
		else
		{
			log << "Triangle: " << primitiveID << ", "
				<< "Fragment: " << fragmentID << ", "
				<< "Point: " << UVec2(x, y) << ", "
				<< "SGID/SIID: " << UVec2(sgid, siid) << ", "
				<< "helper: " << boolean(help);
		}
	}

	log << hoverOriginInfo;
    log.flush();
}
void printInfo (add_ref<VulkanContext>, add_ref<UserData> ui)
{
	ostream_ref log(std::cout);

	add_ref<LayoutManager> lm = ui.mLayoutManager;
	ui.mBuffers.invalidate(lm, ui.mBindings);

	add_cref<std::vector<uint32_t>> outputP(ui.mBuffers.outputP);

	const uint32_t	width			= make_unsigned(ui.mParams.width);
	const uint32_t	height			= make_unsigned(ui.mParams.height);
	const bool		demoting		= ui.mPushConstant.flags.mPerformDemoting != 0;
	const uint32_t	primitiveStride = ui.mPushConstant.primitiveStride;
	const uint32_t	dataOrigin		= width * height * primitiveStride;

	ui.mParams.print(log);

	log << "Buffer " << lm.getBinding<ZBuffer>(ui.mBindings.outputP)->name()
		<< " of size: " << outputP.size() << " content:" << std::endl;

	log << "  Drawing area:              " << UVec2(width, height) << std::endl;
	log << "  Triangle count:            " << primitiveStride << std::endl;
	if (primitiveStride == 1)
	{
		log << "  gl_NumSubgroups:           " << outputP.at(dataOrigin + NUM_SUBGROUPS) << std::endl;
		log << "  gl_SubgroupSize:           " << outputP.at(dataOrigin + SUBGROUP_SIZE) << std::endl;
		log << "  non-helper invocations:    " << outputP.at(dataOrigin + NON_HELPER_COUNT) << std::endl;
		log << "  helper invocations:        " << outputP.at(dataOrigin + HELPER_COUNT) << std::endl;
	}
	else
	{
		const uint32_t num0Subgroups	= outputP.at(dataOrigin + (primitiveStride * NUM_SUBGROUPS) + 0u);
		const uint32_t num1Subgroups	= outputP.at(dataOrigin + (primitiveStride * NUM_SUBGROUPS) + 1u);
		const uint32_t subgroup0Size	= outputP.at(dataOrigin + (primitiveStride * SUBGROUP_SIZE) + 0u);
		const uint32_t subgroup1Size	= outputP.at(dataOrigin + (primitiveStride * SUBGROUP_SIZE) + 1u);
		const uint32_t nonHelper0count	= outputP.at(dataOrigin + (primitiveStride * NON_HELPER_COUNT) + 0u);
		const uint32_t nonHelper1count	= outputP.at(dataOrigin + (primitiveStride * NON_HELPER_COUNT) + 1u);
		const uint32_t helper0count		= outputP.at(dataOrigin + (primitiveStride * HELPER_COUNT) + 0u);
		const uint32_t helper1count		= outputP.at(dataOrigin + (primitiveStride * HELPER_COUNT) + 1u);

		log << "  Summary:\n";
		log << "    non-helper invocations:  " << (nonHelper0count + nonHelper1count) << std::endl;
		log << "    helper invocations:      " << (helper0count + helper1count) << std::endl;

		log << "  Triangle 0:\n";
		log << "    gl_NumSubgroups:         " << num0Subgroups << std::endl;
		log << "    gl_SubgroupSize:         " << subgroup0Size << std::endl;
		log << "    non-helper invocations:  " << nonHelper0count << std::endl;
		log << "    helper invocations:      " << helper0count << std::endl;

		log << "  Triangle 1:\n";
		log << "    gl_NumSubgroups:         " << num1Subgroups << std::endl;
		log << "    gl_SubgroupSize:         " << subgroup1Size << std::endl;
		log << "    non-helper invocations:  " << nonHelper1count << std::endl;
		log << "    helper invocations:      " << helper1count << std::endl;
	}
	log << "  demoting enable:           " << boolean(demoting) << std::endl;

	if (0 == ui.mPushConstant.flags.mValidQuad)
	{
		log << "  selected pixel:         no\n";
	}
	else
	{
		add_cref<std::vector<QuadInfo>> outputQuads(ui.mBuffers.outputQuads);

		auto internal = [&](auto&& what, uint32_t desiredWidth, uint32_t whatWidth = 0u) -> ostream_ref
		{
			std::string s;
			std::stringstream ss;
			if constexpr (std::is_same_v<std::remove_reference_t<decltype(what)>, bool>)
				ss << boolean(std::forward<decltype(what)>(what));
			else ss << std::forward<decltype(what)>(what);
			ss >> s;
			if (0u == whatWidth)
			{
				whatWidth = (uint32_t)s.length();
			}
			const uint32_t diff = (desiredWidth >= whatWidth) ? whatWidth : desiredWidth;
			const uint32_t leading = (desiredWidth >= whatWidth) ? ((desiredWidth - whatWidth) / 2) : 0;
			const uint32_t trailing = ((leading + whatWidth) > desiredWidth) ? 0 : (desiredWidth - leading - whatWidth);
			log << std::string(leading, ' ');
			log << s.substr(0, diff);
			log << std::string(trailing, ' ');
			return log;
		};

		const std::string headerItems[4] { "base", "right", "bottom", "diagonal" };
		const std::map<PixelInfo::Properties, const std::string> fieldNames
		{
			{ PixelInfo::Properties::fragmentID,	"selected fragment:"		},
			{ PixelInfo::Properties::coords,		"selected point:"			},
			{ PixelInfo::Properties::derivative,	"derivative:"				},
			{ PixelInfo::Properties::broadcast,		"broadcast result-value:"	},
			{ PixelInfo::Properties::clustered,		"clustered min-max:"		},
			{ PixelInfo::Properties::swap,			"swap horz-vert-diag:"		},
			{ PixelInfo::Properties::entryIndex,	"this entry index:"			},
			{ PixelInfo::Properties::subgroupInfo,	"subgroup,invocation:"		},
			{ PixelInfo::Properties::quadLeader,	"quad-leader:"				},
			{ PixelInfo::Properties::helper,		"helper:"					},
			{ PixelInfo::Properties::data1,			"data1:"					},
			{ PixelInfo::Properties::data2,			"data2:"					}
		};

		const char		  invalidScalar('.');
		const std::string invalidVector2("[,]");
		const std::string invalidVector3("[,,]");

		uint32_t colHeaderMaxWidth = 0u;
		for (add_cref<std::pair<const PixelInfo::Properties, const std::string>> fieldName : fieldNames)
		{
			colHeaderMaxWidth = std::max(colHeaderMaxWidth, (uint32_t)fieldName.second.length());
		}

		for (uint32_t primitiveID = 0u; primitiveID < primitiveStride; ++primitiveID)
		{
			const uint32_t fieldWidth = 15;
			const uint32_t currentTriangle = primitiveID;
			log << std::string(colHeaderMaxWidth, ' ');
			for (uint32_t i = 0u; i < ARRAY_LENGTH(headerItems); ++i)
			{
				log << internal(headerItems[i], fieldWidth);
			}
			log << std::setw(0) << std::endl;

			if (primitiveStride > 1u)
			{
				const uint32_t numWidth = uint32_t(std::log10(currentTriangle < 1u ? 1.0 : double(currentTriangle)));
				const std::string colHeaderCaption("Triangle");
				const uint32_t leftWidth = (colHeaderMaxWidth - 1u - data_count(colHeaderCaption) - 1u - numWidth - 1u) / 2u;
				const uint32_t rightWidth = colHeaderMaxWidth - leftWidth;
				log << std::string(leftWidth, '-')
					<< ' ' << colHeaderCaption << ' ' << currentTriangle << ' '
					<< std::string(rightWidth, '-');					
			}
			else
			{
				log << std::string(colHeaderMaxWidth, ' ');
			}
			log << std::string(fieldWidth * ARRAY_LENGTH(headerItems), '-') << std::endl;

			add_cref<QuadInfo> quad = outputQuads.at(currentTriangle);

			for (PixelInfo::Properties property : { PixelInfo::Properties::subgroupInfo,
													PixelInfo::Properties::coords,
													PixelInfo::Properties::fragmentID,
													PixelInfo::Properties::swap,
													PixelInfo::Properties::derivative,
													PixelInfo::Properties::broadcast,
													PixelInfo::Properties::clustered,
													PixelInfo::Properties::quadLeader,
													PixelInfo::Properties::helper,
													PixelInfo::Properties::entryIndex,
													PixelInfo::Properties::data1,
													PixelInfo::Properties::data2 })
			{
				log << std::setw(make_signed(colHeaderMaxWidth)) << std::left << fieldNames.at(property);
				for (uint32_t i = 0u; i < ARRAY_LENGTH(headerItems); ++i)
				{
					add_cref<PixelInfo> pixel = quad[i];
					const uint32_t entryIndex = pixel.property<PixelInfo::Properties::entryIndex>();
					const bool validEntry = entryIndex != 0u;

					switch (property)
					{
					case PixelInfo::Properties::fragmentID:
						if (validEntry)
							log << internal(pixel.property<PixelInfo::Properties::fragmentID>(), fieldWidth);
						else log << internal(invalidScalar, fieldWidth);
						break;
					case PixelInfo::Properties::coords:
						if (validEntry)
							log << internal(pixel.property<PixelInfo::Properties::coords>(), fieldWidth);
						else log << internal(invalidVector2, fieldWidth);
						break;
					case PixelInfo::Properties::derivative:
						if (validEntry)
							log << internal(pixel.property<PixelInfo::Properties::derivative>(), fieldWidth);
						else log << internal(invalidVector2, fieldWidth);
						break;
					case PixelInfo::Properties::swap:
						if (validEntry)
							log << internal(pixel.property<PixelInfo::Properties::swap>(), fieldWidth);
						else log << internal(invalidVector3, fieldWidth);
						break;
					case PixelInfo::Properties::broadcast:
						if (validEntry)
							log << internal(pixel.property<PixelInfo::Properties::broadcast>(), fieldWidth);
						else log << internal(invalidVector2, fieldWidth);
						break;
					case PixelInfo::Properties::clustered:
						if (validEntry)
							log << internal(pixel.property<PixelInfo::Properties::clustered>(), fieldWidth);
						else log << internal(invalidVector2, fieldWidth);
						break;
					case PixelInfo::Properties::subgroupInfo:
						if (validEntry)
							log << internal(pixel.property<PixelInfo::Properties::subgroupInfo>(), fieldWidth);
						else log << internal(invalidVector2, fieldWidth);
						break;
					case PixelInfo::Properties::quadLeader:
						if (validEntry)
							log << internal(pixel.property<PixelInfo::Properties::quadLeader>(), fieldWidth);
						else log << internal(invalidScalar, fieldWidth);
					break;
					case PixelInfo::Properties::helper:
						if (validEntry)
							log << internal(pixel.property<PixelInfo::Properties::helper>(), fieldWidth);
						else log << internal(invalidScalar, fieldWidth);
						break;
					case PixelInfo::Properties::entryIndex:
						if (validEntry)
							log << internal((entryIndex - 1u), fieldWidth);
						else log << internal(invalidScalar, fieldWidth);
						break;
					case PixelInfo::Properties::data1:
						log << internal(pixel.property<PixelInfo::Properties::data1>(), fieldWidth);
						break;
					case PixelInfo::Properties::data2:
						log << internal(pixel.property<PixelInfo::Properties::data2>(), fieldWidth);
						break;
					default:
						SIDE_EFFECT(pixel.property<PixelInfo::Properties::subgroupInvocationID>());
						SIDE_EFFECT(pixel.property<PixelInfo::Properties::subgroupID>());
						SIDE_EFFECT(pixel.property<PixelInfo::Properties::derivative>());
						ASSERTION(0);
					}
				}
				log << std::endl;
			}

			uint32_t quadValidFlags[ARRAY_LENGTH(headerItems)];
			uint32_t quadSubgroupIDs[ARRAY_LENGTH(headerItems)];
			uint32_t quadInvocationIDs[ARRAY_LENGTH(headerItems)];
			for (uint32_t i = 0u; i < ARRAY_LENGTH(headerItems); ++i)
			{
				quadValidFlags[i] = quad[i].validEntry() ? 1u : 0u;
				if (1u == quadValidFlags[i])
				{
					quadSubgroupIDs[i] = quad[i].property<PixelInfo::Properties::subgroupID>();
					quadInvocationIDs[i] = quad[i].property<PixelInfo::Properties::subgroupInvocationID>();
				}
				else if (primitiveStride > 1)
				{
					add_cptr<QuadInfo> otherQuad = &outputQuads.at(currentTriangle);
					if (currentTriangle == 0)
						otherQuad = &outputQuads.at(currentTriangle + 1u);
					else otherQuad = &outputQuads.at(currentTriangle - 1u);

					quadSubgroupIDs[i] = otherQuad->at(i).property<PixelInfo::Properties::subgroupID>();
					quadInvocationIDs[i] = otherQuad->at(i).property<PixelInfo::Properties::subgroupInvocationID>();
				}
			}
			const bool quadSubgroupsEqual = std::all_of(std::begin(quadSubgroupIDs), std::end(quadSubgroupIDs),
				std::bind(/*&std::equal_to<uint32_t>::operator(),
						  std::equal_to<uint32_t>(),*/
					std::equal_to<>(),
					std::placeholders::_1, *std::begin(quadSubgroupIDs)));
			const uint64_t invocationSum = std::accumulate(std::begin(quadInvocationIDs), std::end(quadInvocationIDs), uint64_t());
			const uint32_t invocationMin = *std::min_element(std::begin(quadInvocationIDs), std::end(quadInvocationIDs));
			const bool quadInvocationsSequential = (((invocationMin * 2u + ARRAY_LENGTH(headerItems) - 1u) * 2u) == invocationSum);
			const uint32_t quadConsistence = std::accumulate(std::begin(quadValidFlags), std::end(quadValidFlags), 0u);
			const bool inconsistentQuad = quadConsistence != 0u && quadConsistence != 4u;
			const char inconsistentMarker = inconsistentQuad ? '*' : ' ';

			log << std::setw(30) << std::left << "quad the same subgroup:";
			if (quadConsistence > 0u)
				log << boolean(quadSubgroupsEqual) << inconsistentMarker;
			else log << invalidScalar;
			log << std::endl;

			log << std::setw(30) << std::left << "quad invocations sequential:";
			if (quadConsistence > 0u)
				log << boolean(quadInvocationsSequential) << inconsistentMarker;
			else log << invalidScalar;
			log << std::endl;

			log << std::setw(30) << std::left << "selected area is valid quad:";
			if (quadConsistence > 0u)
				log << boolean(quadSubgroupsEqual && quadInvocationsSequential) << inconsistentMarker;
			else log << invalidScalar;
			log << std::endl;
		}
	}
}

TriLogicInt runDemoteInvocationsSingleThread (Canvas& cs, const std::string& assets, add_cref<Params> params)
{
	PostponedJobs				jobs		{};
	ostream_ref					log			(std::cout);
	LayoutManager				lm			(cs.device);
	ProgramCollection			programs	(cs.device, assets);
	programs.addFromFile(VK_SHADER_STAGE_VERTEX_BIT, "shader.vert");
	programs.addFromFile(VK_SHADER_STAGE_FRAGMENT_BIT, "shader.frag");
	const GlobalAppFlags		flags		(getGlobalAppFlags());
	programs.buildAndVerify(flags.vulkanVer, flags.spirvVer, flags.spirvValidate, flags.genSpirvDisassembly, params.buildAlways());

	ZShaderModule				vertShaderModule	= programs.getShader(VK_SHADER_STAGE_VERTEX_BIT);
	ZShaderModule				fragShaderModule	= programs.getShader(VK_SHADER_STAGE_FRAGMENT_BIT);

	VertexInput					vertexInput			(cs.device);
	{
		const float X = 1.0f;
		const float Y = 1.0f;
		const float RX = params.singleTriangle() ? 3.5f : 1.0f;
		const float BY = params.singleTriangle() ? 3.5f : 1.0f;
		const std::vector<Vec2>	triangle1	{ { -X, +BY },   { -X, -Y }, { +RX, -Y } };
		const std::vector<Vec2> triangle2	{ { +RX, -Y },  { +RX, +BY }, { -X, +BY } };
		std::vector<Vec2>		vertices(triangle1);
		if (!params.singleTriangle()) vertices.insert(vertices.end(), triangle2.begin(), triangle2.end());
		vertexInput.binding(0).addAttributes(vertices);
	}
	const uint32_t				primitiveStride		= vertexInput.getAttributeCount(0) / 3;

	const VkFormat				format				= cs.surfaceFormat;
	const VkClearValue			clearColor			{ { { 0.5f, 0.5f, 0.5f, 0.5f } } };
	ZRenderPass					renderPass			= createColorRenderPass(cs.device, {format}, {{clearColor}});
	const uint32_t				srcImageWidth		= make_unsigned(params.width);
	const uint32_t				srcImageHeight		= make_unsigned(params.height);
	ZImage						srcImage			= cs.createColorImage2D(format, srcImageWidth, srcImageHeight);
	ZImageView					srcView				= createImageView(srcImage);
	const VkExtent2D			srcImageSize		= makeExtent2D(srcImageWidth, srcImageHeight);
	ZFramebuffer				srcFramebuffer		= createFramebuffer(renderPass, srcImageSize, {srcView});
	const Bindings				bindings
	{
						/* outputP */				lm.addBindingAsVector<uint32_t>(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
														(make_unsigned(params.width) * make_unsigned(params.height) * primitiveStride + 48u)),
						/* inputQuads */			lm.addBindingAsVector<QuadPoints>(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
														(make_unsigned(params.width) * make_unsigned(params.height) * 4u)),
						/* outputQuads */			lm.addBindingAsVector<QuadInfo>(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
														primitiveStride),
						/* outputDerivatives */		lm.addBindingAsVector<DerivativeEntry>(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
														((make_unsigned(params.width) + 1u) * (make_unsigned(params.height) + 1u) * primitiveStride)),
						/* testH */					lm.addBindingAsVector<uint32_t>(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
														make_unsigned(params.width * params.height)),
	};
	ZPipelineLayout				pipelineLayout		= lm.createPipelineLayout<UserData::PushConstant>(lm.createDescriptorSetLayout());
	ZPipeline					mainThreadPipeline	= createGraphicsPipeline(pipelineLayout, renderPass,
															vertexInput, vertShaderModule, fragShaderModule, srcImageSize);	
	Buffers	buffers(lm, bindings);

	UserData userData(params, lm, bindings, buffers, primitiveStride, jobs);
	cs.events().cbKey.set(onKey, &userData);
	cs.events().cbWindowSize.set(onResize, &userData);
	cs.events().cbCursorPos.set(onCursorPos, &userData);
	cs.events().cbMouseButton.set(onMouseBtn, &userData);

	auto onCommandRecording = [&](add_ref<Canvas>, add_cref<Canvas::Swapchain>, ZCommandBuffer cmdBuffer, ZFramebuffer framebuffer)
	{
		lm.fillBinding(bindings.outputP, 0u);
		lm.fillBinding(bindings.outputQuads, 0u);
		lm.fillBinding(bindings.outputDerivatives, 0u);

		ZImage dstImage = framebufferGetImage(framebuffer);
		ZImageMemoryBarrier	srcPreBlit	(srcImage, VK_ACCESS_NONE, VK_ACCESS_TRANSFER_READ_BIT,
										 VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
		ZImageMemoryBarrier	dstPreBlit	(dstImage, VK_ACCESS_NONE, VK_ACCESS_TRANSFER_WRITE_BIT,
										 VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
		commandBufferBegin(cmdBuffer);
			commandBufferBindPipeline(cmdBuffer, mainThreadPipeline);
			commandBufferPushConstants(cmdBuffer, pipelineLayout, userData.mPushConstant);
			commandBufferBindVertexBuffers(cmdBuffer, vertexInput);
			auto rpbi = commandBufferBeginRenderPass(cmdBuffer, srcFramebuffer, 0);
				vkCmdDraw(*cmdBuffer, vertexInput.getAttributeCount(0), 1, 0, 0);
			commandBufferEndRenderPass(rpbi);
			commandBufferPipelineBarriers(cmdBuffer,
										  VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
										  srcPreBlit, dstPreBlit);
			commandBufferBlitImage(cmdBuffer, srcImage, dstImage, VK_FILTER_NEAREST);
			commandBufferMakeImagePresentationReady(cmdBuffer, dstImage);
		commandBufferEnd(cmdBuffer);
	};

	const int res = cs.run(onCommandRecording, renderPass, std::ref(userData.mDrawTrigger),
						Canvas::OnIdle(), std::bind(&PostponedJobs::perform, std::ref(jobs), std::ref(cs)));

	log << hoverClearInfo << hoverOriginInfo << std::endl;

	return res;
}

} // unnamed namespace

template<> struct TestRecorder<DEMOTE_INVOCATIONS>
{
	static bool record (TestRecord&);
};
bool TestRecorder<DEMOTE_INVOCATIONS>::record (TestRecord& record)
{
	record.name = "demote_invocations";
	record.call = &prepareTests;
	return true;
}
