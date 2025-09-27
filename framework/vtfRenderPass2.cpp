#include "vtfRenderPass2.hpp"
#include "vtfFormatUtils.hpp"
#include "vtfStructUtils.hpp"
#include "vtfZImage.hpp"
#include "vtfBacktrace.hpp"

namespace vtf
{

VkAttachmentLoadOp colorLoadOp(VkFormat format, VkImageLayout initialLayout)
{
    if (formatIsDepthStencil(ZPhysicalDevice(), format).first)
        return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    return VK_IMAGE_LAYOUT_UNDEFINED == initialLayout
        ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
}
VkAttachmentStoreOp colorStoreOp(VkFormat format, VkImageLayout)
{
    if (formatIsDepthStencil(ZPhysicalDevice(), format).first)
        return VK_ATTACHMENT_STORE_OP_DONT_CARE;
    return VK_ATTACHMENT_STORE_OP_STORE;
}
VkAttachmentLoadOp dsLoadOp(VkFormat format, VkImageLayout initialLayout)
{
    if (formatIsDepthStencil(ZPhysicalDevice(), format).first)
        return VK_IMAGE_LAYOUT_UNDEFINED == initialLayout
            ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
    return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
}
VkAttachmentStoreOp dsStoreOp(VkFormat format, VkImageLayout)
{
    if (formatIsDepthStencil(ZPhysicalDevice(), format).first)
        return VK_ATTACHMENT_STORE_OP_STORE;
    return VK_ATTACHMENT_STORE_OP_DONT_CARE;
}
VkImageLayout finaliLayout(VkFormat format, VkImageLayout finalLayout)
{
    if (VK_IMAGE_LAYOUT_MAX_ENUM == finalLayout)
    {
        const auto ds = formatIsDepthStencil(ZPhysicalDevice(), format);
        if (ds.second)
            return VK_IMAGE_LAYOUT_STENCIL_ATTACHMENT_OPTIMAL;
        else if (ds.first)
            return VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
        return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    }
    return finalLayout;
}

RPA::RPA (AttachmentDesc desc, uint32_t otherRpaIndex)
    : VkAttachmentDescription2{}
    , mIndex(otherRpaIndex)
    , mClearValue{}
    , mDescription(desc)
{
}

RPA::RPA (AttachmentDesc desc, VkFormat format, VkClearValue clearValue, VkImageLayout initialLayout, VkImageLayout finalLayout)
    : VkAttachmentDescription2 {
        VkAttachmentDescription2(makeVkStruct()).sType,
        nullptr, // const void*                  pNext;
        0,       // VkAttachmentDescriptionFlags flags;
        format,  // VkFormat                     format;
        VK_SAMPLE_COUNT_1_BIT, // VkSampleCountFlagBits                         samples;
        colorLoadOp(format, initialLayout), // VkAttachmentLoadOp   loadOp;
        colorStoreOp(format, finalLayout),  // VkAttachmentStoreOp  storeOp;
        dsLoadOp(format, initialLayout),    // VkAttachmentLoadOp   stencilLoadOp;
        dsStoreOp(format, finalLayout),     // VkAttachmentStoreOp  stencilStoreOp;
        initialLayout,                      // VkImageLayout        initialLayout;
        finaliLayout(format, finalLayout),  // VkImageLayout        finalLayout;
    }
    , mIndex(INVALID_UINT32)
    , mClearValue(clearValue)
    , mDescription(desc)
{
    ASSERTMSG(AttachmentDesc::Input != desc, "Create input attachment as a reference to existing color/depth/stencil attachment");
}

VkAttachmentDescription2 RPA::operator()() const
{
    return static_cast<add_cref<VkAttachmentDescription2>>(*this);
}

bool RPA::isResourceAttachment (AttachmentDesc desc)
{
    switch (desc)
    {
    case AttachmentDesc::Color:
    case AttachmentDesc::Depth:
    case AttachmentDesc::Stencil:
    case AttachmentDesc::DeptStencil:
        return true;
    default: break;
    }
    return false;
}

add_cptr<char> RPA::descToString (AttachmentDesc desc)
{
    switch (desc)
    {
    case AttachmentDesc::Color:
        return "AttachmentDesc::Color";
    case AttachmentDesc::Depth:
        return "AttachmentDesc::Depth";
    case AttachmentDesc::Stencil:
        return "AttachmentDesc::Stencil";
    case AttachmentDesc::DeptStencil:
        return "AttachmentDesc::DeptStencil";
    case AttachmentDesc::Input:
        return "AttachmentDesc::Input";
    case AttachmentDesc::Resolve:
        return "AttachmentDesc::Resolve";
    case AttachmentDesc::Undefined:
        return "AttachmentDesc::Undefined";
    }
    return nullptr;
}

typedef std::pair<uint32_t, RPA> Item;
typedef std::vector<Item> ItemVec;

void freeAttachmentPool (add_ptr<ZAttachmentPool>) {}
ZAttachmentPool::ZAttachmentPool (add_cref<std::vector<RPA>> list)
{
    [this](add_ptr<add_ptr<ZAttachmentPool>> pp) { *pp = this; }(setter());
    verbose(getGlobalAppFlags().verbose);

    const uint32_t listSize = data_count(list);
    ASSERTMSG(listSize, "Attachments must not be empty");
    setParam<uint32_t>(seed++);

    add_ref<ItemVec> rpas = getParamRef<ItemVec>();
    rpas.resize(listSize, std::make_pair(INVALID_UINT32, RPA(AttachmentDesc::Undefined, INVALID_UINT32)));
    std::transform(list.begin(), list.end(), rpas.begin(),
        [](add_cref<RPA> rpa) { return std::make_pair(INVALID_UINT32, rpa); });

    for (uint32_t i = 0u, realIndex = 0u; i < listSize; ++i)
    {
        add_cref<RPA> rpa = list[i];

        if (RPA::isResourceAttachment(rpa.mDescription))
        {
            uint32_t tries = 0u;
            uint32_t ref = rpa.mIndex;
            add_cptr<RPA> pRef = &rpa;
            for (tries = 0u; INVALID_UINT32 != ref && tries < listSize; ++tries)
            {
                ASSERTMSG(ref < listSize,
                    RPA::descToString(rpa.mDescription), " at index ", rpa.mIndex, " must point to the existing attachment");

                pRef = &list[ref];

                ASSERTMSG(rpa.mDescription == pRef->mDescription,
                    RPA::descToString(rpa.mDescription), " at index ", rpa.mIndex,
                    " type differs from ", RPA::descToString(pRef->mDescription), " at index ", ref);

                ref = list[ref].mIndex;
            }
            ASSERTMSG(tries < listSize, "Circular reference for ",
                RPA::descToString(rpa.mDescription), " at index ", rpa.mIndex);

            rpas[i].second = *pRef;
            rpas[i].second.mIndex = realIndex++;
        }
    }

    for (uint32_t i = 0u; i < listSize; ++i)
    {
        add_cref<RPA> rpa = list[i];

        if (rpa.mDescription == AttachmentDesc::Input)
        {
            ASSERTMSG(rpa.mIndex < listSize,
                "Attachment type: ", RPA::descToString(rpa.mDescription), ", index: ", i,
                " must point to the existing attachment");

            ASSERTMSG(RPA::isResourceAttachment(list[rpa.mIndex].mDescription),
                "Attachment type: ", RPA::descToString(rpa.mDescription), ", index: ", i,
                " may point to color/depth/stencil attachment only");

            rpas[i].second = rpas[rpa.mIndex].second;
            rpas[i].second.mDescription = AttachmentDesc::Input;
        }
    }
}

void ZAttachmentPool::updateDescriptions (add_ref<std::vector<VkAttachmentDescription2>> descs) const
{
    add_cref<ItemVec> src = getParamRef<ItemVec>();
    uint32_t realIndex = 0;
    for (add_cref<Item> rpa : src)
    {
        if (RPA::isResourceAttachment(rpa.second.mDescription))
        {
            ASSERTMSG(rpa.second.mIndex == realIndex, "Mismatch: mIndex = ", rpa.second.mIndex, " realIndex = ", realIndex);
            descs.emplace_back(rpa.second());
            realIndex = realIndex + 1u;
        }
    }
}

uint32_t ZAttachmentPool::updateClearValues (add_ref<std::vector<VkClearValue>> clearValues) const
{
    uint32_t cvSize = data_count(clearValues);
    add_cref<ItemVec> list = getParamRef<ItemVec>();
    uint32_t realIndex = 0u;
    for (add_cref<Item> a : list)
    {
        if (RPA::isResourceAttachment(a.second.mDescription))
        {
            ASSERTMSG(a.second.mIndex == realIndex, "Mismatch: mIndex = ", a.second.mIndex, " realIndex = ", realIndex);
            clearValues.push_back(a.second.mClearValue);
            realIndex = realIndex + 1u;
        }
    }
    return data_count(clearValues) - cvSize;
}

void ZAttachmentPool::updateReferences (
    add_cref<RPRS> referenceIndices,
    add_ref<std::vector<VkAttachmentReference2>> inputAttachments,
    add_ref<uint32_t> inputCount, uint32_t inputOffset,
    add_ref<std::vector<VkAttachmentReference2>> colorAttachments,
    add_ref<uint32_t> colorCount, uint32_t colorOffset,
    add_ref<std::vector<VkAttachmentReference2>> resolveAttachments,
    add_ref<uint32_t> resolveCount, uint32_t resolveOffset,
    add_ref<std::vector<VkAttachmentReference2>> dsAttachments,
    add_ref<uint32_t> dsCount, uint32_t dsOffset) const
{
    inputCount = 0u;
    colorCount = 0u;
    resolveCount = 0u;
    dsCount = 0u;

    MULTI_UNREF(inputOffset, colorOffset, resolveOffset, dsOffset);

    add_cref<ItemVec> descriptions = getParamRef<ItemVec>();
    const uint32_t descriptionCount = data_count(descriptions);
    for (add_cref<RPR> rpr : referenceIndices)
    {
        const uint32_t r = rpr.first;

        ASSERTMSG(r < descriptionCount,
            "Index (", r, ") of bounds (", descriptionCount, ')');
        add_cref<Item> a = descriptions.at(r);

        VkAttachmentReference2 ref = makeVkStruct();
        ref.layout = rpr.second;
        ref.aspectMask = formatGetAspectMask(a.second.format);
        ref.attachment = a.second.mIndex;

        switch (a.second.mDescription)
        {
        case AttachmentDesc::Input:
            ++inputCount;
            inputAttachments.push_back(ref);
            break;
        case AttachmentDesc::Color:
            ++colorCount;
            colorAttachments.push_back(ref);
            break;
        case AttachmentDesc::Resolve:
            ++resolveCount;
            resolveAttachments.push_back(ref);
            break;
        case AttachmentDesc::DeptStencil:
            ++dsCount;
            dsAttachments.push_back(ref);
            break;
        default:
            ASSERTFALSE("Unsupported attachment type: ", demangledName<AttachmentDesc>(), ' ', a.second.mDescription);
        }
    }
}

VkAttachmentDescription2 ZAttachmentPool::get (uint32_t at, add_ref<uint32_t> realIndex) const
{
    return raw(at, realIndex)();
}

add_cref<RPA> ZAttachmentPool::raw (uint32_t at, add_ref<uint32_t> realIndex) const
{
    add_cref<ItemVec> descriptions = getParamRef<ItemVec>();
    ASSERTMSG(descriptions.size() > at, "Index (", at, ") of bounds (", descriptions.size(), ")");
    realIndex = descriptions.at(at).second.mIndex;
    return descriptions.at(at).second;
}

uint32_t ZAttachmentPool::count (AttachmentDesc desc) const
{
    add_cref<ItemVec> list = getParamRef<ItemVec>();
    return uint32_t(std::count_if(list.begin(), list.end(), [&](add_cref<Item> a) { return a.second.mDescription == desc; }));
}

uint32_t ZAttachmentPool::count (add_cref<RPRS> subpassAttachments, AttachmentDesc desc) const
{
    uint32_t result = 0u;
    add_cref<ItemVec> list = getParamRef<ItemVec>();
    const uint32_t listSize = data_count(list);
    for (add_cref<RPR> rpr : subpassAttachments)
    {
        ASSERTMSG(rpr.first < listSize, "???");
        if (list[rpr.first].second.mDescription == desc)
            result = result + 1u;
    }
    return result;
}

uint32_t ZAttachmentPool::size () const
{
    return data_count(getParamRef<ItemVec>());
}

uint32_t ZAttachmentPool::id () const
{
    return getParam<uint32_t>();
}

void freeSubpassDescription2 (add_ptr<ZSubpassDescription2>) {}
ZSubpassDescription2::ZSubpassDescription2 (
    add_cref<RPRS>      attachmentReferences,
    VkPipelineBindPoint pipelineBindPoint,
    uint32_t            viewMask,
    VkSubpassDescriptionFlags flags
)
{
    [this](add_ptr<add_ptr<ZSubpassDescription2>> pp) { *pp = this; }(setter());
    verbose(getGlobalAppFlags().verbose);

    ASSERTMSG(false == attachmentReferences.empty(), "Attachment references must not be empty");
    setParam<ZSubpassDescriptionFlags>(ZSubpassDescriptionFlags::fromFlags(flags));
    setParam<RPRS>(attachmentReferences);
    setParam<VkPipelineBindPoint>(pipelineBindPoint);
    setParam<uint32_t>(viewMask);
}

ZSubpassDependency2::ZSubpassDependency2 (
    VkPipelineStageFlags srcStageMask,
    VkPipelineStageFlags dstStageMask,
    VkAccessFlags        srcAccessMask,
    VkAccessFlags        dstAccessMask,
    VkDependencyFlags    dependencyFlags,
    int32_t              viewOffset)
    : VkSubpassDependency2{
        VK_STRUCTURE_TYPE_SUBPASS_DEPENDENCY_2,
        nullptr,        // const void* pNext;
        INVALID_UINT32, // uint32_t srcSubpass;
        INVALID_UINT32, // uint32_t dstSubpass;
        srcStageMask,
        dstStageMask,
        srcAccessMask,
        dstAccessMask,
        dependencyFlags,
        viewOffset
    }
{
}

VkSubpassDependency2 ZSubpassDependency2::operator ()() const
{
    return static_cast<add_cref<VkSubpassDependency2>>(*this);
}

/*
VkSubpassDescription descriptions[1 + sizeof...(SubpassDescClass)];
descriptions[0] = desc();
uint32_t i = 1u;
UNREF(std::initializer_list<uint32_t>{ (descriptions[i++] = others(), i)... });
return createRenderPassImpl(device, descriptions, ARRAY_LENGTH_CAST(descriptions, uint32_t));
*/
/*
const std::array<VkSubpassDescription, 1 + sizeof...(SubpassDescClass)> a{desc(), others()...};
*/

namespace details
{
void pushRenderpassParams (
    add_ref<std::vector<ZSubpassDescription2>> descs,
    add_ref<std::vector<ZSubpassDependency2>> deps)
{
    const uint32_t descriptionCount = data_count(descs);
    ASSERTMSG(descriptionCount, "There is no subpasses to process");
    for (uint32_t i = 0u; i < data_count(deps); ++i)
    {
        add_ref<ZSubpassDependency2> dep = deps[i];

        if (dep.srcSubpass == 0u) // there was zero descriptions
        {
            dep.srcSubpass = VK_SUBPASS_EXTERNAL;
            dep.dstSubpass = 0u;
        }
        else if ((data_count(deps) - 1u) == i)
        {
            dep.srcSubpass = dep.srcSubpass - 1u;
            dep.dstSubpass = VK_SUBPASS_EXTERNAL;
        }
        else
        {
            dep.dstSubpass = dep.srcSubpass;
            dep.srcSubpass = dep.srcSubpass - 1u;
            ASSERTMSG(dep.dstSubpass < descriptionCount,
                "Cannot insert dependency between descriptions ", dep.srcSubpass, " and ", dep.dstSubpass,
                " because there is only ", descriptionCount, " defined");
        }
    }
}
void pushRenderpassParam (
    add_ref<std::vector<ZSubpassDescription2>> descs,
    add_ref<std::vector<ZSubpassDependency2>> deps,
    ZSubpassDependency2 const& src)
{
    ZSubpassDependency2 dep(src);
    dep.srcSubpass = data_count(descs);
    dep.dstSubpass = data_count(deps);
    deps.push_back(dep);
}
void pushRenderpassParam (
    add_ref<std::vector<ZSubpassDescription2>> descs,
    add_ref<std::vector<ZSubpassDependency2>> deps,
    ZSubpassDescription2 const& desc)
{
    ZSubpassDescription2(desc)
        .setParam<ZDistType<SomeZero, uint32_t>>(data_count(descs))
        .setParam<ZDistType<SomeOne, uint32_t>>(data_count(deps));
    descs.push_back(desc);
}
} // namespace details

ZRenderPass createRenderPassImpl2 (ZDevice device, ZAttachmentPool pool,
    uint32_t subpassCount, add_ptr<ZSubpassDescription2> pSubpasses,
    uint32_t dependencyCount, add_ptr<ZSubpassDependency2> pDependencies)
{
    ASSERTMSG(subpassCount, "There must be at least one subpass defined");

    uint32_t inputCount = 0u;
    uint32_t colorCount = 0u;
    uint32_t resolveCount = 0u;
    uint32_t dsCount = 0u;
    uint32_t inputOffset = 0u;
    uint32_t colorOffset = 0u;
    uint32_t resolveOffset = 0u;
    uint32_t dsOffset = 0u;
    std::vector<VkAttachmentReference2> inputAttachments;
    std::vector<VkAttachmentReference2> colorAttachments;
    std::vector<VkAttachmentReference2> resolveAttachments;
    std::vector<VkAttachmentReference2> dsAttachments;
    std::vector<VkClearValue>           clearValues;
    std::vector<VkSubpassDescription2>  subpassDescriptions;
    std::vector<VkSubpassDependency2>   subpassDependices;
    std::vector<VkAttachmentDescription2> attachmentDescriptions;

    pool.updateClearValues(clearValues);
    pool.updateDescriptions(attachmentDescriptions);

    for (uint32_t iSubpassDesc = 0; iSubpassDesc < subpassCount; ++iSubpassDesc)
    {
        ZSubpassDescription2 subpassDesc = pSubpasses[iSubpassDesc];
        pool.updateReferences(subpassDesc.getParamRef<RPRS>(),
                                inputAttachments,   inputCount,     inputOffset,
                                colorAttachments,   colorCount,     colorOffset,
                                resolveAttachments, resolveCount,   resolveOffset,
                                dsAttachments,      dsCount,        dsOffset);

        VkSubpassDescription2 desc = makeVkStruct();
        desc.flags = subpassDesc.getParam<ZSubpassDescription2::ZSubpassDescriptionFlags>()();
        desc.pipelineBindPoint = subpassDesc.getParam<VkPipelineBindPoint>();
        desc.viewMask = subpassDesc.getParam<uint32_t>();
        desc.inputAttachmentCount = inputCount;
        desc.pInputAttachments = reinterpret_cast<add_ptr<VkAttachmentReference2>>(static_cast<uintptr_t>(inputOffset));
        desc.colorAttachmentCount = colorCount;
        desc.pColorAttachments = reinterpret_cast<add_ptr<VkAttachmentReference2>>(static_cast<uintptr_t>(colorOffset));
        desc.pResolveAttachments = data_or_null(resolveAttachments); // TODO
        desc.pDepthStencilAttachment = data_or_null(dsAttachments); // TODO
        desc.preserveAttachmentCount = (uint32_t)0u;
        desc.pPreserveAttachments = (const uint32_t*)nullptr;

        subpassDescriptions.push_back(desc);

        inputOffset = inputOffset + inputCount;
        colorOffset = colorOffset + colorCount;
        resolveOffset = resolveOffset + resolveCount;
        dsOffset = dsOffset + dsCount;
    }

    for (add_ref<VkSubpassDescription2> desc : subpassDescriptions)
    {
        desc.pColorAttachments = desc.colorAttachmentCount ?
            &colorAttachments[static_cast<uint32_t>(reinterpret_cast<uintptr_t>(desc.pColorAttachments))] : nullptr;
        desc.pInputAttachments = desc.inputAttachmentCount ?
            &inputAttachments[static_cast<uint32_t>(reinterpret_cast<uintptr_t>(desc.pInputAttachments))] : nullptr;
    }

    if (dependencyCount && pDependencies)
    {
        for (uint32_t i = 0; i < dependencyCount; ++i)
        {
            const ZSubpassDependency2 z = pDependencies[i];
            VkSubpassDependency2 dep = z();
            subpassDependices.push_back(dep);
        }
    }

    VkRenderPassCreateInfo2 c{};
    c.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO_2;
    c.pNext = nullptr;
    c.flags = VkRenderPassCreateFlags(0);
    c.attachmentCount   = data_count(attachmentDescriptions);
    c.pAttachments      = data_or_null(attachmentDescriptions);
    c.subpassCount  = data_count(subpassDescriptions);
    c.pSubpasses    = subpassDescriptions.data();
    c.dependencyCount   = data_count(subpassDependices);
    c.pDependencies     = data_or_null(subpassDependices);
    c.correlatedViewMaskCount   = 0u;;
    c.pCorrelatedViewMasks      = (const uint32_t*)nullptr;

    const VkAllocationCallbacksPtr	callbacks = device.getParam<VkAllocationCallbacksPtr>();
    ZRenderPass	renderPass = ZRenderPass::create(VK_NULL_HANDLE, device, callbacks, 2,
                                                    c.attachmentCount, c.subpassCount,
                                                    clearValues, VK_FORMAT_UNDEFINED, VK_IMAGE_LAYOUT_UNDEFINED,
                                                    std::any(pool), {/*subpassDescriptions*/});

    renderPass.verbose(getGlobalAppFlags().verbose);
    add_ref<std::vector<ZDistType<SomeTwo, std::any>>> ss =
        renderPass.getParamRef<std::vector<ZDistType<SomeTwo, std::any>>>();
    for (uint32_t i = 0u; i < subpassCount; ++i)
    {
        ss.push_back(std::any(pSubpasses[i]));
    }

    add_cref<ZDeviceInterface> di = device.getInterface();
    ASSERTMSG(di.vkCreateRenderPass2, "vkCreateRenderPass2() must not be null, "
        "you need ", VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME, " or Vulkan 1.2");
    VKASSERT(di.vkCreateRenderPass2(*device, &c, callbacks, renderPass.setter()));

    return renderPass;
}

uint32_t renderPassGetAttachmentCount (ZRenderPass renderPass, uint32_t subpass)
{
    add_cref<std::vector<ZDistType<SomeTwo, std::any>>> ss =
        renderPass.getParamRef<std::vector<ZDistType<SomeTwo, std::any>>>();
    const uint32_t subpassCount = data_count(ss);

    if (subpassCount == 0u)
    {
        return renderPass.getParam<ZDistType<AttachmentCount, uint32_t>>().get();
    }

    ASSERTMSG(subpass < subpassCount, "Definition subpass ", subpass, " not found among ", subpassCount);
    ZSubpassDescription2 dep = std::any_cast<ZSubpassDescription2>(ss[subpass].get());

    return data_count(dep.getParamRef<RPRS>());
}

uint32_t renderPassSubpassGetColorAttachmentCount (ZRenderPass renderPass, uint32_t subpass)
{
    add_cref<std::vector<ZDistType<SomeTwo, std::any>>> ss =
        renderPass.getParamRef<std::vector<ZDistType<SomeTwo, std::any>>>();
    const uint32_t subpassCount = data_count(ss);

    if (subpassCount == 0u)
    {
        return renderPass.getParam<ZDistType<AttachmentCount, uint32_t>>().get();
    }

    ZAttachmentPool pool = std::any_cast<ZAttachmentPool>(renderPass.getParamRef<ZDistType<SomeOne, std::any>>().get());
    ASSERTMSG(subpass < subpassCount, "Definition subpass ", subpass, " not found among ", subpassCount);
    ZSubpassDescription2 dep = std::any_cast<ZSubpassDescription2>(ss[subpass].get());

    return pool.count(dep.getParamRef<RPRS>(), AttachmentDesc::Color);
}

uint32_t frameBufferTransitAttachments (
    ZRenderPass optRenderPass,
    ZFramebuffer framebuffer,
    uint32_t subpass,
    std::optional<bool> initialLayout)
{
    ZRenderPass renderPass = optRenderPass ? optRenderPass : framebuffer.getParam<ZRenderPass>();
    add_cref<std::vector<ZDistType<SomeTwo, std::any>>> ss =
        renderPass.getParamRef<std::vector<ZDistType<SomeTwo, std::any>>>();
    const uint32_t subpassCount = data_count(ss);
    if (subpassCount == 0u) return INVALID_UINT32; // for compability with ZRenderPass(1)
    ZAttachmentPool pool = std::any_cast<ZAttachmentPool>(renderPass.getParamRef<ZDistType<SomeOne, std::any>>().get());

    add_ref<std::vector<ZImageView>> attachments = framebuffer.getParamRef<std::vector<ZImageView>>();
    const uint32_t attachmentCount = data_count(attachments);
    std::vector<VkAttachmentDescription2> descriptions;
    pool.updateDescriptions(descriptions);
    const uint32_t descriptionCount = data_count(descriptions);
    ASSERTMSG(attachmentCount == descriptionCount, "Attachments in framebuffer (", attachmentCount, ") "
        "must match to the VkAttachmentDescription2 list (", descriptionCount, ")");

    if (INVALID_UINT32 == subpass)
    {
        ASSERTMSG(initialLayout, "???");
        for (uint32_t i = 0u; i < attachmentCount; ++i)
        {
            ZImage image = imageViewGetImage(attachments[i]);
            ASSERTMSG(imageGetFormat(image) == descriptions[i].format,
                "Framebuffer image format (", formatGetString(imageGetFormat(image)), ") and "
                "renderpass attachment description (", formatGetString(descriptions[i].format), ") "
                "at index (", i, ")");
            imageResetLayout(image, (initialLayout.value() ? descriptions[i].initialLayout : descriptions[i].finalLayout));
        }

        return attachmentCount;
    }

    uint32_t processedAttachments = 0u;
    ASSERTMSG(subpass < subpassCount, "Definition subpass ", subpass, " not found among ", subpassCount);
    ZSubpassDescription2 dep = std::any_cast<ZSubpassDescription2>(ss[subpass].get());
    for (add_cref<RPR> rpr : dep.getParamRef<RPRS>())
    {
        uint32_t realIndex = INVALID_UINT32;
        add_cref<RPA> rpa = pool.raw(rpr.first, realIndex);;
        ASSERTMSG(realIndex < descriptionCount, "Unable to find resource for index ", realIndex);
        ZImage image = imageViewGetImage(attachments[realIndex]);

        ASSERTMSG(imageGetFormat(image) == rpa.format,
            "Framebuffer image format (", formatGetString(imageGetFormat(image)), ") and "
            "renderpass attachment description (", formatGetString(rpa.format), ") "
            "at index (", rpr.first, ")");
        imageResetLayout(image, rpr.second);

        processedAttachments = processedAttachments + 1u;
    }

    return processedAttachments;
}

} // namespace vtf
