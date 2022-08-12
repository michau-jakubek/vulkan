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

#include "vtfBacktrace.hpp"
#include "vtfVkUtils.hpp"
#include "vtfZDeletable.hpp"
#include "vtfCUtils.hpp"
#include "vtfProgramCollection.hpp"
#include "vtfFilesystem.hpp"

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

static std::string makeFileName (VkShaderStageFlagBits stage, const strings& code,
								 const uint32_t apiVer, std::optional<uint32_t> spvVer,
								 const bool enableValidation,
								 const char* prefix = nullptr, const char* suffix = nullptr)
{
	size_t hash = 0;
	{
		std::stringstream ss;
		for (const auto& c : code)
		{
			ss << c;
		}
		ss << (enableValidation ? 777 : 392);
		Version v(apiVer);
		ss << v.nmajor ;
		ss << v.nminor;
		if (spvVer.has_value())
		{
			v.update(*spvVer);
			ss << v.nmajor;
			ss << v.nminor;
		}
		ss.flush();
		hash = std::hash<std::string>()(ss.str());
	}
	std::stringstream ss;
	if (prefix) ss << prefix;
	ss << hash;
	ss << "." << shaderStageToCommand(stage);
	if (suffix) ss << suffix;
	return ss.str();
}

static bool verifyIncludes (const strings& codeAndEntryAndIncludes)
{
	bool exists = true;
	for (size_t i = 2; exists && i < codeAndEntryAndIncludes.size(); ++i)
	{
		const fs::path path(codeAndEntryAndIncludes[i]);
		exists = fs::exists(path);
	}
	return exists;
}

static bool containsErrorString(const std::string& text)
{
	bool result = false;
	if (text.length())
	{
		result = std::strstr(text.c_str(), "error:") != nullptr;
	}
	return result;
}

static auto makeCompileGlslCommand (VkShaderStageFlagBits stage,
									const strings& codeAndEntryAndIncludes,
									uint32_t apiVer, std::optional<uint32_t> spirvVer,
									const fs::path& input, const fs::path& output) -> std::string
{
	Version vulkanVer(apiVer);
	Version spvVer(spirvVer.has_value() ? *spirvVer : 0);
	const char* space = " ";

	const std::string entryName = codeAndEntryAndIncludes[1];
	UNREF(entryName);

	std::stringstream cmd;
	cmd << "glslc -c ";
	for (size_t i = 2; i < codeAndEntryAndIncludes.size(); ++i)
	{
		cmd << "-I " << codeAndEntryAndIncludes[i] << space;
	}
	cmd	<< "--target-env=vulkan" << vulkanVer.nmajor << "." << vulkanVer.nminor << space;
	if (spirvVer.has_value())
	{
		cmd << "--target-spv=spv" << spvVer.nmajor << "." << spvVer.nminor << space;
	}
	cmd << "-fshader-stage=" << shaderStageToCommand(stage) << space
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

static auto makeCompileSpvCommand (uint32_t apiVer, std::optional<uint32_t> spirvVer,
							const fs::path& input, const fs::path& output) -> std::string
{
	Version vulkanVer(apiVer);
	Version spvVer(spirvVer.has_value() ? *spirvVer : 0);
	const char* space = " ";

	std::stringstream cmd;
	cmd << "spirv-as ";
	if (spirvVer.has_value())
		cmd << "--target-env spv" << spvVer.nmajor << "." << spvVer.nminor << space;
	else cmd << "--target-env vulkan" << vulkanVer.nmajor << "." << vulkanVer.nminor << space;
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
							 uint32_t apiVer, std::optional<uint32_t> spirvVer,
							 const fs::path& input, const fs::path& output) -> std::string
{
	// glslangValidator -S vert -H --target-env spirv1.4 14580917305851103956.vert.glsl >>
	Version vulkanVer(apiVer);
	Version spvVer(spirvVer.has_value() ? *spirvVer : 0);
	const char* space = " ";

	std::stringstream cmd;
	cmd << "glslangValidator -H ";
	cmd << "-S " << shaderStageToCommand(stage) << space;
	if (spirvVer.has_value())
		cmd << "--target-env spirv" << spvVer.nmajor << "." << spvVer.nminor << space;
	else cmd << "--target-env vulkan" << vulkanVer.nmajor << "." << vulkanVer.nminor << space;
	cmd << input << space << ">> " << output;
	return cmd.str();
}

static auto makeValidateCommand (uint32_t apiVer, std::optional<uint32_t> spirvVer, const fs::path& output) -> std::string
{
	Version vulkanVer(apiVer);
	Version spvVer(spirvVer.has_value() ? *spirvVer : 0);
	const char* space = " ";

	std::stringstream cmd;
	cmd << "spirv-val ";
	if (spirvVer.has_value())
		cmd << "--target-env spv" << spvVer.nmajor << "." << spvVer.nminor;
	else cmd << "--target-env vulkan" << vulkanVer.nmajor << "." << vulkanVer.nminor;
	cmd << space << output;
	return cmd.str();
}

static	bool verifyShaderCode (VkShaderStageFlagBits stage,
							   uint32_t apiVer, std::optional<uint32_t> spirvVer, bool enableValidation,
							   const strings& codeAndEntryAndIncludes, std::vector<unsigned char>& binary, std::string& error)
{
	const bool isGlsl = codeAndEntryAndIncludes[0].find("#version") != std::string::npos;

	bool status = false;
	const std::string pathName(makeFileName(stage, codeAndEntryAndIncludes, apiVer, spirvVer, enableValidation,
											(fs::temp_directory_path() / "").generic_u8string().c_str()));
	const fs::path textPath(pathName + (isGlsl ? ".glsl" : "spvasm"));
	const fs::path binPath(pathName + ".spvbin");
	if (!fs::exists(textPath))
	{
		std::ofstream textFile(textPath.c_str());
		ASSERTION(textFile.is_open());
		textFile << codeAndEntryAndIncludes[0];
	}
	if (fs::exists(binPath))
	{
		uint32_t readed = readFile(binPath, binary); UNREF(readed);
		ASSERTION(readed != INVALID_UINT32);
		ASSERTION(readed % 4 == 0);
		status = true;
	}
	else
	{
		ASSERTION(verifyIncludes(codeAndEntryAndIncludes));
		const std::string compileCmd = isGlsl
				? makeCompileGlslCommand(stage, codeAndEntryAndIncludes, apiVer, spirvVer, textPath, binPath)
				: makeCompileSpvCommand(apiVer, spirvVer, textPath, binPath);
		if (getAppVerboseFlag())
		{
			std::cout << "[APP] Compile command: \"" << compileCmd << "\"" << std::endl;
		}
		if (isGlsl)
		{
			const fs::path asmPath(pathName + ".spvasm");
			const std::string buildAsmCmd = makeBuildSpvAsm(stage, apiVer, spirvVer, textPath, asmPath);
			captureSystemCommandResult(buildAsmCmd.c_str(), status, '\n');
			if (getAppVerboseFlag())
			{
				std::cout << "[APP] Build assembly command: \"" << buildAsmCmd << "\"" << std::endl;
			}
		}
		std::string result = captureSystemCommandResult(compileCmd.c_str(), status, '\n');
		if (status)
		{
			if (containsErrorString(result))
			{
				//error = cmd;
				//error += "\n";
				//error += result;
				error = result;
				status = false;
			}
			else if (fs::exists(binPath))
			{
				uint32_t readed = readFile(binPath, binary); UNREF(readed);
				ASSERTION(readed != INVALID_UINT32);
				ASSERTION(readed % 4 == 0);

				if (enableValidation)
				{
					const std::string validateCmd = makeValidateCommand(apiVer, spirvVer, binPath);
					result = captureSystemCommandResult(validateCmd.c_str(), status, '\n');
					if (getAppVerboseFlag())
					{
						std::cout << "[APP] Validate command: \"" << validateCmd << "\"" << std::endl;
					}
					if (result.length())
					{
						fs::remove(binPath);
						error = "In stage: " + std::string(shaderStageToCommand(stage)) + "\n" + result;
						status = false;
					}
					else status = true;
				}
			}
			else
			{
				//error = cmd;
				//error += "\n";
				//error += result;
				error = result;
				status = false;
			}
		}
		else
		{
			fs::remove(binPath);
			error = result;
		}
	}
	return status;
}

ProgramCollection::ProgramCollection (VulkanContext& vc, const std::string& basePath)
		: m_context(vc), m_basePath(basePath)
{
}

void ProgramCollection::add (VkShaderStageFlagBits type, const std::string& code)
{
	m_stageToCode[type].clear();
	m_stageToCode[type].push_back(code);
}

void ProgramCollection::add (VkShaderStageFlagBits type, const std::vector<unsigned char>& code)
{
	m_stageToBinary[type] = code;
}

bool ProgramCollection::addFromFile(VkShaderStageFlagBits type,
									const std::string& fileName, const strings& includePaths,
									const std::string& entryName, bool verbose)
{
	bool result = false;
	const std::string glsl_name(m_basePath + fileName);
	std::ifstream glsl_handle(glsl_name);
	if (glsl_handle.is_open())
	{
		std::string glsl_content = std::string((std::istreambuf_iterator<char>(glsl_handle)), std::istreambuf_iterator<char>());
		m_stageToCode[type].push_back(glsl_content);
		m_stageToCode[type].push_back(entryName);
		for (size_t p = 0; p < includePaths.size(); ++p)
			m_stageToCode[type].push_back(m_basePath + includePaths[p]);
		result = true;
	}
	else if (verbose)
	{
		const std::string msg("Unable to open \"" + glsl_name + "\"");
		ASSERTMSG(false, msg);
	}
	return result;
}

void ProgramCollection::buildAndVerify (bool enableValidation, std::optional<uint32_t> spirvVer)
{
	std::string error;
	const uint32_t apiVer = m_context.instance.getParam<uint32_t>();
	for (auto stage : availableShaderStages)
	{
		if (mapHasKey(stage, m_stageToCode))
		{
			std::vector<unsigned char> binary;
			if (verifyShaderCode(stage, apiVer, spirvVer, enableValidation, m_stageToCode[stage], binary, error))
				m_stageToBinary[stage] = binary;
			else ASSERTMSG(false, error);
		}
	}
}

std::optional<ZShaderModule> ProgramCollection::getShader (VkShaderStageFlagBits stage, bool verbose) const
{
	std::optional<ZShaderModule> module;
	auto search = m_stageToBinary.find(stage);
	if (m_stageToBinary.end() != search)
	{
		module = createShaderModule(m_context.device, search->second);
	}
	else if (verbose)
	{
		ASSERTION(false);
	}
	return module;
}

} // namespace vtf
