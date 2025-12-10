#ifndef __VTF_OFFLINE_COMPILER_HPP_INCLUDED__
#define __VTF_OFFLINE_COMPILER_HPP_INCLUDED__

#include <sstream>
#include "vtfVkUtils.hpp"
#include "vtfFilesystem.hpp"
#include "vtfProgressRecorder.hpp"

namespace vtf
{

std::vector<char> compileShader (
	add_cref<std::string>			shaderCode,
	VkShaderStageFlagBits			shaderStage,
	add_cref<std::string>			entryPoint,
	bool							isGlsl,
	bool							enableValidation,
	add_cref<std::string>           validationOptions,
	bool							genDisassmebly,
	add_cref<Version>				vulkanVer,
	add_cref<Version>				spirvVer,
	add_cref<fs::path>				binPath,
	add_cref<fs::path>				asmPath,
	add_ref<std::vector<char>>		disassembled,
	add_ref<std::stringstream>		errorCollection,
	add_ref<vtf::ProgressRecorder>	progressRecorder,
	add_ref<bool>					status);
} // namespace vtf

#endif // __VTF_OFFLINE_COMPILER_HPP_INCLUDED__
