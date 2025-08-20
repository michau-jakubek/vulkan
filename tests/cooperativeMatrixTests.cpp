#include "cooperativeMatrixTests.hpp"

#include "vtfStructUtils.hpp"
#include "vtfBacktrace.hpp"
#include "vtfProgramCollection.hpp"
#include "vtfLayoutManager.hpp"
#include "vtfZCommandBuffer.hpp"
#include "vtfZPipeline.hpp"
#include "vtfFloat16.hpp"

namespace coopmat
{
using namespace vtf;

constexpr Option optionBuildAlways		{ "--build-always", 0 };
constexpr Option optionVariant			{ "-v", 1 };
constexpr Option optionConfiguration	{ "-c", 1 };
constexpr Option optionUseSpirvShader	{ "-s", 0 };
OptionParser<CoopParams> CoopParams::getParser()
{
	OptionFlags					flags(OptionFlag::PrintDefault);
	OptionParser<CoopParams>	parser(*this);
	parser.addOption(&CoopParams::Variant, optionVariant,
		"Variant to test: 0-all, 1-A, 2-B, 3-C, 4-D", { 0 }, flags);
	parser.addOption(&CoopParams::Configuration, optionConfiguration,
		"Configuration to test or all if index exceeds or not specified", { 0 }, flags);
	parser.addOption(&CoopParams::buildAlways, optionBuildAlways,
		"Build always", { false }, flags);
	parser.addOption(&CoopParams::useSpirvShader, optionUseSpirvShader,
		"Use SPPIR-V shader instead of GLSL", { false }, flags);
	return parser;
}
const char* VkComponentTypeToString(VkComponentTypeKHR type) {
	static std::pair<VkComponentTypeKHR, const char*> names[]
	{
		{ VK_COMPONENT_TYPE_FLOAT16_KHR,        STRINGIZE(FLOAT16_KHR) },
		{ VK_COMPONENT_TYPE_FLOAT32_KHR,        STRINGIZE(FLOAT32_KHR) },
		{ VK_COMPONENT_TYPE_FLOAT64_KHR,        STRINGIZE(FLOAT64_KHR) },
		{ VK_COMPONENT_TYPE_SINT8_KHR,          STRINGIZE(SINT8_KHR) },
		{ VK_COMPONENT_TYPE_SINT16_KHR,         STRINGIZE(SINT16_KHR) },
		{ VK_COMPONENT_TYPE_SINT32_KHR,         STRINGIZE(SINT32_KHR) },
		{ VK_COMPONENT_TYPE_SINT64_KHR,         STRINGIZE(SINT64_KHR) },
		{ VK_COMPONENT_TYPE_UINT8_KHR,          STRINGIZE(UINT8_KHR) },
		{ VK_COMPONENT_TYPE_UINT16_KHR,         STRINGIZE(UINT16_KHR) },
		{ VK_COMPONENT_TYPE_UINT32_KHR,         STRINGIZE(UINT32_KHR) },
		{ VK_COMPONENT_TYPE_UINT64_KHR,         STRINGIZE(UINT64_KHR) },
		{ VK_COMPONENT_TYPE_BFLOAT16_KHR,       STRINGIZE(BFLOAT16_KHR) },
		{ VK_COMPONENT_TYPE_SINT8_PACKED_NV,    STRINGIZE(SINT8_PACKED_NV) },
		{ VK_COMPONENT_TYPE_UINT8_PACKED_NV,    STRINGIZE(UINT8_PACKED_NV) },
		{ VK_COMPONENT_TYPE_FLOAT_E4M3_NV,      STRINGIZE(FLOAT_E4M3_NV) },
		{ VK_COMPONENT_TYPE_FLOAT_E5M2_NV,      STRINGIZE(FLOAT_E5M2_NV) },
	};
	for (const auto& n : names)
	{
		if (n.first == type)
			return n.second;
	}
	return "unknown";
}
const char* VkScopeToString(VkScopeKHR scope)
{
	static std::pair<VkScopeKHR, const char*> scopes[]
	{
		{ VK_SCOPE_DEVICE_KHR,          STRINGIZE(DEVICE_KHR) },
		{ VK_SCOPE_WORKGROUP_KHR,       STRINGIZE(WORKGROUP_KHR) },
		{ VK_SCOPE_SUBGROUP_KHR,        STRINGIZE(SUBGROUP_KHR) },
		{ VK_SCOPE_QUEUE_FAMILY_KHR,    STRINGIZE(QUEUE_FAMILY_KHR) },
	};
	for (const auto& s : scopes)
	{
		if (s.first == scope)
			return s.second;
	}
	return "unknown";
}
void printVkCooperativeMatrixPropertiesKHR(
	add_cref<VkCooperativeMatrixPropertiesKHR> p,
	uint32_t index, add_ref<std::ostream> str)
{
	str << index << ":  ABCR: ";
	str << VkComponentTypeToString(p.AType) << ", ";
	str << VkComponentTypeToString(p.BType) << ", ";
	str << VkComponentTypeToString(p.CType) << ", ";
	str << VkComponentTypeToString(p.ResultType) << ", ";
	str << VkScopeToString(p.scope) << ' ';
	str << "MKN: " << p.MSize << ',' << p.KSize << ',' << p.NSize;
}

bool anyComponentOf(add_cref<VkCooperativeMatrixPropertiesKHR> conf, std::initializer_list<VkComponentTypeKHR> list)
{
	for (const auto c : list)
	{
		if (conf.AType == c || conf.BType == c || conf.CType == c || conf.ResultType == c)
			return true;
	}
	return false;
}

bool CoopParams::selectMatrixProperties(ZInstance instance, ZPhysicalDevice physicalDevice, add_ref<std::ostream> str)
{
	str << "Looking for valid configuration..." << std::endl;

	bool validConfiguration = false;
	auto pfn = instance.getInterface().vkGetPhysicalDeviceCooperativeMatrixPropertiesKHR;
	if (pfn)
	{
		uint32_t propertyCount = 0u;
		VKASSERT((*pfn)(*physicalDevice, &propertyCount, nullptr));
		std::vector<VkCooperativeMatrixPropertiesKHR>
			props(propertyCount, makeVkStructT<VkCooperativeMatrixPropertiesKHR>());
		VKASSERT((*pfn)(*physicalDevice, &propertyCount, props.data()));
		for (const VkCooperativeMatrixPropertiesKHR& prop : props)
		{
			if (prop.scope == VK_SCOPE_SUBGROUP_KHR && anyComponentOf(prop,
				{ VK_COMPONENT_TYPE_SINT8_KHR, VK_COMPONENT_TYPE_SINT16_KHR,
				  VK_COMPONENT_TYPE_SINT32_KHR, VK_COMPONENT_TYPE_SINT64_KHR,
				  VK_COMPONENT_TYPE_UINT8_KHR, VK_COMPONENT_TYPE_UINT16_KHR,
				  VK_COMPONENT_TYPE_UINT32_KHR, VK_COMPONENT_TYPE_UINT64_KHR,
				  VK_COMPONENT_TYPE_FLOAT16_KHR, VK_COMPONENT_TYPE_FLOAT32_KHR,
				  VK_COMPONENT_TYPE_FLOAT64_KHR
				}) && (!(anyComponentOf(prop,
					{ VK_COMPONENT_TYPE_BFLOAT16_KHR,
					  VK_COMPONENT_TYPE_FLOAT_E4M3_NV,
					  VK_COMPONENT_TYPE_FLOAT_E5M2_NV }
					))))
			{
				confs.push_back(prop);
			}
		}
		return true;
	}
	ASSERTMSG(validConfiguration, __func__, "()");
	return false;
}

add_cref<VkCooperativeMatrixPropertiesKHR> CoopParams::getSelectedConfiguration() const
{
	ASSERTMSG(confs.size(), "No configuration to perform test");
	return confs.at(Configuration >= confs.size() ? 0u : Configuration);
}

TriLogicInt prepareTests(add_cref<TestRecord> record, add_cref<strings> cmdLineParams);
TriLogicInt createDeviceAndPerformTest(ZInstance instance, ZPhysicalDevice physicalDevice, add_cref<CoopParams> params);
TriLogicInt performTests(add_ref<VulkanContext> ctx, add_cref<CoopParams> params);

TriLogicInt prepareTests(add_cref<TestRecord> record, add_cref<strings> cmdLineParams)
{
	add_cref<GlobalAppFlags> gf = getGlobalAppFlags();

	CoopParams					params(record.assets);
	OptionParser<CoopParams>	parser = params.getParser();
	parser.parse(cmdLineParams);
	OptionParserState			state = parser.getState();

	if (state.hasErrors || state.hasWarnings)
	{
		std::cout << state.messagesText() << std::endl;
		if (state.hasErrors) return {};
	}

	if (state.hasHelp)
	{
		parser.printOptions(std::cout);
		std::cout << std::endl;
	}

	ZInstance					instance = createInstance(
		record.name, getAllocationCallbacks(), gf.layers, upgradeInstanceExtensions(strings()), Version(1, 3));
	ZPhysicalDevice				physicalDevice = selectPhysicalDevice(
		make_signed(gf.physicalDeviceIndex), instance, upgradeDeviceExtensions(strings()));

	VkPhysicalDeviceVulkan12Features vulkanMemoryModel = makeVkStruct();
	VkPhysicalDeviceCooperativeMatrixFeaturesKHR cooperativeMatrix = makeVkStruct(&vulkanMemoryModel);
	deviceGetPhysicalFeatures2(physicalDevice, &cooperativeMatrix);
	if (!vulkanMemoryModel.vulkanMemoryModel)
	{
		std::cout << "[ERROR] vulkanMemoryModel not supported" << std::endl;
		return {};
	}
	if (!cooperativeMatrix.cooperativeMatrix)
	{
		std::cout << "[ERROR] cooperativeMatrix not supported" << std::endl;
		return {};
	}

	params.selectMatrixProperties(instance, physicalDevice, std::cout);
	if (state.hasHelp)
	{
		uint32_t index = 0u;
		for (add_cref<VkCooperativeMatrixPropertiesKHR> conf : params.confs)
		{
			printVkCooperativeMatrixPropertiesKHR(conf, index++, std::cout);
			std::cout << std::endl;
		}
		return {};
	}

	if (params.confs.empty())
	{
		std::cout << "[ERROR] No configurations to perform test" << std::endl;
		return {};
	}

	VkPhysicalDeviceSubgroupProperties subgroupProperties = makeVkStruct();
	deviceGetPhysicalProperties2(physicalDevice, &subgroupProperties);
	params.subgroupSize = subgroupProperties.subgroupSize;

	params.allConfigurations = false == parser.getOptionByName(optionConfiguration)->getTouched();

	TriLogicInt result;
	if (params.allConfigurations || params.Configuration >= params.confs.size())
	{
		std::vector<TriLogicInt> results(params.confs.size());
		for (uint32_t c = 0u; c < data_count(params.confs); ++c)
		{
			params.Configuration = c;
			params.allConfigurations = true;
			results[c] = createDeviceAndPerformTest(instance, physicalDevice, params);
		}
		result = std::all_of(results.begin(), results.end(),
					[](add_cref<TriLogicInt> res) { return res.hasValue() && res.value() == 0; })
						? 0 : 1;
	}
	else
	{
		result = createDeviceAndPerformTest(instance, physicalDevice, params);
	}

	return result;
}
TriLogicInt createDeviceAndPerformTest(ZInstance instance, ZPhysicalDevice physicalDevice, add_cref<CoopParams> params)
{
	auto onEnablingFeatures = [&](add_ref<DeviceCaps> caps)
	{
		const auto cooperativeMatrix = &VkPhysicalDeviceCooperativeMatrixFeaturesKHR::cooperativeMatrix;
		auto cm = caps.addUpdateFeatureIf(cooperativeMatrix);
		cm.checkSupported("cooperativeMatrix");

		const auto vulkanMemoryModel = &VkPhysicalDeviceVulkan12Features::vulkanMemoryModel;
		auto vmm = caps.addUpdateFeatureIf(vulkanMemoryModel);
		vmm.checkSupported("vulkanMemoryModel");

		const auto maintenance4 = &VkPhysicalDeviceVulkan13Features::maintenance4;
		auto m4 = caps.addUpdateFeatureIf(maintenance4);
		m4.checkSupported("maintenance4");

		const auto storageBuffer16BitAccess = &VkPhysicalDevice16BitStorageFeatures::storageBuffer16BitAccess;
		auto sb16 = caps.addUpdateFeatureIf(storageBuffer16BitAccess);
		sb16.checkSupported("storageBuffer16BitAccess");

		/*
		const auto storageBuffer8BitAccess = &VkPhysicalDevice8BitStorageFeatures::storageBuffer8BitAccess;
		auto sb8 = caps.addUpdateFeatureIf(storageBuffer8BitAccess);
		sb8.checkSupported("storageBuffer8BitAccess");
		*/

		const auto shaderFloat16 = &VkPhysicalDeviceVulkan12Features::shaderFloat16;
		auto sh16 = caps.addUpdateFeatureIf(shaderFloat16);
		sh16.checkSupported("shaderFloat16");

		const auto shaderInt8 = &VkPhysicalDeviceVulkan12Features::shaderInt8;
		auto sh8 = caps.addUpdateFeatureIf(shaderInt8);
		sh8.checkSupported("shaderFloat8");

		const auto storageBuffer8BitAccess = &VkPhysicalDeviceVulkan12Features::storageBuffer8BitAccess;
		auto sb8 = caps.addUpdateFeatureIf(storageBuffer8BitAccess);
		sb8.checkSupported("storageBuffer8BitAccess");

		caps.addExtension(VK_KHR_COOPERATIVE_MATRIX_EXTENSION_NAME);
		caps.addExtension(VK_KHR_SHADER_SUBGROUP_EXTENDED_TYPES_EXTENSION_NAME);
		caps.addExtension(VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME);
	};

	add_cref<GlobalAppFlags> gf = getGlobalAppFlags();
	ZDevice device = createLogicalDevice(physicalDevice, onEnablingFeatures, ZSurfaceKHR(), gf.debugPrintfEnabled);
	VulkanContext ctx(instance, physicalDevice, device);

	return performTests(ctx, params);
}

namespace
{

template<typename T, typename = void>
struct has_asFloat : std::false_type {};
template<typename T>
struct has_asFloat<T, std::void_t<decltype(std::declval<const T>().asFloat())>> : std::true_type {};
template<class ValueType, bool> struct AsFloatAdapter
{
	float convert(const ValueType& v) const {
		return static_cast<float>(v);
	}
	ValueType construct(float f) const {
		return static_cast<ValueType>(f);
	}
};

template<class ValueType> struct AsFloatAdapter<ValueType, true>
{
	float convert(const ValueType& v) const {
		return v.asFloat();
	}
	ValueType construct(float f) const {
		return ValueType(f);
	}
};

template<VkComponentTypeKHR comp_type> struct ValueImpl
{
	typedef vK_component_type_to_ctype<comp_type> type;
	const VkComponentTypeKHR component = comp_type;
	type value{};
	uint32_t size() const {
		return uint32_t(sizeof(type));
	}
	std::vector<float> readBuffer(ZBuffer buffer, uint32_t elemCount) const {
		std::vector<float> result(elemCount);
		const BufferTexelAccess<type> access(buffer, elemCount, 1u);
		const AsFloatAdapter<type, has_asFloat<type>::value> adapter;
		for (uint32_t i = 0; i < elemCount; ++i)
			result[i] = adapter.convert(access.at(i, 0, 0));
		return result;
	}
	void writeBuffer(ZBuffer buffer, add_cref<std::vector<float>> data) const {
		const uint32_t elemCount = data_count(data);
		BufferTexelAccess<type> access(buffer, elemCount, 1u);
		const AsFloatAdapter<type, has_asFloat<type>::value> adapter;
		for (uint32_t i = 0; i < elemCount; ++i)
			access.at(i, 0, 0) = adapter.construct(data[i]);
	}
};

static_assert(std::is_same_v<uint32_t, vK_component_type_to_ctype<VK_COMPONENT_TYPE_UINT32_KHR>>, "???");

class ValueGenerator
{
	std::vector<float> m_values;
	uint32_t m_current;
public:
	ValueGenerator(VkComponentTypeKHR type)
		: m_values()
		, m_current(0)
	{
		switch (type)
		{
		case VK_COMPONENT_TYPE_FLOAT16_KHR:
		case VK_COMPONENT_TYPE_BFLOAT16_KHR:
		case VK_COMPONENT_TYPE_FLOAT32_KHR:
		case VK_COMPONENT_TYPE_FLOAT64_KHR:
			m_values = { -1.0f, -0.5f, 0.0f, +0.5f, +1.0f };
			break;
		case VK_COMPONENT_TYPE_SINT8_KHR:
		case VK_COMPONENT_TYPE_SINT16_KHR:
		case VK_COMPONENT_TYPE_SINT32_KHR:
		case VK_COMPONENT_TYPE_SINT64_KHR:
			m_values = { 0.0f, -1.0f, +1.0f };
			break;
		case VK_COMPONENT_TYPE_UINT8_KHR:
		case VK_COMPONENT_TYPE_UINT16_KHR:
		case VK_COMPONENT_TYPE_UINT32_KHR:
		case VK_COMPONENT_TYPE_UINT64_KHR:
			m_values = { 1.0f, 0.0f };
			break;
		default:
			ASSERTFALSE(__func__, "(): Unknown type ", VkComponentTypeToString(type));
		}
	}
	float next()
	{
		const float val = m_values[m_current];
		m_current = uint32_t((m_current + 1u) % m_values.size());
		return val;
	}
};

struct Value
{
	std::variant<
		ValueImpl<VK_COMPONENT_TYPE_SINT8_KHR>,
		ValueImpl<VK_COMPONENT_TYPE_SINT16_KHR>,
		ValueImpl<VK_COMPONENT_TYPE_SINT32_KHR>,
		ValueImpl<VK_COMPONENT_TYPE_SINT64_KHR>,
		ValueImpl<VK_COMPONENT_TYPE_UINT8_KHR>,
		ValueImpl<VK_COMPONENT_TYPE_UINT16_KHR>,
		ValueImpl<VK_COMPONENT_TYPE_UINT32_KHR>,
		ValueImpl<VK_COMPONENT_TYPE_UINT64_KHR>,
		ValueImpl<VK_COMPONENT_TYPE_BFLOAT16_KHR>,
		ValueImpl<VK_COMPONENT_TYPE_FLOAT16_KHR>,
		ValueImpl<VK_COMPONENT_TYPE_FLOAT32_KHR>,
		ValueImpl<VK_COMPONENT_TYPE_FLOAT64_KHR>
	> value;
	uint32_t size() const {
		return std::visit([](const auto& x) { return x.size(); }, value);
	}
	template<VkComponentTypeKHR type> void create() {
		value.emplace<ValueImpl<type>>();
	}
	Value(VkComponentTypeKHR type)
	{
		static std::unordered_map<VkComponentTypeKHR, void(Value::*)()> factory
		{
			{ VK_COMPONENT_TYPE_SINT8_KHR, &Value::create<VK_COMPONENT_TYPE_SINT8_KHR> },
			{ VK_COMPONENT_TYPE_SINT16_KHR, &Value::create<VK_COMPONENT_TYPE_SINT16_KHR> },
			{ VK_COMPONENT_TYPE_SINT32_KHR, &Value::create<VK_COMPONENT_TYPE_SINT32_KHR> },
			{ VK_COMPONENT_TYPE_SINT64_KHR, &Value::create<VK_COMPONENT_TYPE_SINT64_KHR> },
			{ VK_COMPONENT_TYPE_UINT8_KHR, &Value::create<VK_COMPONENT_TYPE_UINT8_KHR> },
			{ VK_COMPONENT_TYPE_UINT16_KHR, &Value::create<VK_COMPONENT_TYPE_UINT16_KHR> },
			{ VK_COMPONENT_TYPE_UINT32_KHR, &Value::create<VK_COMPONENT_TYPE_UINT32_KHR> },
			{ VK_COMPONENT_TYPE_UINT64_KHR, &Value::create<VK_COMPONENT_TYPE_UINT64_KHR> },
			{ VK_COMPONENT_TYPE_FLOAT16_KHR, &Value::create<VK_COMPONENT_TYPE_FLOAT16_KHR> },
			{ VK_COMPONENT_TYPE_FLOAT32_KHR, &Value::create<VK_COMPONENT_TYPE_FLOAT32_KHR> },
			{ VK_COMPONENT_TYPE_FLOAT64_KHR, &Value::create<VK_COMPONENT_TYPE_FLOAT64_KHR> },
			{ VK_COMPONENT_TYPE_BFLOAT16_KHR, &Value::create<VK_COMPONENT_TYPE_BFLOAT16_KHR> },
		};
		ASSERTMSG(factory.find(type) != factory.end(),
			"Missing factory for type ", VkComponentTypeToString(type));
		(this->*factory[type])();
	}
	std::vector<float> readBuffer(ZBuffer buffer, uint32_t elemCount) const {
		return std::visit([&](const auto& x) { return x.readBuffer(buffer, elemCount); }, value);
	}
	void writeBuffer(ZBuffer buffer, add_cref<std::vector<float>> data) const {
		return std::visit([&](const auto& x) { x.writeBuffer(buffer, data); }, value);
	}
	std::pair<std::string, std::string> getSpirvNames() const
	{
		return makeSpirvNames(std::visit([&](const auto& x) { return x.component; }, value));
	}
	std::pair<std::string, std::string> getGlslNames() const
	{
		return makeGlslNames(std::visit([&](const auto& x) { return x.component; }, value));
	}
	static std::pair<std::string, std::string> makeGlslNames(VkComponentTypeKHR type)
	{
		std::pair<std::string, std::string> names;
		switch (type)
		{
		case VK_COMPONENT_TYPE_FLOAT16_KHR:
			names.first = "float16_t";
			break;
		case VK_COMPONENT_TYPE_BFLOAT16_KHR:
			names.first = "bfloat16_t";
			break;
		case VK_COMPONENT_TYPE_FLOAT32_KHR:
			names.first = "float32_t";
			break;
		case VK_COMPONENT_TYPE_FLOAT64_KHR:
			names.first = "float64_t";
			break;
		case VK_COMPONENT_TYPE_SINT8_KHR:
			names.first = "int8_t";
			break;
		case VK_COMPONENT_TYPE_SINT16_KHR:
			names.first = "int16_t";
			break;
		case VK_COMPONENT_TYPE_SINT32_KHR:
			names.first = "int32_t";
			break;
		case VK_COMPONENT_TYPE_SINT64_KHR:
			names.first = "int64_t";
			break;
		case VK_COMPONENT_TYPE_UINT8_KHR:
			names.first = "uint8_t";
			break;
		case VK_COMPONENT_TYPE_UINT16_KHR:
			names.first = "uint16_t";
			break;
		case VK_COMPONENT_TYPE_UINT32_KHR:
			names.first = "uint32_t";
			break;
		case VK_COMPONENT_TYPE_UINT64_KHR:
			names.first = "uint64_t";
			break;
		default: break;
		}
		return names;
	}
	static std::pair<std::string, std::string> makeSpirvNames(VkComponentTypeKHR type)
	{
		std::pair<std::string, std::string> names;
		switch (type)
		{
		case VK_COMPONENT_TYPE_FLOAT_E4M3_NV:
			names.first = "%e4m3";
			names.second = "OpTypeFloat 8 Float8E4M3EXT";
			break;
		case VK_COMPONENT_TYPE_FLOAT_E5M2_NV:
			names.first = "%e5m2";
			names.second = "OpTypeFloat 8 Float8E5M2EXT";
			break;
		case VK_COMPONENT_TYPE_BFLOAT16_KHR:
			names.first = "%brainfloat";
			names.second = "OpTypeFloat 16 BFloat16KHR";
			break;
		case VK_COMPONENT_TYPE_FLOAT16_KHR:
			names.first = "%half";
			names.second = "OpTypeFloat 16";
			break;
		case VK_COMPONENT_TYPE_FLOAT32_KHR:
			names.first = "%float";
			names.second = "OpTypeFloat 32";
			break;
		case VK_COMPONENT_TYPE_FLOAT64_KHR:
			names.first = "%double";
			names.second = "OpTypeFloat 64";
			break;
		case VK_COMPONENT_TYPE_SINT8_KHR:
			names.first = "%char";
			names.second = "OpTypeInt 8 1";
			break;
		case VK_COMPONENT_TYPE_SINT16_KHR:
			names.first = "%short";
			names.second = "OpTypeInt 16 1";
			break;
		case VK_COMPONENT_TYPE_SINT32_KHR:
			names.first = "%int";
			names.second = "OpTypeInt 32 1";
			break;
		case VK_COMPONENT_TYPE_SINT64_KHR:
			names.first = "%long";
			names.second = "OpTypeInt 64 1";
			break;
		case VK_COMPONENT_TYPE_UINT8_KHR:
			names.first = "%uchar";
			names.second = "OpTypeInt 8 0";
			break;
		case VK_COMPONENT_TYPE_UINT16_KHR:
			names.first = "%ushort";
			names.second = "OpTypeInt 16 0";
			break;
		case VK_COMPONENT_TYPE_UINT32_KHR:
			names.first = "%uint";
			names.second = "OpTypeInt 32 0";
			break;
		case VK_COMPONENT_TYPE_UINT64_KHR:
			names.first = "%ulong";
			names.second = "OpTypeInt 64 0";
			break;
		default:
			break;
		}
		return names;
	}
	std::vector<std::string> getSpirvCapabilities() const
	{
		return makeSpirvCapabilities(std::visit([&](const auto& x) { return x.component; }, value));
	}
	static std::vector<std::string> makeSpirvCapabilities(VkComponentTypeKHR type)
	{
		std::vector<std::string> caps;

		switch (type)
		{
		case VK_COMPONENT_TYPE_UINT8_KHR:
		case VK_COMPONENT_TYPE_SINT8_KHR:
			caps.push_back("Int8");
			caps.push_back("StorageBuffer8BitAccess");
			break;
		case VK_COMPONENT_TYPE_UINT16_KHR:
		case VK_COMPONENT_TYPE_SINT16_KHR:
			caps.push_back("Int16");
			caps.push_back("StorageBuffer16BitAccess");
			break;
		case VK_COMPONENT_TYPE_UINT64_KHR:
		case VK_COMPONENT_TYPE_SINT64_KHR:
			caps.push_back("Int64");
			break;
		case VK_COMPONENT_TYPE_FLOAT_E4M3_NV:
		case VK_COMPONENT_TYPE_FLOAT_E5M2_NV:
			caps.push_back("Float8EXT");
			caps.push_back("StorageBuffer8BitAccess");
			caps.push_back("Float8CooperativeMatrixEXT");
			break;
		case VK_COMPONENT_TYPE_BFLOAT16_KHR:
			caps.push_back("BFloat16TypeKHR");
			caps.push_back("StorageBuffer16BitAccess");
			caps.push_back("BFloat16CooperativeMatrixKHR");
			break;
		case VK_COMPONENT_TYPE_FLOAT16_KHR:
			caps.push_back("Float16");
			caps.push_back("StorageBuffer16BitAccess");
		default:
			break;
		}
		return caps;
	}
	std::vector<std::string> getSpirvExtensions() const
	{
		return makeSpirvExtensions(std::visit([&](const auto& x) { return x.component; }, value));
	}
	static std::vector<std::string> makeSpirvExtensions(VkComponentTypeKHR type)
	{
		std::vector<std::string> exts;
		switch (type)
		{
		case VK_COMPONENT_TYPE_BFLOAT16_KHR:
			exts.push_back("SPV_KHR_bfloat16");
			exts.push_back("SPV_KHR_16bit_storage");
			break;
		case VK_COMPONENT_TYPE_FLOAT16_KHR:
			exts.push_back("SPV_KHR_16bit_storage");
			break;
		case VK_COMPONENT_TYPE_SINT16_KHR:
		case VK_COMPONENT_TYPE_UINT16_KHR:
			exts.push_back("SPV_KHR_16bit_storage");
			break;
		case VK_COMPONENT_TYPE_FLOAT_E4M3_NV:
		case VK_COMPONENT_TYPE_FLOAT_E5M2_NV:
			exts.push_back("SPV_EXT_float8");
			exts.push_back("SPV_KHR_8bit_storage");
			break;
		case VK_COMPONENT_TYPE_SINT8_KHR:
		case VK_COMPONENT_TYPE_UINT8_KHR:
			exts.push_back("SPV_KHR_8bit_storage");
			break;
		default:
			break;
		}
		return exts;
	}
	std::string getMatrixOperand(MatrixTargets m)
	{
		return makeMatrixOperand(std::visit([&](const auto& x) { return x.component; }, value), m);
	}
	static std::string makeMatrixOperand(VkComponentTypeKHR type, MatrixTargets m)
	{
		bool hasSign = false;

		switch (type)
		{
		case VK_COMPONENT_TYPE_FLOAT16_KHR:
		case VK_COMPONENT_TYPE_FLOAT32_KHR:
		case VK_COMPONENT_TYPE_FLOAT64_KHR:
		case VK_COMPONENT_TYPE_SINT8_KHR:
		case VK_COMPONENT_TYPE_SINT16_KHR:
		case VK_COMPONENT_TYPE_SINT32_KHR:
		case VK_COMPONENT_TYPE_SINT64_KHR:
		case VK_COMPONENT_TYPE_BFLOAT16_KHR:
		case VK_COMPONENT_TYPE_SINT8_PACKED_NV:
		case VK_COMPONENT_TYPE_FLOAT_E4M3_NV:
		case VK_COMPONENT_TYPE_FLOAT_E5M2_NV:
			hasSign = true;
			break;

		case VK_COMPONENT_TYPE_UINT8_KHR:
		case VK_COMPONENT_TYPE_UINT16_KHR:
		case VK_COMPONENT_TYPE_UINT32_KHR:
		case VK_COMPONENT_TYPE_UINT64_KHR:
		case VK_COMPONENT_TYPE_UINT8_PACKED_NV:
			hasSign = false;
			break;

		default: ASSERTFALSE("Unhandled type ", VkComponentTypeToString(type));
		}

		if (false == hasSign)
		{
			return std::string();
		}

		static const std::map<MatrixTargets, std::string> names{
			{MatrixTargets::A, "A"},
			{MatrixTargets::B, "B"},
			{MatrixTargets::C, "C"},
			{MatrixTargets::R, "Result"}
		};
		ASSERTMSG(mapHasKey(m, names), "Unknown matrix target");

		return "Matrix" + names.at(m) + "SignedComponentsKHR";
	}
};

std::vector<float> mulMatrices(
	add_cref<std::vector<float>> A,
	add_cref<std::vector<float>> B,
	const uint32_t rowCountOfA,
	const uint32_t colCountOfB)
{
	std::vector<float> R(rowCountOfA * colCountOfB);
	const uint32_t sharedDim = data_count(A) / rowCountOfA;
	const uint32_t rowCountOfB = data_count(B) / colCountOfB;
	ASSERTION(sharedDim == rowCountOfB);

	for (uint32_t row = 0; row < rowCountOfA; ++row) {
		for (uint32_t col = 0; col < colCountOfB; ++col) {
			float sum = 0.0f;
			for (uint32_t k = 0; k < sharedDim; ++k) {
				float a = A[row * sharedDim + k];
				float b = B[k * colCountOfB + col];
				sum += a * b;
			}
			R[row * colCountOfB + col] = sum;
		}
	}
	return R;
}

std::vector<float> addMatrices(
	add_cref<std::vector<float>> A,
	add_cref<std::vector<float>> B)
{
	const uint32_t N = data_count(A);
	ASSERTION(N == B.size());
	std::vector<float> R(N);
	for (uint32_t i = 0u; i < N; ++i)
		R[i] = A[i] + B[i];
	return R;
}

} // unnamed namespace

void populateData(ZBuffer buffer, VkComponentTypeKHR type, uint32_t size, MatrixTargets matrix)
{
	Value val(type);
	if (MatrixTargets::B == matrix)
	{
		const std::vector<float> data(size, 1.0f);
		val.writeBuffer(buffer, data);
	}
	else
	{
		ValueGenerator gen(type);
		std::vector<float> data(size);
		for (uint32_t i = 0; i < size; ++i)
			data[i] = gen.next();
		val.writeBuffer(buffer, data);
	}
}

void readResults(
	ZBuffer a_buffer, VkComponentTypeKHR a_type, add_ref<std::vector<float>> A, uint32_t ASize,
	ZBuffer b_buffer, VkComponentTypeKHR b_type, add_ref<std::vector<float>> B, uint32_t BSize,
	ZBuffer c_buffer, VkComponentTypeKHR c_type, add_ref<std::vector<float>> C, uint32_t CSize,
	ZBuffer r_buffer, VkComponentTypeKHR r_type, add_ref<std::vector<float>> R, uint32_t RSize)
{
	{
		Value a(a_type);
		A = a.readBuffer(a_buffer, ASize);
	}
	{
		Value b(b_type);
		B = b.readBuffer(b_buffer, BSize);
	}
	{
		Value c(c_type);
		C = c.readBuffer(c_buffer, CSize);
	}
	{
		Value r(r_type);
		R = r.readBuffer(r_buffer, RSize);
	}
}

bool verifyResults(
	MatrixTargets mtx,
	add_cref<VkCooperativeMatrixPropertiesKHR> conf,
	add_cref<std::vector<float>> A,
	add_cref<std::vector<float>> B,
	add_cref<std::vector<float>> C,
	add_cref<std::vector<float>> R)
{
	uint32_t mismatch = 0;
	auto cmp = [&](add_cref<std::vector<float>> reference, add_cref<std::vector<float>> result) -> void
	{
		const uint32_t N = data_count(reference);
		ASSERTION(N == result.size());
		for (uint32_t i = 0u; i < N; ++i)
		{
			const float x = reference[i];
			const float y = result[i];
			const bool ok = x == y;
			if (!ok) ++mismatch;
		}
	};

	auto isZero = [](add_cref<std::vector<float>> mat) -> bool
	{
		return std::all_of(mat.begin(), mat.end(), [](float x) { return x == 0.0f; });
	};

	if (mtx == MatrixTargets::A || mtx == MatrixTargets::B)
	{
		cmp(C, R);
		if (mtx == MatrixTargets::A)
		{
			if (!(isZero(A)))
				std::cout << "Warning: Matrix A is not zero" << std::endl;
			if (isZero(B))
				std::cout << "Warning: Matrix B is zero" << std::endl;
		}
		else
		{
			if (isZero(A))
				std::cout << "Warning: Matrix A is zero" << std::endl;
			if (!(isZero(B)))
				std::cout << "Warning: Matrix B is not zero" << std::endl;
		}
	}
	else if (mtx == MatrixTargets::C)
	{
		const std::vector<float> ref = mulMatrices(A, B, conf.MSize, conf.NSize);
		cmp(ref, R);
		if (!(isZero(C)))
			std::cout << "Warning: Matrix C is not zero" << std::endl;
	}
	else if (mtx == MatrixTargets::R)
	{
		const std::vector<float> ref(R.size(), 0.0f);
		cmp(ref, R);
		if (!(isZero(R)))
			std::cout << "Warning: Matrix R is not zero" << std::endl;
	}
	else
	{
		const std::vector<float> ref = addMatrices(mulMatrices(A, B, conf.MSize, conf.NSize), C);
		cmp(ref, R);
	}
	return 0u == mismatch;
}

MatrixTargets VariantToMatrix(uint32_t var)
{
	MatrixTargets mt[]{ MatrixTargets::ALL,
		MatrixTargets::A, MatrixTargets::B, MatrixTargets::C, MatrixTargets::R };
	return mt[var % ARRAY_LENGTH(mt)];
}

void printParams(add_cref<CoopParams> params, add_ref<std::ostream> str, bool printConfiguration)
{
	const VkCooperativeMatrixPropertiesKHR& conf =
		params.confs.at(params.Configuration >= params.confs.size() ? 0u : params.Configuration);

	str << "-----------------" << std::endl;
	str << "Configuration: " << params.Configuration << std::endl;
	if (printConfiguration)
	{
		str << "AType: " << VkComponentTypeToString(conf.AType) << ", "
			<< "BType: " << VkComponentTypeToString(conf.BType) << std::endl;
		str << "CType: " << VkComponentTypeToString(conf.CType) << ", "
			<< "RType: " << VkComponentTypeToString(conf.ResultType) << std::endl;
		str << "MSize: " << conf.MSize << std::endl
			<< "KSize: " << conf.KSize << std::endl
			<< "NSize: " << conf.NSize << std::endl;
	}
	str << "buildAlways: " << std::boolalpha << params.buildAlways << std::noboolalpha << std::endl;
	str << "useSpirvShader: " << std::boolalpha << params.useSpirvShader << std::noboolalpha << std::endl;
	str	<< "Variant: " << params.Variant << ", ";
	if (params.Variant >= 1u && params.Variant <= 4)
	{
		str << "matrix ";
		switch (params.Variant)
		{
		case 1u: str << 'A'; break;
		case 2u: str << 'B'; break;
		case 3u: str << 'C'; break;
		case 4u: str << 'D'; break;
		}
	}
	else str << "all matrices" << std::endl;
}

ZShaderModule buildGlslProgram(ZDevice device, add_cref<CoopParams> params)
{
	auto path = fs::path(params.assets) / "template.glsl";
	const std::string shaderTemplate = readFile(path.string());

	add_cref<VkCooperativeMatrixPropertiesKHR> conf = params.getSelectedConfiguration();
	const std::string TYPE_A = Value(conf.AType).getGlslNames().first;
	const std::string TYPE_B = Value(conf.BType).getGlslNames().first;
	const std::string TYPE_C = Value(conf.CType).getGlslNames().first;
	const std::string TYPE_R = Value(conf.ResultType).getGlslNames().first;
	const string_to_string_map variables
	{
		{"TYPE_A", TYPE_A}, {"TYPE_B", TYPE_B}, {"TYPE_C", TYPE_C}, {"TYPE_R", TYPE_R},
	};

	const std::string shaderCode = subst_variables(shaderTemplate, variables);

	add_cref<GlobalAppFlags> gf = getGlobalAppFlags();
	ProgramCollection programs(device, params.assets);
	programs.addFromText(VK_SHADER_STAGE_COMPUTE_BIT, shaderCode);
	programs.buildAndVerify(Version(1, 3), Version(1, 3),
		gf.spirvValidate, gf.genSpirvDisassembly, params.buildAlways);
	return programs.getShader(VK_SHADER_STAGE_COMPUTE_BIT);
}

ZShaderModule buildSpirvProgram(ZDevice device, add_cref<CoopParams> params)
{
	auto path = fs::path(params.assets) / "template.spvasm";
	const std::string shaderTemplate = readFile(path.string());

	add_cref<VkCooperativeMatrixPropertiesKHR> conf = params.getSelectedConfiguration();

	std::string AType, BType, CRType;
	std::set<std::string> capabilityList;
	std::set<std::string> extensionList;

	std::ostringstream typeList;
	std::vector<VkComponentTypeKHR> types{ VK_COMPONENT_TYPE_UINT32_KHR, VK_COMPONENT_TYPE_SINT32_KHR };
	for (const VkComponentTypeKHR matType : {conf.AType, conf.BType, conf.CType, conf.ResultType})
	{
		if (auto typePtr = std::find(types.begin(), types.end(), matType); typePtr == types.end())
		{
			types.push_back(matType);
			const Value v(matType);
			const auto [typeName, typeDef] = v.getSpirvNames();
			typeList << typeName << " = " << typeDef << " ; generated" << std::endl;

			const std::vector<std::string> caps = v.getSpirvCapabilities();
			for (const std::string& cap : caps)
				capabilityList.insert(cap);

			const std::vector<std::string> exts = v.getSpirvExtensions();
			for (const std::string& ext : exts)
				extensionList.insert(ext);
		}
	}

	const VkComponentTypeKHR matList[]{ conf.AType, conf.BType, conf.CType };
	std::string* matTypes[]{ &AType, &BType, &CRType };

	for (uint32_t i = 0u; i < 3u; ++i)
	{
		const Value v(matList[i]);
		const auto [typeName, typeDef] = v.getSpirvNames();
		matTypes[i]->assign(typeName);
	}

	std::ostringstream capabilities;
	for (const std::string& cap : capabilityList)
		capabilities << "OpCapability " << cap << " ; generated" << std::endl;
	capabilities.flush();

	std::ostringstream extensions;
	for (const std::string& ext : extensionList)
		extensions << "OpExtension \"" << ext << "\" ; generated" << std::endl;
	extensions.flush();

	std::ostringstream operands;
	for (add_cref<std::pair<VkComponentTypeKHR, MatrixTargets>> matType : {
		    std::make_pair(conf.AType, MatrixTargets::A), 
			std::make_pair(conf.BType, MatrixTargets::B),
			std::make_pair(conf.CType, MatrixTargets::C),
			std::make_pair(conf.ResultType, MatrixTargets::R) })
	{
		const std::string op = Value(matType.first).getMatrixOperand(matType.second);
		if (!op.empty())
		{
			if (operands.tellp())
				operands << '|';
			operands << op;
		}
	}

	const std::map<std::string, std::string> variables{
		{"TypeList", typeList.str()},
		{"AStride", std::to_string(Value(conf.AType).size())},
		{"BStride", std::to_string(Value(conf.BType).size())},
		{"CStride", std::to_string(Value(conf.CType).size())},
		{"RStride", std::to_string(Value(conf.ResultType).size())},
		{"AType", AType},
		{"BType", BType},
		{"CRType", CRType},
		{"Capabilities", capabilities.str()},
		{"Extensions", extensions.str()},
		{"Operands", operands.str()}
	};

	const std::string shaderCode = subst_variables(shaderTemplate, variables);

	add_cref<GlobalAppFlags> gf = getGlobalAppFlags();
	ProgramCollection programs(device, params.assets);
	programs.addFromText(VK_SHADER_STAGE_COMPUTE_BIT, shaderCode);
	programs.buildAndVerify(Version(1, 3), Version(1, 3),
		gf.spirvValidate, gf.genSpirvDisassembly, params.buildAlways);
	return programs.getShader(VK_SHADER_STAGE_COMPUTE_BIT);
}

TriLogicInt performTests(add_ref<VulkanContext> ctx, add_cref<CoopParams> params)
{
	printParams(params, std::cout, true);

	add_cref<VkCooperativeMatrixPropertiesKHR> conf = params.getSelectedConfiguration();

	const uint32_t MSize = conf.MSize;
	const uint32_t KSize = conf.KSize;
	const uint32_t NSize = conf.NSize;
	const uint32_t ASize = MSize * KSize;
	const uint32_t BSize = KSize * NSize;
	const uint32_t CRSize = MSize * NSize;
	const VkComponentTypeKHR a_comp(conf.AType);
	const VkComponentTypeKHR b_comp(conf.BType);
	const VkComponentTypeKHR c_comp(conf.CType);
	const VkComponentTypeKHR r_comp(conf.ResultType);

	const MatrixTargets testMatrix = VariantToMatrix(params.Variant);
	struct PC {
		uint32_t V, MAT_A, MAT_B, MAT_C, MAT_R;
	} const pc {
		uint32_t(testMatrix),
		uint32_t(MatrixTargets::A),
		uint32_t(MatrixTargets::B),
		uint32_t(MatrixTargets::C),
		uint32_t(MatrixTargets::R)
	};
	ZPushRange<PC>	pushRange(VK_SHADER_STAGE_COMPUTE_BIT);

	ZSpecializationInfo		specInfo;
	specInfo.addEntry<uint32_t>(params.subgroupSize); // local_size_x_id
	specInfo.addEntry<uint32_t>(MSize); // MxK * KxN -> MxN
	specInfo.addEntry<uint32_t>(KSize);
	specInfo.addEntry<uint32_t>(NSize);

	LayoutManager			lm(ctx.device);
	ZShaderModule			shader = params.useSpirvShader 
								? buildSpirvProgram(ctx.device, params)
								: buildGlslProgram(ctx.device, params);
	uint32_t	a_null = 0, a_binding = lm.addBindingAsVector<uint8_t>(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
											Value(a_comp).size() * ASize, VK_SHADER_STAGE_COMPUTE_BIT);
	uint32_t	b_null = 0, b_binding = lm.addBindingAsVector<uint8_t>(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
											Value(b_comp).size() * BSize, VK_SHADER_STAGE_COMPUTE_BIT);
	uint32_t	c_null = 0, c_binding = lm.addBindingAsVector<uint8_t>(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
											Value(c_comp).size() * CRSize, VK_SHADER_STAGE_COMPUTE_BIT);
	uint32_t	r_null = 0, r_binding = lm.addBindingAsVector<uint8_t>(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
											Value(r_comp).size() * CRSize, VK_SHADER_STAGE_COMPUTE_BIT);
	if (false == params.useSpirvShader)
	{
		a_null = lm.addBindingAsVector<uint8_t>(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
					Value(a_comp).size() * ASize, VK_SHADER_STAGE_COMPUTE_BIT);
		b_null = lm.addBindingAsVector<uint8_t>(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
					Value(b_comp).size() * BSize, VK_SHADER_STAGE_COMPUTE_BIT);
		c_null = lm.addBindingAsVector<uint8_t>(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
					Value(c_comp).size() * CRSize, VK_SHADER_STAGE_COMPUTE_BIT);
		r_null = lm.addBindingAsVector<uint8_t>(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
					Value(r_comp).size() * CRSize, VK_SHADER_STAGE_COMPUTE_BIT);
	}

	ZDescriptorSetLayout	dsLayout = lm.createDescriptorSetLayout();
	ZPipelineLayout			pipeLayout = lm.createPipelineLayout({ dsLayout }, pushRange);
	ZPipeline				pipeline = createComputePipeline(pipeLayout, shader, specInfo);
	ZBuffer					a_buffer = std::get<DescriptorBufferInfo>(lm.getDescriptorInfo(a_binding)).buffer;
	ZBuffer					b_buffer = std::get<DescriptorBufferInfo>(lm.getDescriptorInfo(b_binding)).buffer;
	ZBuffer					c_buffer = std::get<DescriptorBufferInfo>(lm.getDescriptorInfo(c_binding)).buffer;
	ZBuffer					r_buffer = std::get<DescriptorBufferInfo>(lm.getDescriptorInfo(r_binding)).buffer;

	populateData(a_buffer, a_comp, ASize, MatrixTargets::A);
	populateData(b_buffer, b_comp, BSize, MatrixTargets::B);
	populateData(c_buffer, c_comp, CRSize, MatrixTargets::C);
	populateData(r_buffer, r_comp, CRSize, MatrixTargets::R);
	if (false == params.useSpirvShader)
	{
		lm.fillBinding(a_null);
		lm.fillBinding(b_null);
		lm.fillBinding(c_null);
		lm.fillBinding(r_null);
	}

	{
		OneShotCommandBuffer cmd(ctx.device, ctx.computeQueue);
		commandBufferBindPipeline(cmd, pipeline);
		commandBufferPushConstants(cmd, pipeLayout, pc);
		commandBufferDispatch(cmd, UVec3(3, 1, 1));
	}


	std::vector<float> A, B, C, R;
	readResults(a_buffer, a_comp, A, ASize,
				b_buffer, b_comp, B, BSize,
				c_buffer, c_comp, C, CRSize,
				r_buffer, r_comp, R, CRSize);

	const bool result = verifyResults(testMatrix, conf, A, B, C, R);
	if (params.allConfigurations)
	{
		std::cout << "Result: " << (result ? "PASS" : "FAIL") << std::endl << std::endl;
	}

	return result ? 0 : 1;
}

} // namespace coopmat

template<> struct TestRecorder<COOPERATIVE_MATRIX>
{
	static bool record(TestRecord&);
};
bool TestRecorder<COOPERATIVE_MATRIX>::record(TestRecord& record)
{
	record.name = "cooperative_matrix";
	record.call = &coopmat::prepareTests;
	return true;
}

