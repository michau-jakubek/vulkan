#include "vtfExtensions.hpp"
#include "vtfVkUtils.hpp"
#include "vtfZUtils.hpp"

namespace
{

struct FeatureVisitor
{
    enum Mode
    {
        Searching,
        Chaining,
        Iterate
    };

    using FeatureInfo = vtf::DeviceCaps::FeatureInfo;
    using Comparer = std::function<bool(VkStructureType, VkStructureType, uint32_t, add_cptr<void>, bool)>;
    Comparer                            m_doContinue;
    add_cref<vtf::DeviceCaps::Features> m_features;
    const Mode                          m_mode;
    VkStructureType                     m_breakType;
    add_ref<bool>                       m_continueFlag;
    add_cref<uint32_t>                  m_featureIndex;
    add_ref<FeatureInfo>                m_featureInfo;
    vtf::DeviceCaps::Features           m_chain;

    static bool isF20 (const VkStructureType sType)
    {
        return sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    }

    template <class FeatureStructure>
    void operator ()(const FeatureStructure& feature)
    {
        const VkStructureType sType = vtf::mkstype<FeatureStructure>;
        const bool hasPnext = vtf::hasPnextOfVoidPtr<FeatureStructure>::value;

        if (m_mode == Mode::Chaining)
        {
            m_continueFlag = true;
            add_ref<vtf::FeaturesVar> var = m_chain.emplace_back(feature);
            add_ptr<FeatureStructure> pFeature = std::get_if<FeatureStructure>(&var);

            if constexpr (hasPnext)
            {
                ASSERTION(sType == pFeature->sType);
                pFeature->pNext = m_featureInfo.address;
                m_featureInfo.address = pFeature;
            }
        }
        else if (m_mode == Mode::Searching)
        {
            if (m_continueFlag = (m_breakType != sType); (false == m_continueFlag))
            {
                m_featureInfo.address = &vtf::remove_const(feature);
                m_featureInfo.size = uint32_t(sizeof(feature));
                m_featureInfo.index = m_featureIndex;
                m_featureInfo.type = sType;
            }
        }
        else
        {
            m_continueFlag = m_doContinue ? m_doContinue(m_breakType, sType, m_featureIndex, &feature, hasPnext) : true;
        }
    }

    FeatureVisitor (Comparer doContinue, add_cref< vtf::DeviceCaps::Features> features, Mode mode,
                    VkStructureType breakType, add_cref<uint32_t> featureIndex, add_ref<bool> continueFlag,
                    add_ref<vtf::DeviceCaps::FeatureInfo> featureInfo)
        : m_doContinue          (doContinue)
        , m_features            (features)
        , m_mode                (mode)
        , m_breakType           (breakType)
        , m_continueFlag        (continueFlag)
        , m_featureIndex        (featureIndex)
        , m_featureInfo         (featureInfo)
        , m_chain               ()
    {
        featureInfo.reset();
        if (Mode::Chaining == m_mode)
        {
            m_chain.reserve(features.size());
        }
    }
};

vtf::DeviceCaps::Features traverseFeatures (
    const FeatureVisitor::Mode              mode,
    add_cref<vtf::DeviceCaps::Features>     features,
    VkStructureType                         breakType, // sType to searching for, it is first parameter of doContinue
    add_ref<FeatureVisitor::FeatureInfo>    featureInfo,
    FeatureVisitor::Comparer                doContinue = nullptr) // if lhs == rhs return false to break looking for
{
    bool continueFlag = false;
    uint32_t featureIndex = 0;

    FeatureVisitor visitor(doContinue, features, mode, breakType, featureIndex, continueFlag, featureInfo);

    for (auto begin = features.cbegin(), var = begin; var != features.cend(); ++var)
    {
        featureIndex = static_cast<uint32_t>(std::distance(begin, var));
        std::visit(visitor, *var);
        if (false == continueFlag)
            break;
    }

    if (mode == FeatureVisitor::Mode::Chaining)
    {
        return std::move(visitor.m_chain);
    }

    return {};
}

} // unnamed namespace

namespace vtf
{

DeviceCaps::FeatureInfo_::FeatureInfo_()
{
    reset();
}

void DeviceCaps::FeatureInfo_::reset ()
{
    size = 0u;
    address = nullptr;
    index = vtf::INVALID_UINT32;
    type = VK_STRUCTURE_TYPE_MAX_ENUM;
}

DeviceCaps::DeviceCaps (add_cref<strings> extensions, ZPhysicalDevice device)
    : m_features()
    , availableExtensions(extensions)
    , physicalDevice(device)
{
}

VkBool32 DeviceCaps::checkNotSupportedFeature (
    add_cref<std::string> structName,
    add_cptr<char> fieldNameAndDescription,
    VkBool32 test, bool raiseIfFalse) const
{
    if (VK_FALSE == test && raiseIfFalse)
    {
        ASSERTFALSE(structName, "::",
            (fieldNameAndDescription ? fieldNameAndDescription : ""));
    }
    return test;
}

void DeviceCaps::initializeFeature (add_ptr<void> feature, bool autoInitialize, bool isVkPhysicalDeviceFeatures10)
{
    if (false == autoInitialize) return;

    if (isVkPhysicalDeviceFeatures10)
    {
        const VkPhysicalDeviceFeatures2 f = deviceGetPhysicalFeatures2(physicalDevice);
        *reinterpret_cast<add_ptr<VkPhysicalDeviceFeatures>>(feature) = f.features;
    }
    else
    {
        deviceGetPhysicalFeatures2(physicalDevice, feature);
    }
}

void_cptr DeviceCaps::addUpdateFeature (VkStructureType sType, add_ptr<void> pExpected, add_cptr<void> pSource,
                                   uint32_t featureSize, add_ref<Caller> caller)
{
    const bool isVkPhysicalDeviceFeatures10 = (VK_STRUCTURE_TYPE_MAX_ENUM == sType);

    if (pExpected)
    {
        initializeFeature(pExpected, true, isVkPhysicalDeviceFeatures10);

        if (caller.compareExchange(pExpected, nullptr))
        {
            const FeatureInfo fi = getFeatureInfo(sType, m_features);
            if (fi.size)
            {
                ASSERTION(fi.size == featureSize);
                caller.compareExchange(pExpected, fi.address);
                return fi.address;
            }
            else
            {
                add_ptr<void> pNewFeature = caller.addFeature();
                ASSERTION(pNewFeature); // Should never happen
                caller.compareExchange(pExpected, pNewFeature);
                return pNewFeature;
            }
        }
    }
    else
    {
        const FeatureInfo fi = getFeatureInfo(sType, m_features);
        if (fi.size)
        {
            UNREF(featureSize);
            ASSERTION(fi.size == featureSize);
            if (nullptr == pSource)
                initializeFeature(fi.address, true, isVkPhysicalDeviceFeatures10);
            return fi.address;
        }
        else
        {
            void* pNewFeature = caller.addFeature();
            ASSERTION(pNewFeature); // Should never happen
            if (nullptr == pSource)
                initializeFeature(pNewFeature, true, isVkPhysicalDeviceFeatures10);
            return pNewFeature;
        }
    }

    return nullptr;
}

add_ref<FeaturesVar> DeviceCaps::assertAlreadyExists (VkStructureType sType, FeaturesVar&& var)
{
    FeatureInfo info;
    traverseFeatures(FeatureVisitor::Searching, m_features, sType, info);
    ASSERTMSG((nullptr == info.address), "Feature already exists");
    if (FeatureVisitor::isF20(sType))
        return m_features.emplace_back(var);
    return *m_features.insert(m_features.begin(), var);
}

bool DeviceCaps::removeFeature (VkStructureType sType)
{
    FeatureInfo info;
    traverseFeatures(FeatureVisitor::Mode::Searching, m_features, sType, info);
    if (INVALID_UINT32 != info.index && nullptr != info.address)
    {
        m_features.erase(std::next(m_features.begin(), info.index));
        return true;
    }
    return false;
}

bool DeviceCaps::getFeature (VkStructureType sType, add_ptr<void> feature)
{
    FeatureInfo info;
    traverseFeatures(FeatureVisitor::Mode::Searching, m_features, sType, info);
    if (info.size && info.address)
    {
        auto srcRange = makeStdBeginEnd<char>(info.address, info.size);
        auto dstRange = makeStdBeginEnd<char>(feature, info.size);
        std::copy(srcRange.first, srcRange.second, dstRange.first);
        return true;
    }
    return false;
}

bool DeviceCaps::replaceFeature (VkStructureType sType, add_cptr<void> feature)
{
    FeatureInfo info;
    traverseFeatures(FeatureVisitor::Mode::Searching, m_features, sType, info);
    if (info.size && info.address)
    {
        auto srcRange = makeStdBeginEnd<char>(feature, info.size);
        auto dstRange = makeStdBeginEnd<char>(info.address, info.size);
        std::copy(srcRange.first, srcRange.second, dstRange.first);
        return true;
    }
    return false;
}

DeviceCaps::FeatureInfo_ DeviceCaps::getFeatureInfo (VkStructureType sType, add_cref<Features> others) const
{
    FeatureInfo info;
    traverseFeatures(FeatureVisitor::Mode::Searching, others, sType, info);
    return info;
}

bool DeviceCaps::hasPhysicalDeviceFeatures10 () const
{
    const VkStructureType sType = mkstype<VkPhysicalDeviceFeatures>;
    const FeatureInfo info = getFeatureInfo(sType, m_features);
    if (info.address)
    {
        ASSERTION(sType == info.type);
        ASSERTION(INVALID_UINT32 != info.index);
    }
    return info.address;
}

bool DeviceCaps::hasPhysicalDeviceFeatures20 () const
{
    const VkStructureType sType = mkstype<VkPhysicalDeviceFeatures2>;
#ifdef _MSC_VER
#pragma warning(disable: 4127)
#endif
    ASSERTION(FeatureVisitor::isF20(sType));
#ifdef _MSC_VER
#pragma warning(default: 4127)
#endif
    const FeatureInfo info = getFeatureInfo(sType, m_features);
    if (info.address)
    {
        ASSERTION(sType == info.type);
        ASSERTION(sType == reinterpret_cast<add_cptr<VkPhysicalDeviceFeatures2>>(info.address)->sType);
        ASSERTION(INVALID_UINT32 != info.index);
    }
    return info.address;
}

void DeviceCaps::updateDeviceCreateInfo (add_ref<VkDeviceCreateInfo> createInfo,
                                         add_cref<VkPhysicalDeviceFeatures> merge, add_ref<Features> aux) const
{
    int featureCount = static_cast<int>(m_features.size());

    const FeatureInfo info10 = getFeatureInfo(VK_STRUCTURE_TYPE_MAX_ENUM, m_features);
    const FeatureInfo info20 = getFeatureInfo(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, m_features);

    const bool has10 = (nullptr != info10.address);
    const bool has20 = (nullptr != info20.address);

    if (has10) featureCount = featureCount - 1;
    if (has20) featureCount = featureCount - 1;

    if (has10 && has20)
    {
        ASSERTFALSE("Only one extension, F1 or F2 can be added to the list");
    }

    if (has20 && (featureCount <= 0))
    {
        ASSERTFALSE("F2 cannot appear in the list if no other string extensions are defined");
    }

    bool use20 = false;
    DeviceCaps tmp(*this);
    if (false == has20)
    {
        if (featureCount > 0)
        {
            VkPhysicalDeviceFeatures2 f2{};

            if (has10)
            {
                f2.features = *reinterpret_cast<add_cptr<VkPhysicalDeviceFeatures>>(info10.address);
                const uint32_t fc = uint32_t(sizeof(merge) / sizeof(VkBool32));
                auto src = reinterpret_cast<add_cptr<VkBool32>>(&merge);
                auto dst = reinterpret_cast<add_ptr<VkBool32>>(&f2.features);
                for (uint32_t fi = 0u; fi < fc; ++fi)
                {
                    if (src[fi] != VK_FALSE)
                        dst[fi] = src[fi];
                }
                tmp.removeFeature(VK_STRUCTURE_TYPE_MAX_ENUM);
            }
            else
            {
                f2.features = merge;
            }

            tmp.addUpdateFeature(f2);
            use20 = true;
        }
        else if (false == has10)
        {
            tmp.addUpdateFeature(merge);
            use20 = false;
        }
    }
    else if (false == has10)
    {
        use20 = false;
        tmp.addUpdateFeature(merge);
    }

    FeatureInfo tmpInfo20;
    aux = traverseFeatures(FeatureVisitor::Mode::Chaining, tmp.m_features, VkStructureType(0), tmpInfo20);
    FeatureInfo tmpInfo10 = getFeatureInfo(VK_STRUCTURE_TYPE_MAX_ENUM, aux);

    if (use20)
    {
        ASSERTION(tmpInfo20.address);
        ASSERTION(nullptr == tmpInfo10.address);
    }
    else
    {
        ASSERTION(nullptr == tmpInfo20.address);
        ASSERTION(tmpInfo10.address);
    }

    createInfo.pNext = tmpInfo20.address;
    createInfo.pEnabledFeatures = reinterpret_cast<add_ptr<VkPhysicalDeviceFeatures>>(tmpInfo10.address);
}

} // namespace vtf
