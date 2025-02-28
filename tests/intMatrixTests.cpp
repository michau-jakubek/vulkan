#include "intMatrixTests.hpp"
#include "vtfMatrix.hpp"
#include "vtfBacktrace.hpp"
#include "vtfContext.hpp"
#include "vtfProgramCollection.hpp"
#include "vtfZPipeline.hpp"
#include "vtfZCommandBuffer.hpp"

namespace
{
using namespace vtf;

TriLogicInt runIntMatrixSingleThread (VulkanContext& ctx, const std::string& assets);

TriLogicInt prepareTests (const TestRecord& record, const strings& cmdLineParams)
{
	UNREF(cmdLineParams);
	add_cref<GlobalAppFlags> gf = getGlobalAppFlags();
	VulkanContext ctx(record.name, gf.layers, {}, {}, nullptr, gf.apiVer);
	return runIntMatrixSingleThread(ctx, record.assets);
}

bool matrixTranslate ()
{
	Mat1x4 m14 = Vec4(); UNREF(m14);
	int p0 = 2;	int tx = 8;
	int p1 = 3;	int ty = 9;
	int p2 = 4;	int tz = 7;
	Mat4 m4		= Mat4::translate(Vec4(tx, ty, tz, 1));
	Vec4 v0		(p0, p1, p2, 1);
	Vec4 v1		= m4 * v0;
	auto e0		= float(p0 + tx);
	auto e1		= float(p1 + ty);
	auto e2		= float(p2 + tz);
	bool b4		= v1.x() == e0 && v1.y() == e1 && v1.z() == e2;
	if (!b4)
		std::cout << __func__ << ": Expected " << Vec3(e0, e1, e2) << ", got " << v1.cast<Vec3>() << std::endl;
	return b4;
}

bool matrixTranslate2 ()
{
	const Vec2 v(-1,-1);
	const Vec4 u4 =
			Mat4::rotate(Vec4(0,0,1)) *
			Mat4::translate(Vec4(-0.5,-0.5)) *
			Mat4::scale(Vec4(0.5,0.5)) *
			v.cast<Vec4>(0,1);
	const Vec3 u3 =
			Mat3::rotate(Vec3(0,0,1)) *
			Mat3::translate(Vec3(-0.5,-0.5)) *
			Mat3::scale(Vec3(0.5,0.5)) *
			v.cast<Vec3>(1);
	const Vec2 expected = v * v;
	const float tol = 0.0001f;
	const bool b4 = u4.comparePartially(expected, tol);
	const bool b3 = u3.comparePartially(expected, tol);
	if (!b4) {
		std::cout << __func__ << "(4) Expected: " << expected << ", got: " << u4 << std::endl;
	}
	if (!b3) {
		std::cout << __func__ << "(3) Expected: " << expected << ", got: " << u3 << std::endl;
	}
	return b4 && b3;
}

TriLogicInt runIntMatrixSingleThread (VulkanContext& ctx, const std::string& assets)
{
	if (!matrixTranslate() || !matrixTranslate2())
	{
		return 1;
	}
	auto					onShellCommand		= [](bool& doContinue, const vtf::strings&, std::ostream&) -> void
												{
													doContinue = false;
												};
	auto					shell				= getOrCreateUniqueShell(std::cout, onShellCommand);
	ProgramCollection		programs			(ctx.device, assets);
	programs.addFromFile(VK_SHADER_STAGE_COMPUTE_BIT, "shader.comp");
	const GlobalAppFlags	flags				(getGlobalAppFlags());
	programs.buildAndVerify(flags.vulkanVer, flags.spirvVer, flags.spirvValidate, flags.genSpirvDisassembly);
	ZShaderModule			compShaderModule	= programs.getShader(VK_SHADER_STAGE_COMPUTE_BIT);

	const uint32_t			align = 8u;
	typedef uint8_t			aligntype[align];
	LayoutManager			pl(ctx.device);
#ifdef _MSC_VER
#pragma warning(disable: 4324) // structure was padded due to alignment specifier
#endif
	struct			Binding
	{
		alignas(sizeof(aligntype)) Vec4		inVec;
		alignas(sizeof(aligntype)) Mat4		mat4;
		alignas(sizeof(aligntype)) Mat2x4	mat2x4;
		alignas(sizeof(aligntype)) uint32_t	align;	// align of
		alignas(sizeof(aligntype)) Mat3x2	mat3x2;
		struct alignas(16)		{ bool _; }	bTest;
		alignas(sizeof(aligntype)) Vec4		outVec;	// mat4 * inVec
		alignas(sizeof(aligntype)) Mat3x4	mat3x4;	// mat2x4 * mat3x2
	}						binding				{};
#ifdef _MSC_VER
#pragma warning(default: 4324) // structure was padded due to alignment specifier
#endif
	std::cout << "Default alignment: " << sizeof(aligntype)			<< std::endl;
	std::cout << "Offset inVec:      " << offsetof(Binding, inVec)	<< std::endl;
	std::cout << "Offset mat4:       " << offsetof(Binding, mat4)	<< std::endl;
	std::cout << "Offset mat2x4:     " << offsetof(Binding, mat2x4)	<< std::endl;
	std::cout << "Offset align:      " << offsetof(Binding, align)	<< std::endl;
	std::cout << "Offset mat3x2:     " << offsetof(Binding, mat3x2)	<< std::endl;
	std::cout << "Offset byte test:  " << offsetof(Binding, bTest)	<< std::endl;
	std::cout << "Offset outVex:     " << offsetof(Binding, outVec)	<< std::endl;
	std::cout << "Offset mat3x3:     " << offsetof(Binding, mat3x4)	<< std::endl;

	const uint32_t			inOutBinding	= pl.addBinding<Binding>();
	ZDescriptorSetLayout	dsLayout		= pl.createDescriptorSetLayout();
	ZPipelineLayout			pipelineLayout	= pl.createPipelineLayout({ dsLayout });
	ZPipeline				pipeline		= createComputePipeline(pipelineLayout, compShaderModule);
	ZCommandPool			cmdPool			= createCommandPool(ctx.device, ctx.computeQueue);
	ZCommandBuffer			cmdBuff			= allocateCommandBuffer(cmdPool);

	std::cout << "Create storage buffer binding: " << inOutBinding << std::endl;
	binding.align = alignof(decltype(Binding::outVec));
	binding.mat4 = Mat4::sequential();
	binding.mat2x4 = Mat2x4::sequential();
	binding.mat3x2 = Mat3x2::sequential();
	binding.inVec = Vec4(17,18,19,20);
	binding.bTest = { true };
	pl.writeBinding(inOutBinding, binding);

	commandBufferBegin(cmdBuff);
		commandBufferBindPipeline(cmdBuff, pipeline);
		commandBufferDispatch(cmdBuff);
	commandBufferEnd(cmdBuff);
	commandBufferSubmitAndWait(cmdBuff);

	auto readBinding = [&]() -> Binding
	{
		Binding result;
		pl.readBinding(inOutBinding, result);
		return result;
	};

	const Binding result = readBinding();

	uint32_t expectedAlign = uint32_t(alignof(decltype(Binding::align)));
	bool b0 = result.align == expectedAlign;
	auto expectedOutVec = result.mat4 * result.inVec;
	bool b1 = result.outVec == expectedOutVec;
	auto expectedMat3x4 = result.mat2x4 * result.mat3x2;
	bool b2 = result.mat3x4 == expectedMat3x4;
	bool b3 = !!result.bTest._ == !!binding.bTest._;


	if (!b0) {
		std::cout << "Expected outVec:" << expectedAlign << ", got:" << result.align << std::endl;
	}
	if (!b1) {
		std::cout << "Expected outVec:" << expectedOutVec << ", got:" << result.outVec << std::endl;
	}
	if (!b2) {
		std::cout << "Expected Mat3x4:" << expectedMat3x4;
		std::cout << "got:" << result.mat3x4;
	}
	if (b3) {
		std::cout << "Value of bool:true is seen as int:" << static_cast<int>(result.bTest._) << std::endl;
	} else {
		std::cout << "Expected byte test:" << static_cast<int>(binding.bTest._) << ", got:" << static_cast<int>(result.bTest._) << std::endl;
	}

	return (b0 && b1 && b2 && b3) ? 0 : 1;
}

} // unnamed namespace

template<> struct TestRecorder<INT_MATRIX>
{
	static bool record (TestRecord&);
};
bool TestRecorder<INT_MATRIX>::record (TestRecord& record)
{
	record.name = "int_matrix";
	record.call = &prepareTests;
	return true;
}
