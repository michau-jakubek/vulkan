#ifdef ENABLE_GL

#define GLFW_INCLUDE_VULKAN
#include <GL/glad.h>
#include <GLFW/glfw3.h>
#include <algorithm>
#include <vector>
#include <iterator>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <iostream>
#include "vtfFilesystem.hpp"

#endif

#include <iterator>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <iostream>
#include <regex>

#include "vtfBacktrace.hpp"
#include "vtfZUtils.hpp"
#include "vtfProgramCollection.hpp"
#include "vtfFilesystem.hpp"
#include "vtfBacktrace.hpp"

#ifdef ENABLE_GL

static std::string shaderStageToString (VkShaderStageFlagBits stage)
{
	switch (stage)
	{
	case VK_SHADER_STAGE_VERTEX_BIT:	return "VERTEX";
	case VK_SHADER_STAGE_FRAGMENT_BIT:	return "FRAGMENT";
	case VK_SHADER_STAGE_COMPUTE_BIT:	return "COMPUTE";
	case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT:	return "CONTROL";
	case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT:	return "EVALUATION";
	case VK_SHADER_STAGE_GEOMETRY_BIT:	return "GEOMETRY";
	default: break;
	}
	ASSERTION(false);
	return std::string();
}

static GLenum shaderStageToGL (VkShaderStageFlagBits stage)
{
	switch (stage)
	{
	case VK_SHADER_STAGE_VERTEX_BIT:	return GL_VERTEX_SHADER;
	case VK_SHADER_STAGE_FRAGMENT_BIT:	return GL_FRAGMENT_SHADER;
	case VK_SHADER_STAGE_COMPUTE_BIT:	return GL_COMPUTE_SHADER;
	case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT:	return GL_TESS_CONTROL_SHADER;
	case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT:	return GL_TESS_EVALUATION_SHADER;
	case VK_SHADER_STAGE_GEOMETRY_BIT:	return GL_GEOMETRY_SHADER;
	default: break;
	}
	ASSERTION(false);
	return 0;
}

static bool checkShaderCompileStatus(GLuint shader, std::string& info)
{
	GLint ok;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
	if (!ok)
	{
		GLint len;
		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);

		std::vector<GLchar> log(len);
		glGetShaderInfoLog(shader, len, nullptr, log.data());

		info.resize(len);
		std::transform(log.begin(), log.end(), info.begin(),
				[](const auto& c) -> std::string::value_type { return c; });
	}
	return (GL_TRUE == ok);
}

static bool checkProgramLinkStatus(GLuint program, std::string& info)
{
	GLint ok;
	glGetProgramiv(program, GL_LINK_STATUS, &ok);
	if (!ok)
	{
		GLint len;
		glGetProgramiv(program, GL_INFO_LOG_LENGTH, &len);

		std::vector<GLchar> log(len);
		glGetProgramInfoLog(program, len, nullptr, log.data());

		info.resize(len);
		std::transform(log.begin(), log.end(), info.begin(),
				[](const auto& c) -> std::string::value_type { return c; });
	}
	return (GL_TRUE == ok);
}

bool verifyShaderCodeGL (const VerifyShaderCodeInfo* infos, uint32_t infoCount, std::string& error, int majorVersion, int minorVersion)
{
	constexpr size_t maxHandles = 10;

	ASSERTION(infoCount < maxHandles);
	for (uint32_t i = 0; i < infoCount; ++i) shaderStageToGL((&infos[i])->stage_);

	glfwInit();

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, majorVersion);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, minorVersion);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
	//glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);
	glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);

	GLFWwindow* window = glfwCreateWindow(256, 256, "", nullptr, nullptr); // Windowed
	//	glfwHideWindow(window);
	glfwMakeContextCurrent(window);

	gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);

	uint32_t compiledCount = 0;
	GLuint handles[maxHandles];
	for (uint32_t i = 0; i == compiledCount && i < infoCount; ++i)
	{
		std::string errorMsg;
		const VerifyShaderCodeInfo* info = &infos[i];

		auto code = info->code_.data();
		handles[i] = glCreateShader(shaderStageToGL(info->stage_));
		glShaderSource(handles[i], 1, &code, NULL);
		glCompileShader(handles[i]);
		if (checkShaderCompileStatus(handles[i], errorMsg))
			++compiledCount;
		else
		{
			error = "glCompileShader-" + shaderStageToString(info->stage_) + '\n'
					+ info->code_ + '\n' + errorMsg;
			glDeleteShader(handles[i]);
			break;
		}
	}

	if (compiledCount != infoCount)
	{
		for (uint32_t i = 0; i < compiledCount; ++i) glDeleteShader(handles[i]);
		glfwDestroyWindow(window);
		glfwTerminate();
		return false;
	}

	std::string errorMsg;
	GLuint program = glCreateProgram();
	for (uint32_t i = 0; i < compiledCount; ++i) glAttachShader(program, handles[i]);
	// Use this before a linking a program
	// glBindFragDataLocation(p, 0/*frameBufferNumber*/, "outColor");
	glLinkProgram(program);
	bool programLinkStatus = checkProgramLinkStatus(program, errorMsg);
	if (!programLinkStatus)
	{
		error = "glLinkProgram\n" + errorMsg;
	}

	glDeleteProgram(program);
	for (uint32_t i = 0; i < compiledCount; ++i) glDeleteShader(handles[i]);

	glfwDestroyWindow(window);
	glfwTerminate();

	return programLinkStatus;
}

void addProgram (ProgramCollection& programCollection,	const std::string& programName,		VkShaderStageFlagBits programType,
				 const std::string& glslFileName,		const std::string& spirvFileName,
				 const std::string& glslSource,			const std::string& spirvSource,
				 bool				verbose)
{
	std::string glsl_content;
#ifdef CTS
	std::shared_ptr<glu::ShaderSource>	sc;
#endif
	static_cast<void>(programName);
	const std::string glsl_name(glslFileName);
	std::ifstream glsl_handle(glsl_name);
	if (glsl_handle.is_open())
	{
		glsl_content = std::string((std::istreambuf_iterator<char>(glsl_handle)), std::istreambuf_iterator<char>());
		std::cout << "File \"" << glsl_name << "\" size: " << glsl_content.length() << std::endl;
	}
	else if (!glslSource.empty())
	{
		glsl_content = glslSource;
		std::cout << "Unable to open " << glsl_name << ", used inline GLSL code instead" << std::endl;
	}

	if (!glsl_content.empty())
	{
#ifdef CTS
		switch (programType)
		{
		case VK_SHADER_STAGE_VERTEX_BIT:					sc	= std::make_shared<glu::VertexSource>(glsl_content);	break;
		case VK_SHADER_STAGE_FRAGMENT_BIT:					sc	= std::make_shared<glu::FragmentSource>(glsl_content);	break;
		case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT:		sc	= std::make_shared<glu::TessellationControlSource>(glsl_content);	break;
		case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT:	sc	= std::make_shared<glu::TessellationEvaluationSource>(glsl_content); break;
		case VK_SHADER_STAGE_GEOMETRY_BIT:					sc	= std::make_shared<glu::GeometrySource>(glsl_content); break;
		default:	DE_ASSERT(false);
		}

		programCollection.glslSources.add(programName) << *sc;
#else
		programCollection.add(programType, glsl_content);
#endif
	}
	else
	{
		std::string spv_content;
		const std::string spv_name(spirvFileName);
		std::ifstream spv_handle(spv_name);
		if (spv_handle.is_open())
		{
			spv_content = std::string((std::istreambuf_iterator<char>(spv_handle)), std::istreambuf_iterator<char>());
			std::cout << "File \"" << spv_name << "\" size: " << spv_content.length() << std::endl;
		}
		else
		{
#ifdef CTS
			DE_ASSERT(!spirvSource.empty());
#else
			if (verbose)
			{
				ASSERTION(!spirvSource.empty());
			}
#endif
			spv_content = spirvSource;
			std::cout << "Unable to open " << spv_name << ", used inline SPIR-V code instead" << std::endl;
		}
#ifdef CTS
		programCollection.spirvAsmSources.add(programName)
			<< spv_content
			<< vk::SpirVAsmBuildOptions(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_4, true);
#else
		programCollection.add(programType, spv_content);
#endif
	}
}

#endif


namespace vtf
{

static const VkShaderStageFlagBits availableShaderStages[]
{
	VK_SHADER_STAGE_VERTEX_BIT,
	VK_SHADER_STAGE_FRAGMENT_BIT,
	VK_SHADER_STAGE_COMPUTE_BIT,
	VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,
	VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,
	VK_SHADER_STAGE_GEOMETRY_BIT
};

static const char* shaderStageToCommand (VkShaderStageFlagBits stage)
{
	switch (stage)
	{
	case VK_SHADER_STAGE_VERTEX_BIT:					return "vert";
	case VK_SHADER_STAGE_FRAGMENT_BIT:					return "frag";
	case VK_SHADER_STAGE_COMPUTE_BIT:					return "comp";
	case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT:		return "tesc";
	case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT:	return "tese";
	case VK_SHADER_STAGE_GEOMETRY_BIT:					return "geom";
	default: break;
	}
	ASSERTION(false);
	return 0;
}

static std::string makeFileName (uint32_t index, VkShaderStageFlagBits stage,
								 const strings& codeAndEntryAndIncludes,
								 const Version& vulkanVer, const Version& spirvVer,
								 const bool enableValidation, const bool genDisassembly,
								 const char* prefix = nullptr, const char* suffix = nullptr)
{
	const std::string fileName = codeAndEntryAndIncludes.at(ProgramCollection::StageToCode::fileName).empty()
			? shaderStageToCommand(stage)
			: fs::path(codeAndEntryAndIncludes.at(ProgramCollection::StageToCode::fileName)).filename().string();

	size_t hash = 0;
	{
		std::ostringstream ss;
		ss << index;
		ss << (codeAndEntryAndIncludes.at(ProgramCollection::StageToCode::fileName).empty()
			   ? codeAndEntryAndIncludes.at(ProgramCollection::StageToCode::shaderCode)
			   : codeAndEntryAndIncludes.at(ProgramCollection::StageToCode::fileName));
		ss << (enableValidation ? 777 : 392);
		ss << (genDisassembly ? 111 : 577);
		ss << vulkanVer.nmajor;
		ss << vulkanVer.nminor;
		ss << spirvVer.nmajor;
		ss << spirvVer.nminor;
		ss.flush();
		hash = std::hash<std::string>()(ss.str());
	}
	std::ostringstream ss;
	if (prefix) ss << prefix;
	ss << hash << '.' << fileName;
	if (suffix) ss << suffix;
	return ss.str();
}

static bool verifyIncludes (const strings& codeAndEntryAndIncludes)
{
	bool exists = true;
	for (size_t j = ProgramCollection::StageToCode::includePaths;
		 exists && j < codeAndEntryAndIncludes.size(); ++j)
	{
		const fs::path path(codeAndEntryAndIncludes.at(j));
		exists = fs::exists(path);
	}
	return exists;
}

static bool containsErrorString (const std::string& text)
{	
	return std::regex_search(text, std::regex("error:", std::regex_constants::icase));
}

static bool containsWarningString(const std::string& text)
{
	return std::regex_search(text, std::regex("warning:", std::regex_constants::icase));
}

/*
*  glslangValidator options that are used
* ========================================
*   -I<dir>     add dir to the include search path; includer's directory
*               is searched first, followed by left-to-right order of -I
*   -S <stage>  uses specified stage rather than parsing the file extension
*               choices for <stage> are vert, tesc, tese, geom, frag, or comp
*   -V[ver]     create SPIR-V binary, under Vulkan semantics; turns on -l;
*               default file name is <stage>.spv (-o overrides this)
*               'ver', when present, is the version of the input semantics,
*               which will appear in #define VULKAN ver
*               '--client vulkan100' is the same as -V100
*               a '--target-env' for Vulkan will also imply '-V'
*   -l          link all input files together to form a single module (implicitly enabled by -V)
*   -o <file>   save binary to <file>, requires a binary option (e.g., -V)
*  --target-env {vulkan1.0 | vulkan1.1 | vulkan1.2 | opengl |
*                 spirv1.0 | spirv1.1 | spirv1.2 | spirv1.3 | spirv1.4 |
*                 spirv1.5 | spirv1.6}
*                                     Set the execution environment that the
*                                     generated code will be executed in.
*                                     Defaults to:
*                                      * vulkan1.0 under --client vulkan<ver>
*                                      * opengl    under --client opengl<ver>
*                                      * spirv1.0  under --target-env vulkan1.0
*                                      * spirv1.3  under --target-env vulkan1.1
*                                      * spirv1.5  under --target-env vulkan1.2
*                                     Multiple --target-env can be specified.
*  Currently unused
* ==================
*   --spirv-dis                       output standard-form disassembly; works only
*                                     when a SPIR-V generation option is also used
*   --spirv-val                       execute the SPIRV-Tools validator
*   -e <name> | --entry-point <name>  specify <name> as the entry-point function name
*/
static auto makeCompileGlslCommand (VkShaderStageFlagBits stage,
									const strings& codeAndEntryAndIncludes,
									const Version& vulkanVer, const Version& spirvVer,
									const bool enableValidation,
									const fs::path& input, const fs::path& output) -> std::string
{
	const char* space = " ";

	const std::string entryName = codeAndEntryAndIncludes.at(ProgramCollection::StageToCode::entryName);
	UNREF(entryName);

	std::stringstream cmd;
	cmd << "glslangValidator" << space;
	for (size_t j = ProgramCollection::StageToCode::includePaths; j < codeAndEntryAndIncludes.size(); ++j)
	{
		cmd << "-I" << codeAndEntryAndIncludes.at(j) << space;
	}
	cmd << "--target-env vulkan" << vulkanVer.nmajor << "." << vulkanVer.nminor << space;
	cmd << "--target-env spirv" << spirvVer.nmajor << "." << spirvVer.nminor << space;
	if (enableValidation)
	{
		cmd << "--spirv-val ";
	}
	cmd << "-S" << space << shaderStageToCommand(stage) << space
		// ***** "-e " << entryName << space
		<< input << space
		<< "-o " << output << space
#if SYSTEM_OS_LINUX
		<< "2>&1 && ls "
#else
		<< "2>&1 && dir "
#endif
		<< output;
	return cmd.str();
}

static auto makeCompileSpvCommand (const Version& vulkanVer, const Version& spirvVer,
								   const fs::path& input, const fs::path& output) -> std::string
{
	const char* space = " ";
	std::stringstream cmd;

	cmd << "spirv-as ";
	cmd << "--target-env spv" << spirvVer.nmajor << "." << spirvVer.nminor << space;
	UNREF(vulkanVer);
	//cmd << "--target-env vulkan" << vulkanVer.nmajor << "." << vulkanVer.nminor << space;
	cmd << "-o " << output << space;
	cmd << input << space
#if SYSTEM_OS_LINUX
		<< "2>&1 && ls "
#else
		<< "2>&1 && dir "
#endif
		<< output;
	return cmd.str();
}

static auto makeBuildSpvAsm (VkShaderStageFlagBits stage,
							 const strings& codeAndEntryAndIncludes,
							 const Version& vulkanVer, const Version& spirvVer,
							 const fs::path& input, const fs::path& output) -> std::string
{
	// glslangValidator -S vert -H --target-env spirv1.4 14580917305851103956.vert.glsl >> 14580917305851103956.vert.spvasm
	const char* space = " ";
	std::stringstream cmd;

	cmd << "glslangValidator -g -H --spirv-dis" << space;
	for (size_t j = ProgramCollection::StageToCode::includePaths; j < codeAndEntryAndIncludes.size(); ++j)
	{
		cmd << "-I" << codeAndEntryAndIncludes.at(j) << space;
	}
	cmd << "--target-env vulkan" << vulkanVer.nmajor << "." << vulkanVer.nminor << space;
	cmd << "--target-env spirv" << spirvVer.nmajor << "." << spirvVer.nminor << space;
	cmd << "-S " << shaderStageToCommand(stage) << space;
	cmd << input << space << ">" << space << output << space;
	cmd << "2>&1";
	cmd.flush();
	return cmd.str();
}

static auto makeBuildSpvAsm (const fs::path& input, const fs::path& output) -> std::string
{
	std::stringstream cmd;
	cmd << "spirv-dis --comment --no-color " << input << " -o " << output << " 2>&1";
	cmd.flush();
	return cmd.str();
}

static auto makeValidateCommand (const Version& vulkanVer, const Version& spirvVer, const fs::path& output) -> std::string
{
	const char* space = " ";
	std::stringstream cmd;

	cmd << "spirv-val" << space;
	cmd << "--target-env vulkan" << vulkanVer.nmajor << "." << vulkanVer.nminor << space;
	cmd << "--target-env spv" << spirvVer.nmajor << "." << spirvVer.nminor << space;
	cmd << output << space;
	cmd << "2>&1";
	cmd.flush();
	return cmd.str();
}

static std::string makeCompilerSignature (bool glslangValidator)
{
	bool status = false;
	// As far I know glslangValidator is an alias to glslang utility
	const char* exe = glslangValidator ? "glslangValidator" : "glslang";
	const std::string locCmd(
#if SYSTEM_OS_LINUX == 1
							std::string("type ") + exe
#else
							std::string("where ") + exe
#endif
							);
	const std::string locRes = captureSystemCommandResult(locCmd.c_str(), status, '\n');
	const std::string verCmd(exe + std::string(" --version"));
	const std::string verRes = captureSystemCommandResult(verCmd.c_str(), status, '\n');
	return	"[APP] Compiler path:\n[APP] " + locRes
			+ "[APP] Compiler version:\n" + verRes;
}

static	bool verifyShaderCode (uint32_t index, VkShaderStageFlagBits stage,
							   const Version& vulkanVer, const Version& spirvVer,
							   bool enableValidation, bool genDisassmebly, bool buildAlways,
							   const strings& codeAndEntryAndIncludes, std::vector<unsigned char>& binary,
							   std::string& errors)
{
	const bool isGlsl = codeAndEntryAndIncludes[ProgramCollection::StageToCode::shaderCode]
							.find("#version") != std::string::npos;

	bool status = false;
	std::stringstream errorCollection;
	const char* tmpDir = getGlobalAppFlags().tmpDir;
	const fs::path tmpPath = std::strlen(tmpDir) ? fs::path(tmpDir) : fs::temp_directory_path();
	const std::string fileName(makeFileName(index, stage, codeAndEntryAndIncludes, vulkanVer, spirvVer,
											enableValidation, genDisassmebly));
	const std::string pathName((tmpPath / fileName).string());
	const fs::path textPath(pathName + (isGlsl ? ".glsl" : ".spvasm"));
	const fs::path binPath(pathName + ".spvbin");
	const fs::path asmPath(pathName + ".spvasm");
	if (!fs::exists(textPath))
	{
		std::ofstream textFile(textPath.c_str());
		ASSERTMSG(textFile.is_open(), textPath.string());
		textFile << codeAndEntryAndIncludes[ProgramCollection::StageToCode::shaderCode];
	}
	if (getGlobalAppFlags().verbose)
	{
		std::cout << makeCompilerSignature(true);
	}
	if (!buildAlways && fs::exists(binPath))
	{
		uint32_t readed = readFile(binPath, binary); UNREF(readed);
		if (getGlobalAppFlags().verbose)
		{
			const std::string compileCmd = isGlsl
				? makeCompileGlslCommand(stage, codeAndEntryAndIncludes, vulkanVer, spirvVer, enableValidation, textPath, binPath)
				: makeCompileSpvCommand(vulkanVer, spirvVer, textPath, binPath);
			std::cout << "[APP] Compile command: \"" << compileCmd << "\"" << std::endl;
			std::cout << "      File exists: \"" << binPath << "\"" << std::endl;

			if (isGlsl)
			{
				const std::string buildAsmCmd = makeBuildSpvAsm(binPath, asmPath);
				std::cout << "[APP] Build assembly command: \"" << buildAsmCmd << "\"" << std::endl;
				std::cout << "      File exists: \"" << asmPath << "\"" << std::endl;
			}

		}
		ASSERTION(readed != INVALID_UINT32);
		ASSERTION(readed % 4 == 0);
		status = true;
	}
	else
	{
		ASSERTION(verifyIncludes(codeAndEntryAndIncludes));
		const std::string compileCmd = isGlsl
				? makeCompileGlslCommand(stage, codeAndEntryAndIncludes, vulkanVer, spirvVer, enableValidation, textPath, binPath)
				: makeCompileSpvCommand(vulkanVer, spirvVer, textPath, binPath);
		std::string result = captureSystemCommandResult(compileCmd.c_str(), status, '\n');
		bool areErrors = containsErrorString(result);
		bool areWarnings = containsWarningString(result);
		if (getGlobalAppFlags().verbose)
		{
			std::cout << "[APP] Compile command: \"" << compileCmd << "\", "
					  << "Errors: " << (areErrors ? "Yes" : "No") << ", "
					  << "Warnings: " << (areWarnings ? "Yes" : "No") << ", "
					  << "Status: " << std::boolalpha << status << std::noboolalpha
					  << std::endl;
		}
		if (isGlsl)
		{
			bool sinkStatus = true;
			const std::string buildAsmCmd = makeBuildSpvAsm(stage, codeAndEntryAndIncludes, vulkanVer, spirvVer, textPath, asmPath);
			const std::string asmCmdResult = captureSystemCommandResult(buildAsmCmd.c_str(), sinkStatus, '\n');
			if (!sinkStatus)
			{
				errorCollection << asmCmdResult << std::endl;
			}
			if (getGlobalAppFlags().verbose)
			{
				std::cout << "[APP] Build assembly comand: " << buildAsmCmd << ", "
						  << "Warnings: " << (containsWarningString(asmCmdResult) ? "Yes" : "No") << ", "
						  << "Errors: " << (containsErrorString(asmCmdResult) ? "Yes" : "No") << ", "
						  << "Status: " << std::boolalpha << sinkStatus << std::noboolalpha
						  << std::endl;
			}
		}
		if (status)
		{
			if (areErrors || (getGlobalAppFlags().nowerror == false && areWarnings))
			{
				if (enableValidation && fs::exists(binPath))
				{
					std::error_code error_code;
					const fs::path invalidBinFile(pathName + ".invalid.spvbin");
					fs::rename(binPath, invalidBinFile, error_code);
					ASSERTION(error_code.value() == 0);
					if (getGlobalAppFlags().verbose)
					{
						std::cout << "[APP] " << binPath << " has been renamed with " << invalidBinFile << std::endl;
					}
				}
				errorCollection << result << std::endl;
				status = false;
			}
			else if (fs::exists(binPath))
			{
				uint32_t readed = readFile(binPath, binary); UNREF(readed);
				ASSERTION(readed != INVALID_UINT32);
				ASSERTION(readed % 4 == 0);

				if (isGlsl)
				{
					bool sinkStatus = true;
					const std::string buildAsmCmd = makeBuildSpvAsm(binPath, asmPath);
					const std::string asmCmdResult = captureSystemCommandResult(buildAsmCmd.c_str(), sinkStatus, '\n');
					if (getGlobalAppFlags().verbose)
					{
						std::cout << "[APP] Second build assembly command: " << buildAsmCmd << ", "
								  << "Warnings: " << (containsWarningString(asmCmdResult) ? "Yes" : "No") << ", "
								  << "Errors: " << (containsErrorString(asmCmdResult) ? "Yes" : "No") << ", "
								  << "Status: " << std::boolalpha << sinkStatus << std::noboolalpha
								  << std::endl;
					}
					if (!sinkStatus)
					{
						errorCollection << asmCmdResult << std::endl;
					}
				}

				if (enableValidation)
				{
					const std::string validateCmd = makeValidateCommand(vulkanVer, spirvVer, binPath);
					result = captureSystemCommandResult(validateCmd.c_str(), status, '\n');
					areErrors = containsErrorString(result);
					areWarnings = containsWarningString(result);
					if (getGlobalAppFlags().verbose)
					{
						std::cout << "[APP] Validate command: \"" << validateCmd << "\", "
								  << "Warnings: " << (containsWarningString(result) ? "Yes" : "No") << ", "
								  << "Errors: " << (containsErrorString(result) ? "Yes" : "No") << ", "
								  << "Status: " << std::boolalpha << status << std::noboolalpha
								  << std::endl;
					}
					if (areErrors || !status)
					{
						status = false;
						errorCollection << result << std::endl;

						std::error_code error_code;
						const fs::path invalidBinFile(pathName + ".invalid.spvbin");
						fs::rename(binPath, invalidBinFile, error_code);
						ASSERTION(error_code.value() == 0);
						if (getGlobalAppFlags().verbose)
						{
							std::cout << "[APP] " << binPath << " has been renamed with " << invalidBinFile << std::endl;
						}
					}
				}
			}
			else
			{
				status = false;
			}
		}
		else
		{
			if (fs::exists(binPath))
			{
				fs::remove(binPath);
			}
		}
	}

	errorCollection.flush();
	errors = errorCollection.str();

	return status;
}

ProgramCollection::ProgramCollection (ZDevice device, const std::string& basePath)
		: m_device(device), m_basePath(basePath)
{
}

void ProgramCollection::addFromText (VkShaderStageFlagBits type, const std::string& code, const strings& includePaths, const std::string& entryName)
{
	const auto key = std::make_pair(type, m_stageToCount[type]++);
	// [0]: glsl code, [1]: entry name, [2] file name, [3...]: include path(s)
	m_stageToCode[key].push_back(code);
	m_stageToCode[key].push_back(entryName);
	m_stageToCode[key].push_back(std::string());
	for (size_t p = 0; p < includePaths.size(); ++p)
		m_stageToCode[key].push_back(m_basePath + includePaths[p]);
}

bool ProgramCollection::addFromFile(VkShaderStageFlagBits type,
									const std::string& fileName, const strings& includePaths,
									const std::string& entryName, bool verbose)
{
	bool				result			(false);
	const std::string	source_name		((fs::path(m_basePath) / fileName).string());
	std::ifstream		source_handle	(source_name);
	if (source_handle.is_open())
	{
		std::istreambuf_iterator<char> end, begin(source_handle);
		std::string source_content = std::string(begin, end);
		source_handle.close();
		const auto key = std::make_pair(type, m_stageToCount[type]++);
		// [0]: glsl code, [1]: entry name, [2] file name, [3...]: include path(s)
		m_stageToCode[key].push_back(source_content);
		m_stageToCode[key].push_back(entryName);
		m_stageToCode[key].push_back(source_name);
		for (size_t p = 0; p < includePaths.size(); ++p)
			m_stageToCode[key].push_back(m_basePath + includePaths[p]);
		result = true;
	}
	else if (verbose)
	{
		const std::string msg("Unable to open \"" + source_name + "\"");
		ASSERTMSG(false, msg);
	}
	return result;
}

void ProgramCollection::buildAndVerify (const Version& vulkanVer, const Version& spirvVer,
										bool enableValidation, bool genDisassembly, bool buildAlways)
{
	std::string error;
	for (auto stage : availableShaderStages)
	{
		if (mapHasKey(stage, m_stageToCount))
		{
			for (uint32_t k = 0; k < add_const_ref(m_stageToCount).at(stage); ++k)
			{
				const auto key = std::make_pair(stage, k);
				std::vector<unsigned char> binary;
				if (verifyShaderCode(k, stage, vulkanVer, spirvVer,
									 enableValidation, genDisassembly, buildAlways,
									 m_stageToCode[key], binary, error))
					m_stageToBinary[key] = binary;
				else ASSERTMSG(false, error);
			}
		}
	}
}

ZShaderModule ProgramCollection::getShader (VkShaderStageFlagBits stage, uint32_t index, bool verbose) const
{
	ZShaderModule module;
	auto search = m_stageToBinary.find(std::make_pair(stage, index));
	if (m_stageToBinary.end() != search)
	{
		module = createShaderModule(m_device, stage, search->second);
	}
	else if (verbose)
	{
		ASSERTION(false);
	}
	return module;
}

} // namespace vtf
