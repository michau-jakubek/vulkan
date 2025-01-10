#ifndef __VTF_EXTENSIONS_HPP_INCLUDED__
#define __VTF_EXTENSIONS_HPP_INCLUDED__

#include "vtfStructUtils.hpp"
#include "vtfCUtils.hpp"
#include <type_traits>
#include <variant>
#include <vector>

namespace vtf
{

class DeviceCaps
{
    struct FeatureInfo_
    {
        add_ptr<void>   address;
        uint32_t        index;
        VkStructureType type;
        uint32_t        size;
        FeatureInfo_ ();
        void reset ();
    };

    std::vector<FeaturesVar> m_features;
    VkBool32 checkNotSupportedFeature (add_cref<std::string> structName,
                                       add_cptr<char> fieldNameAndDescription,
                                       VkBool32 test, bool raiseIfFalse) const;
    void initializeFeature (add_ptr<void> feature, bool autoInitialize, bool isVkPhysicalDeviceFeatures10);
    FeatureInfo_ getFeatureInfo (VkStructureType sType, add_cref<std::vector<FeaturesVar>> others) const;
    bool getFeature (VkStructureType sType, add_ptr<void> feature);
    bool replaceFeature (VkStructureType sType, add_cptr<void> feature);

public:
    typedef FeatureInfo_ FeatureInfo;
    add_cref<strings> availableExtensions;
    ZPhysicalDevice physicalDevice;
    strings requiredExtension;

    using Features = std::vector<FeaturesVar>;
    DeviceCaps(add_cref<strings> extensions, ZPhysicalDevice device);

    bool hasPhysicalDeviceFeatures10 () const;
    bool hasPhysicalDeviceFeatures20 () const;
    void updateDeviceCreateInfo (add_ref<VkDeviceCreateInfo> createInfo,
                                 add_cref<VkPhysicalDeviceFeatures> merge, add_ref<Features> aux) const;
    auto assertAlreadyExists (VkStructureType sType, FeaturesVar&& var) -> add_ref<FeaturesVar>;

    bool removeFeature (VkStructureType sType);
    template<typename FeatureStructure>
    bool removeFeature ()
    {
        return removeFeature(mkstype<FeatureStructure>);
    }

    template<typename FeatureStructure>
    bool getFeature (FeatureStructure& feature)
    {
        return getFeature(mkstype<FeatureStructure>, &feature);
    }

    template<typename FeatureStructure>
    bool replaceFeature (const FeatureStructure& feature)
    {
        FeatureStructure str(feature);
        const VkStructureType sType = mkstype<FeatureStructure>;
        if constexpr (hasPnextOfVoidPtr<FeatureStructure>::value)
        {
            str.sType = sType;
            str.pNext = nullptr;
        }
        return replaceFeature(sType, &str);
    }

    template<class FeatureStructure> struct TemporaryStructure
    {
        add_cref<DeviceCaps> owner;
        add_cref<FeatureStructure> structure;
        TemporaryStructure (add_cref<DeviceCaps> caps, add_cref<FeatureStructure> str)
            : owner     (caps)
            , structure (str) {}
        VkBool32 checkNotSupported (VkBool32 FeatureStructure::* pField,
                                    bool raiseIfFalse = true, add_cptr<char> fieldNameAndDescription = {}) const
        {
            return owner.checkNotSupportedFeature(demangledName<FeatureStructure>(),
                                                  fieldNameAndDescription, structure.*pField, raiseIfFalse);
        }
    };

    template<class FeatureStructure>
    auto addFeature(const FeatureStructure& feature, bool autoInitialize = false)
        -> TemporaryStructure<FeatureStructure>
    {
        const VkStructureType sType = mkstype<FeatureStructure>;
        add_ref<FeaturesVar> var = assertAlreadyExists(sType, FeaturesVar(feature));
        add_ref<FeatureStructure> str = std::get<FeatureStructure>(var);
        if constexpr (hasPnextOfVoidPtr<FeatureStructure>::value)
        {
            str.sType = sType;
            str.pNext = nullptr;
            initializeFeature(&str, autoInitialize, false);
        }
        else
        {
            initializeFeature(&str, autoInitialize, true);
        }
        return TemporaryStructure<FeatureStructure>(*this, str);
    }
};

} // namespace vtf

#endif // __VTF_EXTENSIONS_HPP_INCLUDED__
