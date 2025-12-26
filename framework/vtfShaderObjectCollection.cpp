#include "vtfShaderObjectCollection.hpp"
#include "vtfStructUtils.hpp"
#include "vtfZUtils.hpp"
#include "vtfProgressRecorder.hpp"
#include "vtfBacktrace.hpp"

namespace vtf
{

ShaderObjectCollection::ShaderObjectCollection (ZDevice device, add_cref<std::string> basePath, uint32_t maxShaders)
	: _GlSpvProgramCollection	(device, basePath)
	, m_maxShaders				(maxShaders)
	, m_linkCount				(0u)
	, m_links					()
{
	m_links.reserve(maxShaders);
}

struct ShaderBuildOptions
{
	bool enableValidation	= false;
	bool buildAlways		= false;
	bool genAssembly		= false;
	Version vulkanVer		= Version(0u, 0u);
	Version spirvVer		= Version(0u, 0u);
};

struct ShaderObjectCollection::ShaderData
{
	ZShaderObject		shader;
	ShaderBuildOptions	options;
	ZPushConstants		pushConstants;
	virtual ~ShaderData () = default;
			ShaderData () = default;
};

ShaderObjectCollection::IntShaderLink::~IntShaderLink ()
{
	if (autoDelete)	delete data;
	data = nullptr;
}
ShaderObjectCollection::IntShaderLink::IntShaderLink (add_ref<ShaderObjectCollection> coll,
													  VkShaderStageFlagBits stage,
													  VkShaderStageFlagBits nextStage, uint32_t index)
	: ShaderLink	()
	, collection	(coll)
	, data			(new ShaderData)
	, autoDelete	(true)
{
	ASSERTION(data);
	this->stage = stage;
	this->index = index;
	const VkShaderStageFlagBits validStage =
		(VK_SHADER_STAGE_FRAGMENT_BIT == stage || VK_SHADER_STAGE_COMPUTE_BIT == stage) ? VkShaderStageFlagBits(0) : nextStage;
	data->shader.setParam<ZDistType<SomeZero, VkShaderStageFlagBits>>(stage);
	data->shader.setParam<ZDistType<SomeOne, VkShaderStageFlagBits>>(validStage);
}
ShaderObjectCollection::IntShaderLink::IntShaderLink (IntShaderLink&& other) noexcept
	: ShaderLink	(std::forward<ShaderLink>(other))
	, collection	(other.collection)
	, data			(other.data)
	, autoDelete	(other.autoDelete)
{
	other.data = nullptr;
}
ShaderObjectCollection::IntShaderLink::IntShaderLink (std::nullptr_t, add_cref<IntShaderLink> other) noexcept
	: ShaderLink	(other)
	, collection	(other.collection)
	, data			(other.data)
	, autoDelete	(false)
{
}
auto ShaderObjectCollection::find (VkShaderStageFlagBits type, uint32_t shaderIndex,
								   add_ref<std::vector<IntShaderLink>> linkList) -> add_ptr<ShaderLink>
{
	for (add_ref<ShaderLink> link : linkList)
	{
		if (link.stage == type && link.index == shaderIndex)
			return &link;
	}
	return nullptr;
}

auto ShaderObjectCollection::add (VkShaderStageFlagBits stage, VkShaderStageFlagBits nextStage,
									uint32_t shaderIndex, add_cref<ShaderLink> parent) -> ShaderLink
{
	m_links.emplace_back(*this, stage, nextStage, shaderIndex);
	add_ptr<ShaderLink> pNewLink = &m_links.back();
	if (add_ptr<ShaderLink> pPrevLink = find(parent.stage, parent.index, m_links); pPrevLink)
	{
		static_cast<add_ptr<IntShaderLink>>(pPrevLink) // force reset nextStage
			->data->shader.setParam<ZDistType<SomeOne, VkShaderStageFlagBits>>(stage);
		pPrevLink->next = pNewLink;
		pNewLink->prev = pPrevLink;
	}
	return *pNewLink;
}

auto ShaderObjectCollection::addFromText (VkShaderStageFlagBits type,
										  add_cref<std::string> code, VkShaderStageFlagBits nextStage,
										  add_cref<strings> includePaths, add_cref<std::string> entryName) -> ShaderLink
{
	ASSERTMSG((m_links.size() + 1u <= m_maxShaders), "Amount of shaders exceeds maxShaderCount");
	_addFromText(type, code, includePaths, entryName);
	const uint32_t	shaderIndex = m_stageToCount.at(type) - 1u;
	return add(type, nextStage, shaderIndex, {});
}

auto ShaderObjectCollection::addFromText (VkShaderStageFlagBits type,
										  add_cref<std::string> code, add_cref<ShaderLink> parent,
										  add_cref<strings> includePaths, add_cref<std::string> entryName) -> ShaderLink
{
	ASSERTMSG((m_links.size() + 1u <= m_maxShaders), "Amount of shaders exceeds maxShaderCount");
	_addFromText(type, code, includePaths, entryName);
	const uint32_t	shaderIndex = m_stageToCount.at(type) - 1u;
	return add(type, VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM, shaderIndex, parent);
}

auto ShaderObjectCollection::addFromFile (VkShaderStageFlagBits type, add_cref<std::string> fileName, VkShaderStageFlagBits nextStage,
										  add_cref<strings> includePaths, add_cref<std::string> entryName,
										  bool verbose) -> ShaderLink
{
	if (_addFromFile(type, fileName, includePaths, entryName, verbose).second != INVALID_UINT32)
	{
		ASSERTMSG((m_links.size() + 1u <= m_maxShaders), "Amount of shaders exceeds maxShaderCount");
		const uint32_t	shaderIndex = m_stageToCount.at(type) - 1u;
		return add(type, nextStage, shaderIndex, {});
	}
	return {};
}

auto ShaderObjectCollection::addFromFile (VkShaderStageFlagBits type, add_cref<std::string> fileName, add_cref<ShaderLink> parent,
										  add_cref<strings> includePaths, add_cref<std::string> entryName,
										  bool verbose) -> ShaderLink
{
	if (_addFromFile(type, fileName, includePaths, entryName, verbose).second != INVALID_UINT32)
	{
		ASSERTMSG((m_links.size() + 1u <= m_maxShaders), "Amount of shaders exceeds maxShaderCount");
		const uint32_t	shaderIndex = m_stageToCount.at(type) - 1u;
		return add(type, VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM, shaderIndex, parent);
	}
	return {};
}

void ShaderObjectCollection::updateShaders(std::initializer_list<ShaderLink> links,
										   bool buildAlways, bool enableValidation, bool genAssembly,
										   add_cref<Version> vulkanVer,
										   add_cref<Version> spirvVer)
{
	for (add_cref<ShaderLink> link : links)
	{
		updateShader(link, buildAlways, enableValidation, genAssembly, vulkanVer, spirvVer);
	}
}

void ShaderObjectCollection::updateShader (add_cref<ShaderLink> link, bool buildAlways,
										   bool enableValidation, bool genAssembly,
										   add_cref<Version> vulkanVer, add_cref<Version> spirvVer)
{
	add_ptr<IntShaderLink> update =
		static_cast<add_ptr<IntShaderLink>>(find(link.stage, link.index, m_links));
	ASSERTION(update);
	ShaderBuildOptions buildOptions;
	buildOptions.enableValidation	= enableValidation;
	buildOptions.buildAlways		= buildAlways;
	buildOptions.genAssembly		= genAssembly;
	buildOptions.vulkanVer			= vulkanVer;
	buildOptions.spirvVer			=spirvVer;
	update->data->options = buildOptions;
}

void ShaderObjectCollection::updateShaders (std::initializer_list<ShaderLink> links,
											std::initializer_list<ZDescriptorSetLayout> layouts)
{
	for (add_cref<ShaderLink> link : links)
	{
		updateShader(link, layouts);
	}
}

void ShaderObjectCollection::updateShader (add_cref<ShaderLink> link,
										   std::initializer_list<ZDescriptorSetLayout> layouts)
{
	add_ptr<IntShaderLink> update =
		static_cast<add_ptr<IntShaderLink>>(find(link.stage, link.index, m_links));
	ASSERTION(update);
	add_ref<std::vector<ZDescriptorSetLayout>> shaderLayouts =
		update->data->shader.getParamRef<std::vector<ZDescriptorSetLayout>>();
	shaderLayouts.clear();
	shaderLayouts.reserve(layouts.size());
	for (ZDescriptorSetLayout layout : layouts)
	{
		shaderLayouts.push_back(layout);
	}
}

void ShaderObjectCollection::updateShaders (std::initializer_list<ShaderLink> links,
											add_cref<ZPushConstants> pushConstants)
{
	VkShaderStageFlags newFlags = VkShaderStageFlags(0);
	for (add_cref<ShaderLink> link : links)
	{
		newFlags |= link.stage;
	}

	for (add_cref<ShaderLink> link : links)
	{
		add_ref<ZPushConstants> pc = pushc(link);
		pc = pushConstants;
		pc.updateStageFlags(newFlags);
	}
}

add_ref<ZSpecializationInfo> ShaderObjectCollection::spec (add_cref<ShaderLink> link)
{
	add_ptr<IntShaderLink> update =
		static_cast<add_ptr<IntShaderLink>>(find(link.stage, link.index, m_links));
	ASSERTION(update);
	return update->data->shader.getParamRef<ZSpecializationInfo>();
}

add_ref<ZPushConstants> ShaderObjectCollection::pushc (add_cref<ShaderLink> link)
{
	add_ptr<IntShaderLink> update =
		static_cast<add_ptr<IntShaderLink>>(find(link.stage, link.index, m_links));
	ASSERTION(update);
	return update->data->pushConstants;
}

extern	bool verifyShaderCode (
    uint32_t                    shaderIndex,
    VkShaderStageFlagBits       stage,
    add_cref<Version>           vulkanVer,
    add_cref<Version>           spirvVer,
    add_cref<strings>           codeAndEntryAndIncludes,
    add_ref<std::string>        shaderFileName,
    add_ref<std::vector<char>>  binary,
    add_ref<std::vector<char>>  assembly,
	add_ref<std::vector<char>>  disassembly,
    add_ref<std::string>        errors,
	add_ref<ProgressRecorder>	progressRecorder,
    bool enableValidation,
	add_cref<std::string>		spirvValArgs,
    bool genDisassmebly,
    bool buildAlways,
    bool genBinary);

extern void assertPushConstantSizeMax (ZDevice dev, const uint32_t size);

struct ShaderObjectCollection::ZShaderCreateInfoEXT : VkShaderCreateInfoEXT
{
	const bool								emptySpecInfo;
	const VkSpecializationInfo				specInfo;
	const std::vector<VkPushConstantRange>	pushRanges;
	std::vector<VkDescriptorSetLayout>		setLayouts;
	ZShaderCreateInfoEXT () = delete;
	ZShaderCreateInfoEXT (ZDevice device, add_ref<IntShaderLink> link,
						  VkShaderStageFlagBits nextStage, VkShaderCreateFlagsEXT flags)
		: emptySpecInfo	(link.collection.spec(link).empty())
		, specInfo		(emptySpecInfo ? VkSpecializationInfo() : link.collection.spec(link)())
		, pushRanges	(link.collection.pushc(link).ranges())
		, setLayouts	()
	{
		assertPushConstantSizeMax(device, link.collection.pushc(link).size());

		add_cref<std::vector<ZDescriptorSetLayout>> shaderSetLayouts =
			link.data->shader.getParamRef<std::vector<ZDescriptorSetLayout>>();
		if (false == shaderSetLayouts.empty())
		{
			setLayouts.resize(shaderSetLayouts.size());
			std::transform(shaderSetLayouts.begin(), shaderSetLayouts.end(), setLayouts.begin(),
				[](add_cref<ZDescriptorSetLayout> setLayout) { return *setLayout; });
		}

		add_ref<VkShaderCreateInfoEXT> createInfo(*this);
		createInfo = makeVkStruct();
		createInfo.flags		= flags;
		createInfo.stage		= link.stage;
		createInfo.nextStage	= nextStage;
		createInfo.codeType		= VK_SHADER_CODE_TYPE_SPIRV_EXT;
		createInfo.codeSize		= data_count(link.collection.getShaderCode(link.stage, link.index, true));
		createInfo.pCode		= data_or_null(link.collection.getShaderCode(link.stage, link.index, true));
		createInfo.pName		= link.collection.getShaderEntry(link.stage, link.index).c_str();
		createInfo.setLayoutCount	= data_count(setLayouts);
		createInfo.pSetLayouts		= data_or_null(setLayouts);
		createInfo.pushConstantRangeCount	= data_count(pushRanges);
		createInfo.pPushConstantRanges		= data_or_null(pushRanges);
		createInfo.pSpecializationInfo		= emptySpecInfo ? nullptr : &specInfo;
	}
	VkShaderCreateInfoEXT operator ()() const
	{
		return add_cref<VkShaderCreateInfoEXT>(*this);
	}
};

int ShaderObjectCollection::create (add_cref<Version> vulkanVer, add_cref<Version> spirvVer,
									add_ref<std::vector<IntShaderLink>> links, const bool package)
{
	for (add_ref<IntShaderLink> link : links)
	{
		ASSERTMSG(link.data->shader.has_handle() || link.data->shader.use_count() == 1u,
					link.data->shader, " Shader already in use, use_count() = ", link.data->shader.use_count());
	}
	ASSERTMSG(links.size() > (package ? 1u : 0u), 
		package ? "At least two stages are needed to create a package shaders"
				: "There must be at least one shader to create");

	add_cref<ZDeviceInterface>			di		= m_device.getInterface();
	ASSERTMSG(di.isShaderObjectEnabled(), "ERROR: \"" VK_EXT_SHADER_OBJECT_EXTENSION_NAME "\" not supported");

	add_ref<ProgressRecorder>			progressRecorder = m_device.getParamRef<ZPhysicalDevice>()
															.getParamRef<ZInstance>().getParamRef<ProgressRecorder>();

	std::vector<VkShaderEXT>			handles	(links.size(), VK_NULL_HANDLE);
	add_cref<ShaderObjectCollection>	me		(*this);
	std::vector<ZShaderCreateInfoEXT>	zInfos;

	zInfos.reserve(links.size());

	for (add_ref<IntShaderLink> link : links)
	{
		std::string errors, shaderFileName;
        std::vector<char> binary, assembly, disassembly;
		const StageAndIndex key = link.key();
		add_ptr<ShaderData> data = link.data;
		add_cref<ShaderBuildOptions> options = data->options;

		add_cref<GlobalAppFlags> gf = getGlobalAppFlags(); // TODO: read real param from buildAndVerify()
		const Version buildVulkanVer = options.vulkanVer ? options.vulkanVer : vulkanVer;
		const Version buildSpirvVer = options.spirvVer ? options.spirvVer : spirvVer;

		if (verifyShaderCode((link.index + 1024u), link.stage, buildVulkanVer, buildSpirvVer,
			me.m_stageToCode.at(key), shaderFileName, binary, assembly, disassembly, errors, progressRecorder,
			data->options.enableValidation, gf.spirvValArgs, data->options.genAssembly, data->options.buildAlways, true))
		{
			m_stageToFileName[key] = std::move(shaderFileName);
			m_stageToAssembly[key] = std::move(assembly);
			m_stageToBinary[key] = std::move(binary);
		}
		else ASSERTFALSE(errors);

		data->shader.setParam<ZDevice>(m_device);
		data->shader.getParamRef<std::vector<type_index_with_default>>() = data->pushConstants.types();

		zInfos.emplace_back(m_device, link, data->shader.getParam<ZDistType<SomeOne, VkShaderStageFlagBits>>(),
							package ? VkShaderCreateFlagsEXT(VK_SHADER_CREATE_LINK_STAGE_BIT_EXT) : VkShaderCreateFlagsEXT(0));
	}

	std::vector<VkShaderCreateInfoEXT> createInfos(zInfos.size());
	std::transform(zInfos.begin(), zInfos.end(), createInfos.begin(),
		[](add_cref<ZShaderCreateInfoEXT> ric) { return ric(); });

	VKASSERTMSG(di.vkCreateShadersEXT(
						*m_device,
						data_count(createInfos),
						data_or_null(createInfos),
						m_device.getParam<VkAllocationCallbacksPtr>(),
						handles.data()),
			"Failed to create shader object(s)");

	auto updateHandle = [&](uint32_t index, add_ptr<VkShaderEXT> handle)
	{
		*handle = handles.at(index);
	};

	uint32_t index = 0u;
	for (add_ref<IntShaderLink> link : links)
	{
		updateHandle(index++, link.data->shader.setter());
	}

	return make_signed(index);
}

void ShaderObjectCollection::buildAndVerify (add_cref<Version> vulkanVer, add_cref<Version> spirvVer)
{
	std::vector<IntShaderLink> standaloneShaders;
	std::vector<std::vector<IntShaderLink>> linkedShaders;

	for (uint32_t i = 0u; i < data_count(m_links); ++i)
	{
		add_ref<IntShaderLink> link = m_links.at(i);

		add_ptr<IntShaderLink> first = static_cast<add_ptr<IntShaderLink>>(link.head());
		if (first->count() == 1u && nullptr == find(first->stage, first->index, standaloneShaders))
		{
			standaloneShaders.emplace_back(nullptr, *first);
		}
		else
		{
			std::vector<IntShaderLink> shaders;
			while (first)
			{
				shaders.emplace_back(nullptr, *first);
				first = static_cast<add_ptr<IntShaderLink>>(static_cast<add_ptr<ShaderLink>>(first)->next);
				i = i + 1u;
			}
			linkedShaders.emplace_back(std::move(shaders));
		}
	}

	if (false == standaloneShaders.empty())
	{
		create(vulkanVer, spirvVer, standaloneShaders, false);
	}

	for (add_ref<std::vector<IntShaderLink>> links : linkedShaders)
	{
		create(vulkanVer, spirvVer, links, true);
	}
}

ZShaderObject ShaderObjectCollection::getShader (VkShaderStageFlagBits stage, uint32_t index)
{
	add_cptr<IntShaderLink> update = static_cast<add_ptr<IntShaderLink>>(find(stage, index, m_links));
	ASSERTION(update);
	return update->data->shader;
}

ZShaderObject ShaderObjectCollection::getShader (add_cref<ShaderLink> link)
{
	add_cptr<IntShaderLink> update = static_cast<add_ptr<IntShaderLink>>(find(link.stage, link.index, m_links));
	ASSERTION(update);
	return update->data->shader;
}

static void commandBufferBindShaders (
	ZCommandBuffer cmdBuffer,
	std::initializer_list<ZShaderObject> shaders,
	bool unbind)
{
	const uint32_t shaderCount = uint32_t(shaders.size());

	if (shaderCount == 0u) return;

	ASSERTMSG(shaderCount <= 64u, "Currently supported shader count is 64");
	add_cref<ZDeviceInterface> di = cmdBuffer.getParamRef<ZDevice>().getInterface();
	ASSERTMSG(di.isShaderObjectEnabled(), "ERROR: \"" VK_EXT_SHADER_OBJECT_EXTENSION_NAME "\" not supported");

	VkShaderEXT				list[64];
	VkShaderStageFlagBits	stages[64];
	for (auto beg = shaders.begin(), i = beg; i != shaders.end(); ++i)
	{
		const auto j = std::distance(beg, i);
		list[j]	= unbind ? VK_NULL_HANDLE : **i;
		stages[j] = i->getParam<ZDistType<SomeZero, VkShaderStageFlagBits>>();
	}
	di.vkCmdBindShadersEXT(*cmdBuffer, shaderCount, stages, list);
}

void commandBufferBindShaders (
	ZCommandBuffer cmd,
	std::initializer_list<ZShaderObject> shaders)
{
	commandBufferBindShaders(cmd, shaders, false);
}

void commandBufferUnbindShaders (
	ZCommandBuffer cmd,
	std::initializer_list<ZShaderObject> shaders)
{
	commandBufferBindShaders(cmd, shaders, true);
}

} // namespace vtf
