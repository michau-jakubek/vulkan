#ifndef __COOPERATIVE_MATRIX_TESTS_HPP_INCLUDED__
#define __COOPERATIVE_MATRIX_TESTS_HPP_INCLUDED__

#include "allTests.hpp"

#include "vtfContext.hpp"
#include "vtfCommandLine.hpp"

#include <iostream>

namespace coopmat
{
using namespace vtf;

struct CoopParams
{
	CoopParams(add_cref<std::string> assets_) : assets(assets_) {}
	bool selectMatrixProperties(ZInstance instance, ZPhysicalDevice physicalDevice, add_ref<std::ostream> str);
	add_cref<VkCooperativeMatrixPropertiesKHR> getSelectedConfiguration() const;
	add_cref<std::string> assets;
	uint32_t subgroupSize = 0;
	uint32_t Variant = 0; // A,B,C,R
	uint32_t Configuration = 0; // (0, confs.size()-1]
	std::vector<VkCooperativeMatrixPropertiesKHR> confs;
	bool buildAlways = false;
	bool useSpirvShader = false;
	bool allConfigurations = false;
	OptionParser<CoopParams> getParser();
};

enum class MatrixTargets : uint32_t
{
	ALL = 131, A, B, C, R
};

} // namespace coopmat

#endif // __COOPERATIVE_MATRIX_TESTS_HPP_INCLUDED__