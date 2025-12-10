#include "vtfOfflineCompiler.hpp"

namespace vtf
{

std::vector<char> compileShader(
    add_cref<std::string>           /*shaderCode*/,
    VkShaderStageFlagBits           /*shaderStage*/,
    add_cref<std::string>           /*entryPoint*/,
    bool                            /*isGlsl*/,
    bool                            /*enableValidation*/,
    add_cref<std::string>           /*validationOptions*/,
    bool                            /*genDisassmebly*/,
    add_cref<vtf::Version>          /*vulkanVer*/,
    add_cref<vtf::Version>          /*spirvVer*/,
    add_cref<fs::path>              /*binPath*/,
    add_cref<fs::path>              /*asmPath*/,
    add_ref<std::vector<char>>      /*disassembly*/,
    add_ref<std::stringstream>      /*errorCollection*/,
    add_ref<vtf::ProgressRecorder>  /*progressRecorder*/,
    add_ref<bool>                   /*status*/)
{
    ASSERTFALSE("Offline glslang compiler is unavailable, "
        "rebuilt your project width OFFLINE_SHADER_COMPILER enabled");
	return {};
}

} // namespace vtf
