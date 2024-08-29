#include "vtfZShaderObject.hpp"
#include "vtfStructUtils.hpp"
#include "vtfZUtils.hpp"

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
	bool enableValidation = false;
	bool buildAlways = false;
	bool genAssembly = false;
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
													  VkShaderStageFlagBits stage, uint32_t index)
	: ShaderLink	()
	, collection	(coll)
	, data			(new ShaderData)
	, autoDelete	(true)
{
	this->stage = stage;
	this->index = index;
	ASSERTION(data);
}
ShaderObjectCollection::IntShaderLink::IntShaderLink (IntShaderLink&& other) noexcept
	: ShaderLink	(std::forward<ShaderLink>(other))
	, collection	(other.collection)
	, data			(other.data)
	, autoDelete	(other.autoDelete)
{
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

auto ShaderObjectCollection::add (VkShaderStageFlagBits type, uint32_t shaderIndex, add_cref<ShaderLink> parent) -> ShaderLink
{
	m_links.emplace_back(*this, type, shaderIndex);
	add_ptr<ShaderLink> pNewLink = &m_links.back();
	{
		add_ptr<IntShaderLink> iNewLink = static_cast<add_ptr<IntShaderLink>>(pNewLink);
		iNewLink->data->shader.setParam<ZDevice>(m_device);
		iNewLink->data->shader.setParam<VkShaderStageFlagBits>(VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM);
	}
	if (add_ptr<ShaderLink> pPrevLink = find(parent.stage, parent.index, m_links); pPrevLink)
	{
		pPrevLink->next = pNewLink;
		pNewLink->prev = pPrevLink;
	}
	return *pNewLink;
}

auto ShaderObjectCollection::addFromText (VkShaderStageFlagBits type,
										  add_cref<std::string> code, add_cref<ShaderLink> parent,
										  add_cref<strings> includePaths, add_cref<std::string> entryName) -> ShaderLink
{
	ASSERTMSG((m_links.size() + 1u <= m_maxShaders), "Amount of shaders exceeds maxShaderCount");
	_GlSpvProgramCollection::addFromText(type, code, includePaths, entryName);
	const uint32_t	shaderIndex = m_stageToCount.at(type) - 1u;
	return add(type, shaderIndex, parent);
}

auto ShaderObjectCollection::addFromFile (VkShaderStageFlagBits type, add_cref<std::string> fileName, add_cref<ShaderLink> parent,
										  add_cref<strings> includePaths, add_cref<std::string> entryName,
										  bool verbose) -> ShaderLink
{
	if (_GlSpvProgramCollection::addFromFile(type, fileName, includePaths, entryName, verbose))
	{
		ASSERTMSG((m_links.size() + 1u <= m_maxShaders), "Amount of shaders exceeds maxShaderCount");
		const uint32_t	shaderIndex = m_stageToCount.at(type) - 1u;
		return add(type, shaderIndex, parent);
	}
	return {};
}

void ShaderObjectCollection::updateShader (add_cref<ShaderLink> link, bool buildAlways,
										   bool enableValidation, bool genAssembly)
{
	add_ptr<IntShaderLink> update =
		static_cast<add_ptr<IntShaderLink>>(find(link.stage, link.index, m_links));
	ASSERTION(update);
	ShaderBuildOptions buildOptions;
	buildOptions.enableValidation	= enableValidation;
	buildOptions.buildAlways		= buildAlways;
	buildOptions.genAssembly		= genAssembly;
	update->data->options = buildOptions;
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
	shaderLayouts.resize(layouts.size());
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

extern	bool verifyShaderCode (uint32_t shaderIndex, VkShaderStageFlagBits stage,
							   add_cref<Version> vulkanVer, add_cref<Version> spirvVer,
							   add_cref<strings> codeAndEntryAndIncludes,
							   add_ref<std::string> shaderFileName,
							   add_ref<std::vector<uint8_t>> binary,
							   add_ref<std::vector<uint8_t>> assembly,
							   add_ref<std::string> errors,
							   bool enableValidation,
							   bool genDisassmebly,
							   bool buildAlways,
							   bool genBinary);

struct ShaderObjectCollection::ZShaderCreateInfoEXT : VkShaderCreateInfoEXT
{
	const bool							emptySpecInfo;
	const VkSpecializationInfo			specInfo;
	std::vector<VkPushConstantRange>	userRanges;
	ZShaderCreateInfoEXT () = delete;
	ZShaderCreateInfoEXT (add_ref<IntShaderLink> link,
						  VkShaderStageFlagBits nextStage, VkShaderCreateFlagsEXT flags)
		: emptySpecInfo	(link.collection.spec(link).empty())
		, specInfo		(emptySpecInfo ? VkSpecializationInfo() : link.collection.spec(link)())
		, userRanges	(link.collection.pushc(link).ranges())
	{
		add_cref<std::vector<ZDescriptorSetLayout>> shaderSetLayouts =
			link.data->shader.getParamRef<std::vector<ZDescriptorSetLayout>>();
		std::vector<VkDescriptorSetLayout> setLayouts;
		if (!shaderSetLayouts.empty())
		{
			setLayouts.resize(shaderSetLayouts.size());
			std::transform(shaderSetLayouts.begin(), shaderSetLayouts.end(), setLayouts.begin(),
				[](add_cref<ZDescriptorSetLayout> setLayout) { return *setLayout; });
		}

		add_ref<std::vector<VkPushConstantRange>> shaderRanges =
			link.data->shader.getParamRef<std::vector<VkPushConstantRange>>();
		shaderRanges = std::move(userRanges);

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
		createInfo.pushConstantRangeCount	= data_count(shaderRanges);
		createInfo.pPushConstantRanges		= data_or_null(shaderRanges);
		createInfo.pSpecializationInfo		= emptySpecInfo ? nullptr : &specInfo;
	}
	VkShaderCreateInfoEXT operator ()() const
	{
		return add_cref<VkShaderCreateInfoEXT>(*this);
	}
};

int ShaderObjectCollection::create (add_cref<Version> vulkanVer, add_cref<Version> spirvVer,
									add_ref<std::vector<IntShaderLink>> links)
{
	int handleCount = 0;
	for (add_ref<IntShaderLink> link : links)
	{
		ASSERTION(link.count() == 1u);
		if (link.data->shader.has_handle())
			continue;
		++handleCount;
	}
	if (0 == handleCount)
	{
		return 0;
	}

	add_cref<ZDeviceInterface>			di		= deviceGetInterface(m_device);
	ASSERTMSG(di.shaderObject(), "ERROR: \"" VK_EXT_SHADER_OBJECT_EXTENSION_NAME "\" not supported");

	std::vector<VkShaderEXT>			handles	(make_unsigned(handleCount), VK_NULL_HANDLE);
	add_cref<ShaderObjectCollection>	me		(*this);
	std::vector<ZShaderCreateInfoEXT>	zInfos;

	zInfos.reserve(links.size());

	for (add_ref<IntShaderLink> link : links)
	{
		if (link.data->shader.has_handle())
			continue;

		std::string errors, shaderFileName;
		std::vector<uint8_t> binary, assembly;
		const StageAndIndex key = link.key();
		add_ptr<ShaderData> data = link.data;

		if (verifyShaderCode(link.index, link.stage, vulkanVer, spirvVer,
			me.m_stageToCode.at(key), shaderFileName, binary, assembly, errors,
			data->options.enableValidation, data->options.genAssembly, data->options.buildAlways, true))
		{
			m_stageToFileName[key] = std::move(shaderFileName);
			m_stageToAssembly[key] = std::move(assembly);
			m_stageToBinary[key] = std::move(binary);
		}
		else ASSERTMSG(false, errors);

		data->shader.setParam<VkShaderStageFlagBits>(link.stage);

		zInfos.emplace_back(link, VkShaderStageFlagBits(0), VkShaderCreateFlagsEXT(0));
	}

	std::vector<VkShaderCreateInfoEXT> createInfos(zInfos.size());
	std::transform(zInfos.begin(), zInfos.end(), createInfos.begin(),
		[](add_cref<ZShaderCreateInfoEXT> ric) { return ric(); });

	VKASSERT3(di.vkCreateShadersEXT(
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

	int index = 0u;
	for (add_ref<IntShaderLink> link : links)
	{
		if (link.data->shader.has_handle())
			continue;
		updateHandle(make_unsigned(index++), link.data->shader.setter());
	}

	ASSERTION(handleCount == index);

	return handleCount;
}

bool ShaderObjectCollection::create (add_cref<Version> vulkanVer, add_cref<Version> spirvVer, add_ref<IntShaderLink> link)
{
	ASSERTION(link.count() > 1u);
	if (link.data->shader.has_handle())
	{
		return false;
	}

	add_cref<ZDeviceInterface> di = deviceGetInterface(m_device);
	ASSERTMSG(di.shaderObject(), "ERROR: \"" VK_EXT_SHADER_OBJECT_EXTENSION_NAME "\" not supported");

	std::vector<VkShaderEXT>			handles	(link.count(), VK_NULL_HANDLE);
	add_cref<ShaderObjectCollection>	me		(*this);
	std::vector<ZShaderCreateInfoEXT>	zInfos;

	zInfos.reserve(link.count());

	add_ptr<ShaderLink> next = &link;
	while (next)
	{
		std::string errors, shaderFileName;
		std::vector<uint8_t> binary, assembly;
		const StageAndIndex key = next->key();
		add_ptr<IntShaderLink> pLink = static_cast<add_ptr<IntShaderLink>>(next);
		add_ptr<ShaderData> data = pLink->data;

		ASSERTMSG(false == data->shader.has_handle(),
			"Shader already created. Don't add shader(s) after build, currently unsupported.");

		if (verifyShaderCode(next->index, next->stage, vulkanVer, spirvVer,
			me.m_stageToCode.at(key), shaderFileName, binary, assembly, errors,
			data->options.enableValidation, data->options.genAssembly, data->options.buildAlways, true))
		{
			m_stageToFileName[key] = std::move(shaderFileName);
			m_stageToAssembly[key] = std::move(assembly);
			m_stageToBinary[key] = std::move(binary);
		}
		else ASSERTMSG(false, errors);

		const VkShaderStageFlagBits oldStage = data->shader.getParam<VkShaderStageFlagBits>();
		UNREF(oldStage);

		data->shader.setParam<VkShaderStageFlagBits>(next->stage);

		next = next->next;

		zInfos.emplace_back(*pLink, (next ? next->stage : VkShaderStageFlagBits(0)), VK_SHADER_CREATE_LINK_STAGE_BIT_EXT);
	}

	std::vector<VkShaderCreateInfoEXT> createInfos(zInfos.size());
	std::transform(zInfos.begin(), zInfos.end(), createInfos.begin(),
		[](add_ref<ZShaderCreateInfoEXT> ric) { return ric(); });

	VKASSERT3(di.vkCreateShadersEXT(
						*m_device,
						data_count(createInfos),
						data_or_null(createInfos),
						m_device.getParam<VkAllocationCallbacksPtr>(),
						handles.data()),
				"Failed to create shader object");

	auto updateHandle = [&](uint32_t index, add_ptr<VkShaderEXT> handle)
	{
		*handle = handles.at(index);
	};

	uint32_t index = 0;
	add_ptr<IntShaderLink> iLink = &link;
	while (iLink)
	{
		updateHandle(index++, iLink->data->shader.setter());
		iLink = static_cast<add_ptr<IntShaderLink>>(iLink->next);
	}

	ASSERTION(handles.size() == index);

	return true;
}

void ShaderObjectCollection::buildAndVerify (add_cref<Version> vulkanVer, add_cref<Version> spirvVer)
{
	std::vector<IntShaderLink> standaloneShaders, linkedShaders;

	for (add_ref<IntShaderLink> link : m_links)
	{
		add_ptr<IntShaderLink> first = static_cast<add_ptr<IntShaderLink>>(link.head());
		if (first->count() == 1u && nullptr == find(first->stage, first->index, standaloneShaders))
		{
			standaloneShaders.emplace_back(nullptr, *first);
		}
		else if (nullptr == find(first->stage, first->index, linkedShaders))
		{
			linkedShaders.emplace_back(nullptr, *first);
		}
	}

	create(vulkanVer, spirvVer, standaloneShaders);

	for (add_ref<IntShaderLink> link : linkedShaders)
	{
		create(vulkanVer, spirvVer, link);
	}
}

ZShaderObject ShaderObjectCollection::getShader (VkShaderStageFlagBits stage, uint32_t index)
{
	add_ptr<IntShaderLink> update =
		static_cast<add_ptr<IntShaderLink>>(find(stage, index, m_links));
	ASSERTION(update);
	return update->data->shader;
}

void commandBufferBindShaders (ZCommandBuffer cmdBuffer, std::initializer_list<ZShaderObject> shaders)
{
	const uint32_t shaderCount = uint32_t(shaders.size());

	if (shaderCount == 0u) return;

	ASSERTMSG(shaderCount <= 64u, "Currently supported shader count is 64");
	add_cref<ZDeviceInterface> di = deviceGetInterface(cmdBuffer.getParam<ZDevice>());
	ASSERTMSG(di.shaderObject(), "ERROR: \"" VK_EXT_SHADER_OBJECT_EXTENSION_NAME "\" not supported");

	VkShaderEXT				list[64];
	VkShaderStageFlagBits	stages[64];
	for (auto beg = shaders.begin(), i = beg; i != shaders.end(); ++i)
	{
		const auto j = std::distance(beg, i);
		list[j]	= **i;
		stages[j] = i->getParam<VkShaderStageFlagBits>();
	}
	di.vkCmdBindShadersEXT(*cmdBuffer, shaderCount, stages, list);
}

} // namespace vtf
