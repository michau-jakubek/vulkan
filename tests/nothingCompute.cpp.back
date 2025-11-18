#include "nothingCompute.hpp"
#include "vtfBacktrace.hpp"
#include "vtfContext.hpp"
#include "vtfProgramCollection.hpp"
#include "vtfDSBMgr.hpp"
#include "vtfZPipeline.hpp"
#include "vtfZCommandBuffer.hpp"
#include "vtfFloat16.hpp"
#include "vtfZUtils.hpp"
#include "vtfVertexInput.hpp"
#include <numeric>
#include "demangle.hpp"

namespace
{
using namespace vtf;

#ifdef _MSC_VER
// warning C4324: '': structure was padded due to alignment specifier
#pragma warning(disable: 4324)
#endif
struct alignas(4) AlignedFloat16_t : public Float16
{
	using Float16::Float16;
	uint32_t count() const {
		return 1;
	}
	AlignedFloat16_t& operator[](uint32_t) {
		return *this;
	}
	const AlignedFloat16_t& operator[](uint32_t) const {
		return *this;
	}
};
struct AlignedF16Vec2 : public VecX<Float16, 2>
{
	using VecX<Float16, 2>::VecX;
};
struct alignas(8) AlignedF16Vec3 : public VecX<Float16, 3>
{
	using VecX<Float16, 3>::VecX;
};
struct AlignedF16Vec4 : public VecX<Float16, 4>
{
	using VecX<Float16, 4>::VecX;
};

#ifdef _MSC_VER
// warning C4324: '': structure was padded due to alignment specifier
#pragma warning(default: 4324)
#endif

static_assert(sizeof(Float16) == sizeof(uint16_t), "???");

/*
auto makeStructImpl(const std::tuple<>&)
{
	struct S {};
	return S{};
}

template<class Base, class Field> struct makeInherited
{
	struct alignas(4) Temp : Base
	{
		//Base base;
		Field field;
	};
	typedef Temp type;
};

template<class X, class... Y> auto makeStructImpl(const std::tuple<X, Y...>&)
{
	//typedef decltype(makeStructImpl(std::tuple<Y...>())) T;
	auto u = makeStructImpl(std::tuple<Y...>());
	typedef typename makeInherited<decltype(u), X>::type S;
	std::cout << "Sizeof(T): " << sizeof(u) << std::endl;
	std::cout << "Sizeof(S): " << sizeof(S) << std::endl;
	std::cout << demangledName<X>() << std::endl;
	return S{};
}

template<class... X> auto makeStruct(const std::tuple<X...>& t)
{
	return makeStructImpl(t);
}
*/
template<class FVec> void fillFloat16 (FVec& vec, add_ref<float> val)
{
	for (uint32_t i = 0u; i < vec.count(); ++i)
	{
		vec[i] = val;
		val += 1.0f;
	}
}

template<class FVec> void printFloat16 (const FVec& val, add_cref<std::string> name, add_ref<std::ostream> str)
{
	static const char comps[]{ 'x', 'y', 'z', 'w' };
	{
		if (val.count() == 1)
		{
			str << name << " = " << val[0] << std::endl;
		}
		else
		{
			for (uint32_t i = 0; i < val.count(); ++i)
				str << name << "." << comps[i] << " = " << val[i] << std::endl;
		}
	}
}

TriLogicInt runTest (const TestRecord& record, const strings& cmdLineParams)
{
	UNREF(cmdLineParams);
	add_cref<GlobalAppFlags> gf(getGlobalAppFlags());

	auto onEnablingFeatures = [](add_ref<DeviceCaps> caps)
	{
		caps.requiredExtension.push_back("VK_KHR_shader_float16_int8");
		caps.addUpdateFeatureIf(&VkPhysicalDevice16BitStorageFeatures::storageBuffer16BitAccess);
		caps.addUpdateFeatureIf(&VkPhysicalDevice16BitStorageFeatures::uniformAndStorageBuffer16BitAccess);
		caps.addUpdateFeatureIf(&VkPhysicalDeviceVulkan12Features::shaderFloat16);
	};

	VulkanContext ctx(record.name, gf.layers, {}, {}, onEnablingFeatures, gf.apiVer, gf.debugPrintfEnabled);

	ProgramCollection		programs	(ctx.device, record.assets);
	programs.addFromFile(VK_SHADER_STAGE_COMPUTE_BIT, "shader.comp");
	programs.buildAndVerify(true);
	ZShaderModule			shader		= programs.getShader(VK_SHADER_STAGE_COMPUTE_BIT);

	/*
	typedef std::tuple<
		AlignedFloat16_t,
		AlignedF16Vec2,
		AlignedF16Vec3,
		AlignedF16Vec4,
		AlignedF16Vec4,
		AlignedFloat16_t,
		AlignedFloat16_t,
		AlignedF16Vec2,
		AlignedFloat16_t,
		AlignedF16Vec2,
		AlignedFloat16_t
	> InTuple;


	//typedef std::tuple<int, char, double> InTuple;
	auto inTuple = makeStruct(InTuple());
	std::cout << "SIZEOF 1: " << sizeof(inTuple) << std::endl;
	std::cout << "SIZEOF 1: " << sizeof(InTuple) << std::endl;
	std::cout << demangledName(inTuple) << std::endl;
	*/

	LayoutManager				lm(ctx.device);
	using type = Float16;
	struct In
	{
		AlignedFloat16_t p;
		AlignedF16Vec2 q;
		AlignedFloat16_t r;
		AlignedF16Vec2 s;
		AlignedFloat16_t t;
		AlignedFloat16_t u;

		AlignedF16Vec4 a;
		AlignedF16Vec4 b;
		AlignedF16Vec3 c;
		AlignedF16Vec2 d;
		AlignedFloat16_t e;

		static void printOffsets()
		{
			std::cout << "p " << offsetof(In, p) << std::endl;
			std::cout << "q " << offsetof(In, q) << std::endl;
			std::cout << "r " << offsetof(In, r) << std::endl;
			std::cout << "s " << offsetof(In, s) << std::endl;
			std::cout << "t " << offsetof(In, t) << std::endl;
			std::cout << "u " << offsetof(In, u) << std::endl;
			std::cout << "a " << offsetof(In, a) << std::endl;
			std::cout << "b " << offsetof(In, b) << std::endl;
			std::cout << "c " << offsetof(In, c) << std::endl;
			std::cout << "d " << offsetof(In, d) << std::endl;
			std::cout << "e " << offsetof(In, e) << std::endl;
		}

		static void update(uint32_t binding, add_ref<LayoutManager> man)
		{
			In in; float k = 1.0;
			fillFloat16(in.p, k);
			fillFloat16(in.q, k);
			fillFloat16(in.r, k);
			fillFloat16(in.s, k);
			fillFloat16(in.t, k);
			fillFloat16(in.a, k);
			fillFloat16(in.b, k);
			fillFloat16(in.c, k);
			fillFloat16(in.d, k);
			fillFloat16(in.e, k);

			printOffsets();

			std::cout << "UPDATE >>>>>>>" << std::endl;
			for (uint32_t i = 0; i < (sizeof(in) / sizeof(type)); ++i)
			{
				std::cout << i << ": " << ((type*)&in)[i] << std::endl;
			}
			std::cout << "<<<<<<< UPDATE" << std::endl;
			man.writeBinding(binding, in);
		}
		static void print(add_ref<LayoutManager> man, uint32_t bind, add_ref<std::ostream> str, bool raw)
		{
			printOffsets();

			if (raw)
			{
				std::vector<Float16> v(ROUNDUP(sizeof(In) / sizeof(Float16), 4));
				man.readBinding(bind, v);
				std::cout << "RAW <<<<<<<" << std::endl;
				for (uint32_t i = 0; i < v.size(); ++i)
				{
					str << i << ": " << v[i] << std::endl;
				}
				std::cout << ">>>>>>> RAW" << std::endl;
			}
			else
			{
				In in;
				if (sizeof(in) != man.readBinding(bind, in))
				{
					std::cout << "DIFF" << std::endl;
				}
				std::cout << "FLOAT <<<<<<<" << std::endl;
				printFloat16(in.p, "p", str);
				str << std::endl;
				printFloat16(in.q, "q", str);
				str << std::endl;
				printFloat16(in.r, "r", str);
				str << std::endl;
				printFloat16(in.s, "s", str);
				str << std::endl;
				printFloat16(in.t, "t", str);
				str << std::endl;
				printFloat16(in.a, "a", str);
				str << std::endl;
				printFloat16(in.b, "b", str);
				str << std::endl;				
				printFloat16(in.c, "c", str);
				str << std::endl;
				printFloat16(in.d, "d", str);
				str << std::endl;
				printFloat16(in.e, "e", str);
				std::cout << ">>>>>>> FLOAT" << std::endl;
				str << std::endl;
			}
		}
	};

	std::cout << "SIZEOF 2: " << sizeof(In) << std::endl;

	const uint32_t			inBinding0 = lm.addBinding<In>();
	const uint32_t			outBinding = lm.addBindingAsVector<type>(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 256);
	const uint32_t			cloneInBinding = lm.cloneBindingAsVector<type>(inBinding0); UNREF(cloneInBinding);
	const uint32_t			cloneOutBinding = lm.cloneBindingAsVector<type>(outBinding); UNREF(cloneOutBinding);
	ZDescriptorSetLayout	dsLayout = lm.createDescriptorSetLayout();
	ZPipelineLayout			pipelineLayout = lm.createPipelineLayout({ dsLayout }, ZPushRange<uint32_t>());
	ZPipeline				pipeline = createComputePipeline(pipelineLayout, shader);

	const uint32_t mode = 1;
#ifdef _MSC_VER
#pragma warning(disable: 4127)
#endif
	if (mode == 1)
#ifdef _MSC_VER
#pragma warning(default: 4127)
#endif
	{
		// w tym trybie struktura po stronie hosta jest potraktowana jak zwykla tablica,
		// a po stronie gpu zostaje skastowana do niej samej i przepisana do bufora out
		std::vector<type> v(lm.getBindingElementCount(inBinding0));
		std::iota(v.begin(), v.end(), 1.0f);
		lm.writeBinding(inBinding0, v);
	}
	else
	{
		In::update(inBinding0, lm);
	}

	{
		OneShotCommandBuffer cmd(ctx.device, ctx.computeQueue);
		commandBufferBindPipeline(cmd, pipeline);
		commandBufferPushConstants(cmd, pipelineLayout, mode);
		commandBufferDispatch(cmd);
	}

	{
		In::print(lm, outBinding, std::cout, false);
		In::print(lm, outBinding, std::cout, true);
	}

	return 0;
}

} // unnamed namespace

template<> struct TestRecorder<NOTHING_COMPUTE>
{
	static bool record(TestRecord&);
};
bool TestRecorder<NOTHING_COMPUTE>::record(TestRecord& record)
{
	record.name = "nothing_compute";
	record.call = &runTest;
	return true;
}
