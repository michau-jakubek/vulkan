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
#include "vtfStructUtils.hpp"

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

struct VerifyShaderCodeInfo
{
	const std::string			code_;
	const VkShaderStageFlagBits	stage_;
	VerifyShaderCodeInfo(const std::string& code, VkShaderStageFlagBits stage)
		: code_(code), stage_(stage) {}
};
bool verifyShaderCodeGL(const VerifyShaderCodeInfo* infos, uint32_t infoCount, std::string& error, int majorVersion = 4, int minorVersion = 5, void* glWindow = nullptr);
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
	VK_SHADER_STAGE_GEOMETRY_BIT,
	VK_SHADER_STAGE_TASK_BIT_EXT,
	VK_SHADER_STAGE_MESH_BIT_EXT,
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
	case VK_SHADER_STAGE_TASK_BIT_EXT:					return "task";
	case VK_SHADER_STAGE_MESH_BIT_EXT:					return "mesh";
	default: break;
	}
	ASSERTFALSE(""/*-Wgnu-zero-variadic-macro-arguments*/);
	return 0;
}

static std::string makeFileName (uint32_t shaderIndex, VkShaderStageFlagBits stage,
								 const strings& codeAndEntryAndIncludes,
								 const Version& vulkanVer, const Version& spirvVer,
								 const bool enableValidation, const bool genDisassembly, const bool buildAlways,
								 const char* prefix = nullptr, const char* suffix = nullptr)
{
	const std::string entryName = codeAndEntryAndIncludes.at(ProgramCollection::StageToCode::entryName);
	const std::string fileName = codeAndEntryAndIncludes.at(ProgramCollection::StageToCode::fileName).empty()
			? shaderStageToCommand(stage)
			: fs::path(codeAndEntryAndIncludes.at(ProgramCollection::StageToCode::fileName)).filename().string();

	size_t hash = 0;
	{
		std::ostringstream ss;
		ss << shaderIndex;
		UNREF(entryName);
		//ss << entryName;
		ss << (codeAndEntryAndIncludes.at(ProgramCollection::StageToCode::fileName).empty()
			   ? codeAndEntryAndIncludes.at(ProgramCollection::StageToCode::shaderCode)
			   : codeAndEntryAndIncludes.at(ProgramCollection::StageToCode::fileName));
		if (buildAlways && !codeAndEntryAndIncludes.at(ProgramCollection::StageToCode::fileName).empty())
		{
			ss << codeAndEntryAndIncludes.at(ProgramCollection::StageToCode::shaderCode);
		}
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

static bool containsErrorString (add_cref<std::string> text, bool ignoreEntryPointMain = true)
{
	std::istringstream iss(text);
	const std::regex rError("(error:.*)", std::regex_constants::icase);
	const std::regex rEntryPoint("must.*main", std::regex_constants::icase);

	std::string line;
	int matches = 0;

	while (std::getline(iss, line))
	{
		std::smatch sm;
		if (std::regex_search(line, sm, rError))
		{
			if (ignoreEntryPointMain && std::regex_search(sm.str(), rEntryPoint))
			{
				continue;
			}
			matches = matches + 1;
		}
	}
	return (0 < matches);
}

static bool containsWarningString (const std::string& text)
{
	return std::regex_search(text, std::regex("warning:", std::regex_constants::icase));
}

static std::string makeCompilerListFromPATH (const char* compiler)
{
#if SYSTEM_OS_WINDOWS == 1
	const char pathSep = ';';
	const std::string exeExt(".exe");
#else
	const char pathSep = ':';
	const std::string exeExt;
#endif
	bool any = false;
	std::error_code ec;
	std::ostringstream compilers;
	const std::string compilerWithExt = compiler + exeExt;
#if defined(_MSC_VER)
 #pragma warning(disable : 4996)
#endif
	add_cptr<char> pathVar = std::getenv("PATH");
#if defined(_MSC_VER)
#pragma warning(default : 4996)
#endif
	const strings paths = splitString(pathVar ? std::string(pathVar) : std::string(), pathSep);

	for (add_cref<std::string> path : paths)
	{
		const fs::path compilerPath(fs::absolute(fs::path(path), ec) / compilerWithExt);
		if (fs::exists(compilerPath))
		{
			any = true;
			compilers << compilerPath.string() << '\n';
		}
	}

	if (any)
		return compilers.str();

	return compilerWithExt;
}

static std::vector<std::pair<std::string, std::string>> makeCompilerSignature (bool glslangValidator)
{
	bool status = false;
	// As far I know glslangValidator is an alias to glslang utility
	const char* exe = glslangValidator ? "glslangValidator" : "glslang";
	auto getCompilerVersion = [&](add_cref<std::string> comp) -> std::string
	{
		const std::string verCmd = '\"' + comp + "\" --version";
		return captureSystemCommandResult(verCmd.c_str(), status, '\n');
	};
	std::string compiler;
	std::vector<std::pair<std::string, std::string>> compilers;
	/*
	* On WSL Ubuntu -aP switches to the type command cause an error, try searching the PATH
	const std::string locCmd(
#if SYSTEM_OS_LINUX == 1
		std::string("type -aP ") + exe
#else
		std::string("where ") + exe
#endif
	);
	std::istringstream str(captureSystemCommandResult(locCmd.c_str(), status, '\n'));
	*/
	std::istringstream str(makeCompilerListFromPATH(exe));
	while (std::getline(str, compiler))
	{
		compilers.emplace_back(compiler, getCompilerVersion(compiler));
	}
	return compilers;
}

static std::string getCompilerExecutable ()
{
	const auto compilers = makeCompilerSignature(true);
	ASSERTMSG(data_count(compilers) > 0u,
		"[ERROR] No GLSL compiler found. Try update environment PATH variable");
	add_cref<GlobalAppFlags> gf = getGlobalAppFlags();
	uint32_t compilerIndex = gf.compilerIndex;
	if (compilerIndex >= data_count(compilers))
	{
		compilerIndex = 0u;
		if (gf.verbose > 0)
		{
			std::cout << "[APP WARNING] Compiler index exceeds compilers list, index 0 will be used\n";
		}
	}
	return compilers.at(compilerIndex).first;
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
static std::string maybe_quoted (add_cref<std::string> s) { return s; }

static auto makeCompileGlslCommand (VkShaderStageFlagBits stage,
									const strings& codeAndEntryAndIncludes,
									const Version& vulkanVer, const Version& spirvVer,
									const bool enableValidation, const std::string& compilerExecutable,
									const fs::path& input, const fs::path& output) -> std::string
{
	const char* space = " ";

	const std::string entryName = codeAndEntryAndIncludes.at(ProgramCollection::StageToCode::entryName);

	std::stringstream cmd;
	cmd << maybe_quoted(compilerExecutable) << space;
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
		<< "-e " << entryName << space
		<< "--source-entrypoint " << entryName << space
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
								   const std::string& compilerExecutable,
								   const fs::path& input, const fs::path& output) -> std::string
{
	const char* space = " ";
	std::stringstream cmd;

	cmd << maybe_quoted((fs::path(compilerExecutable).parent_path() / "spirv-as").string()) << space;
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

static auto makeBuildSpvDis (const std::string& compilerExecutable, const fs::path& input, const fs::path& output) -> std::string
{
	std::stringstream cmd;
	cmd << maybe_quoted((fs::path(compilerExecutable).parent_path() / "spirv-dis").string());
	cmd << " --comment --no-color " << input << " -o " << output << " "
#if SYSTEM_OS_LINUX
	<< "2>&1 && ls "
#else
	<< "2>&1 && dir "
#endif
	<< output;
	cmd.flush();
	return cmd.str();
}

static auto makeValidateCommand (const Version& vulkanVer, const Version& spirvVer, const fs::path& output) -> std::string
{
	const char* space = " ";
	std::stringstream cmd;

	cmd << maybe_quoted(((fs::path(getCompilerExecutable()).parent_path() / "spirv-val").string())) << space;
	cmd << "--target-env vulkan" << vulkanVer.nmajor << "." << vulkanVer.nminor << space;
	cmd << "--target-env spv" << spirvVer.nmajor << "." << spirvVer.nminor << space;
	cmd << output << space;
	cmd << "2>&1";
	cmd.flush();
	return cmd.str();
}

bool verifyShaderCode (uint32_t shaderIndex, VkShaderStageFlagBits stage,
					   add_cref<Version> vulkanVer, add_cref<Version> spirvVer,
					   add_cref<strings> codeAndEntryAndIncludes,
					   add_ref<std::string> shaderFileName,
                       add_ref<std::vector<char>> binary,
                       add_ref<std::vector<char>> assembly,
      [[maybe_unused]] add_ref<std::vector<char>> disassembly,
					   add_ref<std::string> errors,
					   bool enableValidation,
					   bool genDisassmebly,
					   bool buildAlways,
					   bool genBinary)
{
	add_cref<std::string> shaderCode = codeAndEntryAndIncludes[ProgramCollection::StageToCode::shaderCode];
	add_cref<std::string> codeFileName = codeAndEntryAndIncludes[ProgramCollection::StageToCode::fileName];
	add_cref<std::string> shaderHdr = codeAndEntryAndIncludes[ProgramCollection::StageToCode::header];
	const bool isGlsl = shaderHdr.find("#version") != std::string::npos;

	bool status = false;
	std::stringstream errorCollection;
	add_cref<GlobalAppFlags> gf(getGlobalAppFlags());
	const char* tmpDir = gf.tmpDir;
	const fs::path tmpPath = std::strlen(tmpDir) ? fs::path(tmpDir) : fs::temp_directory_path();
	shaderFileName = makeFileName(shaderIndex, stage, codeAndEntryAndIncludes, vulkanVer, spirvVer,
								  enableValidation, genDisassmebly, buildAlways);
	const std::string pathName((tmpPath / shaderFileName).string());
	shaderFileName += (isGlsl ? ".glsl" : ".spvasm");
	const fs::path textPath(pathName + (isGlsl ? ".glsl" : ".spvasm"));
	const fs::path binPath(pathName + ".spvbin");
	const fs::path asmPath(pathName + ".spvasm");
	const std::string compilerExecutable = getCompilerExecutable();
	if (buildAlways || !fs::exists(textPath))
	{
		if (shaderCode.empty())
		{
			fs::copy_file(fs::path(codeFileName), textPath, fs::copy_options::overwrite_existing);
		}
		else
		{
			std::ofstream textFile(textPath.c_str());
			ASSERTMSG(textFile.is_open(), textPath.string());
			textFile << shaderCode;
		}
	}
	if (gf.verbose)
	{
		std::string versionLine;
		auto compilers = makeCompilerSignature(true);
		std::cout << "[APP] Found " << data_count(compilers) << " compiler(s):\n";
		for (uint32_t cv = 0u; cv < data_count(compilers); ++cv)
		{
			add_ref<std::pair<std::string, std::string>> item = compilers.at(cv);
			std::cout << "  " << cv << ": " << std::quoted(item.first) << '\n';
			std::cout << "  Version:\n";
			std::istringstream version(std::move(item.second));
			while (std::getline(version, versionLine))
			{
				std::cout << "    " << versionLine << '\n';
			}
		}
	}
	auto assemblyShaderCode = [&]() -> void
	{
		assembly.resize(shaderCode.length());
		std::transform(shaderCode.begin(), shaderCode.end(),
			assembly.begin(), [](const char c) { return uint8_t(c); });
	};
	if (!buildAlways && (!genBinary || fs::exists(binPath)))
	{
		if (readFile(asmPath, assembly) == INVALID_UINT32)
		{
			assemblyShaderCode();
		}
		const uint32_t readBin = readFile(binPath, binary);
		if (gf.verbose)
		{
			const std::string compileCmd = isGlsl
				? makeCompileGlslCommand(stage, codeAndEntryAndIncludes, vulkanVer, spirvVer, enableValidation,
										compilerExecutable, textPath, binPath)
				: makeCompileSpvCommand(vulkanVer, spirvVer, compilerExecutable, textPath, binPath);
			std::cout << "[APP] Compile command: \"" << compileCmd << "\"" << std::endl;
			std::cout << "      File: \"" << binPath << "\" exists: "
										<< boolean(fs::exists(binPath)) << std::endl;

			if (genDisassmebly)
			{
				const std::string buildDisasmCmd = makeBuildSpvDis(compilerExecutable, binPath, asmPath);
				std::cout << "[APP] Build assembly command: \"" << buildDisasmCmd << "\"" << std::endl;
				std::cout << "      File \"" << asmPath << "\" exists: "
											<< boolean(fs::exists(asmPath)) << std::endl;
			}
		}
		ASSERTION(readBin != INVALID_UINT32);
		ASSERTION(readBin % 4 == 0);
		status = true;
	}
	else if (!genBinary && !enableValidation)
	{
		ASSERTMSG(!isGlsl, "Cannot disable binaries generation for GLSL shaders");
		assemblyShaderCode();
	}
	else
	{
		ASSERTION(verifyIncludes(codeAndEntryAndIncludes));
		const std::string compileCmd = isGlsl
				? makeCompileGlslCommand(stage, codeAndEntryAndIncludes, vulkanVer, spirvVer, enableValidation,
										compilerExecutable, textPath, binPath)
				: makeCompileSpvCommand(vulkanVer, spirvVer, compilerExecutable, textPath, binPath);
		std::string result = captureSystemCommandResult(compileCmd.c_str(), status, '\n');
		bool areErrors = containsErrorString(result);
		bool areWarnings = containsWarningString(result);
		const bool binFileExists = fs::exists(binPath);
		if (gf.verbose)
		{
			std::cout << "[APP] Compile command: \"" << compileCmd << "\"\n"
					  << "      Command result: " << std::quoted(result) << std::endl
					  << "      File \"" << binPath << "\" created: " << boolean(binFileExists, true) << std::endl
					  << "      Errors: " << boolean(areErrors, true) << ", "
					  << "      Warnings: " << boolean(areWarnings, true) << ", "
					  << "      Status: " << boolean(status)
					  << std::endl;
			std::cout << "[SYS] Command result: " << std::quoted(result) << std::endl;
		}
		if (status &= (!areErrors && (!areWarnings || gf.nowerror) && binFileExists); status)
		{
			const uint32_t readBin = readFile(binPath, binary);
			ASSERTION(readBin != INVALID_UINT32);
			ASSERTION(readBin % 4 == 0);

			if (enableValidation)
			{
				const std::string validateCmd = makeValidateCommand(vulkanVer, spirvVer, binPath);
				result = captureSystemCommandResult(validateCmd.c_str(), status, '\n');
				areErrors = containsErrorString(result);
				areWarnings = containsWarningString(result);
				if (gf.verbose)
				{
					std::cout << "[APP] Validate command: \"" << validateCmd << "\"\n"
							  << "      Command result: " << std::quoted(result) << std::endl
							  << "      Warnings: " << boolean(areWarnings, true) << ", "
							  << "      Errors: " << boolean(areErrors, true) << ", "
							  << "      Status: " << boolean(status)
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
					if (gf.verbose)
					{
						std::cout << "[APP] " << binPath << " has been renamed with " << invalidBinFile << std::endl;
					}
				}
			} // fi (enableValidation)

			if (status && genDisassmebly)
			{
				bool disassmStatus = true;
				const std::string buildDisasmCmd = makeBuildSpvDis(compilerExecutable, binPath, asmPath);
				result  = captureSystemCommandResult(buildDisasmCmd.c_str(), disassmStatus, '\n');
				areErrors = containsErrorString(result);
				areWarnings = containsWarningString(result);
				const bool asmFileExists = fs::exists(asmPath);
				if (gf.verbose)
				{
					std::cout << "[APP] Build assembly comand: \"" << buildDisasmCmd << "\"\n"
							  << "      Command result: \"" << result << "\"\n"
							  << "      Warnings: " << boolean(areWarnings, true) << ", "
							  << "      Errors: " << boolean(areErrors, true) << ", "
							  << "      Status: " << boolean(disassmStatus)
							  << "      File \"" << asmPath << "\" created: " << boolean(asmFileExists, true)
							  << std::endl;
				}
				if (disassmStatus && asmFileExists)
				{
					const uint32_t readAsm = readFile(asmPath, assembly);
					ASSERTION(readAsm != INVALID_UINT32);
				}
			}
			else if (!isGlsl)
			{
				assemblyShaderCode();
			}
		}
		else
		{
			std::error_code ec;
			fs::remove(textPath, ec);
			errorCollection << result << std::endl;
		}
	}

	errorCollection.flush();
	errors = errorCollection.str();

	return status;
}

_GlSpvProgramCollection::_GlSpvProgramCollection (ZDevice device, add_cref<std::string> basePath)
	: m_device			(device)
	, m_basePath		(basePath)
	, m_tempDir			()
	, m_stageToCount	()
	, m_stageToCode		()
	, m_stageToFileName	()
	, m_stageToAssembly	()
	, m_stageToBinary	()
{
}

void _GlSpvProgramCollection::addFromText (VkShaderStageFlagBits type, add_cref<std::string> code,
										   add_cref<strings> includePaths, add_cref<std::string> entryName)
{
	std::string header;
	std::copy_n(code.begin(), 20, std::back_inserter(header));
	const auto key = std::make_pair(type, m_stageToCount[type]++);
	// [0]: glsl code, [1]: header, [2]: entry name, [3] file name, [4...]: include path(s)
	m_stageToCode[key].push_back(code);
	m_stageToCode[key].push_back(header);
	m_stageToCode[key].push_back(entryName);
	m_stageToCode[key].push_back(std::string());
	for (size_t p = 0; p < includePaths.size(); ++p)
		m_stageToCode[key].push_back(m_basePath + includePaths[p]);
}

bool _GlSpvProgramCollection::addFromFile (VkShaderStageFlagBits type,
										   add_cref<std::string> fileName, add_cref<strings> includePaths,
										   add_cref<std::string> entryName, bool verbose)
{
	bool				result			(false);
	const fs::path		basePath		(m_basePath);
	const fs::path		sourcePath		= basePath / fileName;
	const std::string	source_name		(sourcePath.string());
	std::ifstream		source_handle	(source_name);
	if (source_handle.is_open())
	{
		std::istreambuf_iterator<char> begin(source_handle);
		//std::string source_content = std::string(begin, end);
		std::string header;
		std::copy_n(begin, 20, std::back_inserter(header));
		source_handle.close();
		const auto key = std::make_pair(type, m_stageToCount[type]++);
		// [0]: glsl code, [1]: header, [2]: entry name, [3] file name, [4...]: include path(s)
		m_stageToCode[key].push_back(std::string());
		m_stageToCode[key].push_back(header);
		m_stageToCode[key].push_back(entryName);
		m_stageToCode[key].push_back(source_name);
		for (size_t p = 0; p < includePaths.size(); ++p)
			m_stageToCode[key].push_back((basePath / includePaths[p]).string());
		result = true;
	}
	else if (verbose)
	{
		ASSERTFALSE("Unable to open \"", source_name, "\"");
	}
	return result;
}

auto _GlSpvProgramCollection::getShaderCode (VkShaderStageFlagBits stage,
											 uint32_t index,
                                             bool binORasm) const -> add_cref<std::vector<char>>
{
	if (binORasm)
		return m_stageToBinary.at({ stage, index });
	return m_stageToAssembly.at({ stage, index });
}

add_cref<std::string> _GlSpvProgramCollection::getShaderEntry (VkShaderStageFlagBits stage, uint32_t index) const
{
	return m_stageToCode.at({ stage, index }).at(entryName);
}

add_cref<std::string> _GlSpvProgramCollection::getShaderFile (VkShaderStageFlagBits stage, uint32_t index, bool inORout) const
{
	if (inORout)
		return m_stageToCode.at({ stage, index }).at(fileName);
	return m_stageToFileName.at({ stage, index });
}

add_ptr<_GlSpvProgramCollection::ShaderLink> _GlSpvProgramCollection::ShaderLink::head ()
{
	add_ptr<ShaderLink> link = this;
	while (link->prev) link = link->prev;
	return link;
}

uint32_t _GlSpvProgramCollection::ShaderLink::count () const
{
	uint32_t n = 1u;
	add_cptr<ShaderLink> link = this;
	while (link->next)
	{
		link = link->next;
		n = n + 1u;
	}
	return n;
}

ProgramCollection::ProgramCollection (ZDevice device, add_cref<std::string> basePath)
	: _GlSpvProgramCollection(device, basePath)
{
}

void ProgramCollection::buildAndVerify (add_cref<Version> vulkanVer, add_cref<Version> spirvVer,
										bool enableValidation, bool genDisassembly, bool buildAlways)
{
	std::string errors;
	const bool genBinary = true;
	for (auto stage : availableShaderStages)
	{
		if (mapHasKey(stage, m_stageToCount))
		{
			for (uint32_t k = 0; k < add_const_ref(m_stageToCount).at(stage); ++k)
			{
				const auto key = std::make_pair(stage, k);
				std::string shaderFileName;
                std::vector<char> binary, assembly, disassembly;
				if (verifyShaderCode(k, stage, vulkanVer, spirvVer,
									 m_stageToCode[key], shaderFileName, binary, assembly, disassembly,
									 errors, enableValidation, genDisassembly, buildAlways, genBinary))
				{
					m_stageToDisassembly[key]	= std::move(disassembly);
					m_stageToFileName[key]		= std::move(shaderFileName);
					m_stageToAssembly[key]		= std::move(assembly);
					m_stageToBinary[key]		= std::move(binary);
				}
				else
				{
					std::ostringstream codeWidthLines;
					add_cref<std::string> code = m_stageToCode[key][ProgramCollection::StageToCode::shaderCode];
					if (code.empty() == false)
					{
						uint32_t num = 0u;
						std::string line;
						std::istringstream is(code);
						while (std::getline(is, line) && ++num)
						{
							codeWidthLines << num << ": " << std::move(line) << std::endl;
						}
					}
					ASSERTFALSE(errors, codeWidthLines.str());
				}
			}
		}
	}
}

void ProgramCollection::buildAndVerify (bool buildAlways)
{
	add_cref<GlobalAppFlags>	gf = getGlobalAppFlags();
	buildAndVerify(gf.vulkanVer, gf.spirvVer, gf.spirvValidate, gf.genSpirvDisassembly, buildAlways);
}

ZShaderModule ProgramCollection::getShader (VkShaderStageFlagBits stage, uint32_t index, bool verbose) const
{
	ZShaderModule module;
	const auto stageAndIndex = StageAndIndex(stage, index);
	auto search = m_stageToBinary.find(stageAndIndex);
	if (m_stageToBinary.end() != search)
	{
		add_cref<std::vector<char>> code = search->second;
		const auto size = code.size();
		ASSERTMSG(size % 4u == 0u, "Shader raw code size (", size, ") must be aligned to four bytes");
		module = createShaderModule(m_device, stage,
			reinterpret_cast<add_cptr<uint32_t>>(code.data()), size_t(size), m_stageToCode.at(stageAndIndex).at(entryName));
	}
	else if (verbose)
	{
		ASSERTFALSE(""/*-Wgnu-zero-variadic-macro-arguments*/);
	}
	return module;
}

VkShaderStageFlagBits shaderGetStage (ZShaderModule module)
{
	return module.getParam<VkShaderStageFlagBits>();
}

} // namespace vtf
