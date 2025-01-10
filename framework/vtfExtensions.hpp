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
    struct Caller
    {
        virtual bool compareExchange (add_ptr<void> pExpected, add_ptr<void> pDesired) = 0;
        virtual void_ptr addFeature () = 0;
    };
    template <class CompareExchange, class AddFeature>
    struct LambdaCaller : Caller
    {
        AddFeature fnAddFeature;
        CompareExchange fnCompareExchange;
        LambdaCaller (CompareExchange compareExchange, AddFeature addFeature)
            : fnAddFeature      (addFeature)
            , fnCompareExchange (compareExchange)
        {
        }
        virtual bool compareExchange (void_ptr pExpected, void_ptr pDesired) override
        {
            return fnCompareExchange(pExpected, pDesired);
        }
        virtual void_ptr addFeature () override
        {
            return fnAddFeature();
        }
    };

    std::vector<FeaturesVar> m_features;

    template <class FeatureStructure>
    void prepareFeature (FeatureStructure& feature, VkStructureType sType)
    {
        MULTI_UNREF(sType, feature);
        if constexpr (hasPnextOfVoidPtr<FeatureStructure>::value)
        {
            feature.pNext = nullptr;
            feature.sType = sType;
        }
    }

    template <class FeatureStructure, class FieldType>
    void_cptr _addUpdateFeatureSet (add_cptr<FeatureStructure> pSource, FieldType FeatureStructure::* pField,
                               add_cref<FieldType> setToValue, add_cref<FieldType> expectedValue = {},
                               bool anableExpectd = false)
    {
        FeatureStructure expected = FeatureStructure{};
        FeatureStructure desired = pSource ? *pSource : expected;
        const VkStructureType sType = mkstype<FeatureStructure>;

        prepareFeature(desired, sType);
        prepareFeature(expected, sType);

        auto compareExchange = [&](void_ptr pExpected, void_ptr pDesired) -> bool
        {
            if (pField ?
                (anableExpectd ? (expectedValue == reinterpret_cast<add_ptr<FeatureStructure>>(pExpected)->*pField) : true) :
                false)
            {
                if (pDesired)
                {
                    reinterpret_cast<FeatureStructure*>(pDesired)->*pField = setToValue;
                }
                return true;
            }
            return false;
        };

        auto addFeature = [&]() -> void*
        {
            //verifyFeature(sType, true);
            prepareFeature(desired, sType);
            m_features.emplace_back(desired);
            return std::get_if<FeatureStructure>(&m_features.back());
        };

        LambdaCaller caller(compareExchange, addFeature);
        return addUpdateFeature(sType, (pField ? &expected : nullptr), pSource,
                                static_cast<uint32_t>(sizeof(FeatureStructure)), caller);
    }

    void_cptr addUpdateFeature (VkStructureType sType, add_ptr<void> pExpected, add_cptr<void> pSource,
                           uint32_t featureSize, add_ref<Caller> caller);

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
        prepareFeature(str, sType);
        return replaceFeature(sType, &str);
    }

    template<class FeatureStructure> struct TemporaryStructure
    {
        const bool valid;
        add_cref<DeviceCaps> owner;
        add_cptr<FeatureStructure> ptr;
        TemporaryStructure (add_cref<DeviceCaps> caps, add_cptr<FeatureStructure> str)
            : valid (str)
            , owner (caps)
            , ptr   (str) {}
        VkBool32 checkNotSupported (VkBool32 FeatureStructure::* pField,
                                    bool raiseIfFalse = true, add_cptr<char> fieldNameAndDescription = {}) const
        {
            return owner.checkNotSupportedFeature(demangledName<FeatureStructure>(), fieldNameAndDescription,
                                                  (valid ? ptr->*pField : VK_FALSE), raiseIfFalse);
        }
        operator bool() const { return valid; }
    };

    template <class FeatureStructure, class FieldType>
    bool addUpdateFeatureSet (FieldType FeatureStructure::* pField, const std::decay_t<FieldType>& setToValue,
        const std::decay_t<FieldType>& expectedValue = {}, bool enableExpectd = false)
    {
        return _addUpdateFeatureSet<FeatureStruct>(nullptr, pField, setToValue, expectedValue, enableExpectd);
    }

    template <class FeatureStructure>
    auto addUpdateFeatureIf (VkBool32 FeatureStructure::* pField) -> TemporaryStructure<FeatureStructure>
    {
        void_cptr pf = _addUpdateFeatureSet<FeatureStructure>(nullptr, pField, VK_TRUE, VK_TRUE, true);
        return TemporaryStructure<FeatureStructure>(*this, reinterpret_cast<add_cptr<FeatureStructure>>(pf));
    }

    template <class FeatureStructure>
    bool addUpdateFeature (const FeatureStructure& feature)
    {
        return _addUpdateFeatureSet<FeatureStructure, std::nullptr_t>(&feature, nullptr, nullptr);
    }

    template <class FeatureStructure>
    auto addUpdateFeature() -> TemporaryStructure<FeatureStructure>
    {
        void_cptr pf = _addUpdateFeatureSet<FeatureStructure, std::nullptr_t>(nullptr, nullptr, nullptr);
        return TemporaryStructure<FeatureStructure>(*this, reinterpret_cast<add_cptr<FeatureStructure>>(pf));
    }
};

} // namespace vtf

#endif // __VTF_EXTENSIONS_HPP_INCLUDED__
