#include "vtfZRenderPass.hpp"
#include "vtfZImage.hpp"
#include "vtfBacktrace.hpp"

namespace vtf
{

static VkAttachmentDescription reduce (add_cref<VkAttachmentDescription2> src)
{
    VkAttachmentDescription res{};
    // VkStructureType   sType;
    // const void*       pNext;
    res.flags = src.flags;
    res.format = src.format;
    res.samples = src.samples;
    res.loadOp = src.loadOp;
    res.storeOp = src.storeOp;
    res.stencilLoadOp = src.stencilLoadOp;
    res.stencilStoreOp = src.stencilStoreOp;
    res.initialLayout = src.initialLayout;
    res.finalLayout = src.finalLayout;
    return res;
}

static VkSubpassDependency reduce (add_cref<VkSubpassDependency2> src)
{
    VkSubpassDependency res{};
    // VkStructureType  sType;
    // const void*      pNext;
    res.srcSubpass = src.srcSubpass;
    res.dstSubpass = src.dstSubpass;
    res.srcStageMask = src.srcStageMask;
    res.dstStageMask = src.dstStageMask;
    res.srcAccessMask = src.srcAccessMask;
    res.dstAccessMask = src.dstAccessMask;
    res.dependencyFlags = src.dependencyFlags;
    // int32_t  viewOffset;
    return res;
}

void ZAttachmentPool::updateReferences (
    add_cref<RPARS> referenceIndices,
    add_ref<std::vector<VkAttachmentReference>> inputAttachments,
    add_ref<uint32_t> inputCount, uint32_t inputOffset,
    add_ref<std::vector<VkAttachmentReference>> colorAttachments,
    add_ref<uint32_t> colorCount, uint32_t colorOffset,
    add_ref<std::vector<VkAttachmentReference>> resolveAttachments,
    add_ref<uint32_t> resolveCount, uint32_t resolveOffset,
    add_ref<std::vector<VkAttachmentReference>> dsAttachments,
    add_ref<uint32_t> dsCount, uint32_t dsOffset) const
{
    inputCount = 0u;
    colorCount = 0u;
    resolveCount = 0u;
    dsCount = 0u;

    MULTI_UNREF(inputOffset, colorOffset, resolveOffset, dsOffset);

    add_cref<ItemVec> descriptions = getParamRef<ItemVec>();
    const uint32_t descriptionCount = data_count(descriptions);
    for (add_cref<RPAR> rpr : referenceIndices)
    {
        const uint32_t r = rpr.first;

        ASSERTMSG(r < descriptionCount,
            "Index (", r, ") of bounds (", descriptionCount, ')');
        add_cref<Item> a = descriptions.at(r);

        VkAttachmentReference ref{};
        ref.layout = rpr.second;
        // ref.aspectMask = formatGetAspectMask(a.second.format); introduced in VkAttachmentReference2
        ref.attachment = a.second.mIndex;

        switch (a.second.mDescription)
        {
        case AttachmentDesc::Input:
            ++inputCount;
            inputAttachments.push_back(ref);
            break;
        case AttachmentDesc::Color:
        case AttachmentDesc::DeptStencil:
        case AttachmentDesc::Presentation:
            ++colorCount;
            colorAttachments.push_back(ref);
            break;
        case AttachmentDesc::Resolve:
            ++resolveCount;
            resolveAttachments.push_back(ref);
            break;
        case AttachmentDesc::DSAttachment:
            ++dsCount;
            dsAttachments.push_back(ref);
            break;
        default:
            ASSERTFALSE("Unsupported attachment type: ", demangledName<AttachmentDesc>(), ' ', a.second.mDescription);
        }
    }
}

ZRenderPass createRenderPassImpl (ZDevice device, ZAttachmentPool pool,
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
    std::vector<VkAttachmentReference> inputAttachments;
    std::vector<VkAttachmentReference> colorAttachments;
    std::vector<VkAttachmentReference> resolveAttachments;
    std::vector<VkAttachmentReference> dsAttachments;
    std::vector<VkClearValue>          clearValues;
    std::vector<VkSubpassDescription>  subpassDescriptions;
    std::vector<VkSubpassDependency>   subpassDependices;
    std::vector<VkAttachmentDescription> attachmentDescriptions;
    std::vector<VkAttachmentDescription2> attachmentDescriptions2;

    pool.updateClearValues(clearValues);
    pool.updateDescriptions(attachmentDescriptions2);
    std::transform(attachmentDescriptions2.begin(), attachmentDescriptions2.end(),
                    std::back_inserter(attachmentDescriptions),
                        static_cast<VkAttachmentDescription(*)(add_cref<VkAttachmentDescription2>)>(reduce));

    for (uint32_t iSubpassDesc = 0; iSubpassDesc < subpassCount; ++iSubpassDesc)
    {
        ZSubpassDescription2 subpassDesc = pSubpasses[iSubpassDesc];
        pool.updateReferences(subpassDesc.getParamRef<RPARS>(),
            inputAttachments, inputCount, inputOffset,
            colorAttachments, colorCount, colorOffset,
            resolveAttachments, resolveCount, resolveOffset,
            dsAttachments, dsCount, dsOffset);

        VkSubpassDescription desc{};
        desc.flags = subpassDesc.getParam<ZSubpassDescription2::ZSubpassDescriptionFlags>()();
        desc.pipelineBindPoint = subpassDesc.getParam<VkPipelineBindPoint>();
        // desc.viewMask = subpassDesc.getParam<uint32_t>(); introduced in VkSubpassDescription2
        desc.inputAttachmentCount = inputCount;
        desc.pInputAttachments = reinterpret_cast<add_ptr<VkAttachmentReference>>(static_cast<uintptr_t>(inputOffset));
        desc.colorAttachmentCount = colorCount;
        desc.pColorAttachments = reinterpret_cast<add_ptr<VkAttachmentReference>>(static_cast<uintptr_t>(colorOffset));
        desc.pResolveAttachments = reinterpret_cast<add_ptr<VkAttachmentReference>>(static_cast<uintptr_t>(resolveOffset));
        desc.pDepthStencilAttachment = (0u == dsCount)
                                        ? nullptr
                                        : reinterpret_cast<add_ptr<VkAttachmentReference>>(static_cast<uintptr_t>(dsOffset + 1u));
        desc.preserveAttachmentCount = (uint32_t)0u;
        desc.pPreserveAttachments = (const uint32_t*)nullptr;

        subpassDescriptions.push_back(desc);

        inputOffset = inputOffset + inputCount;
        colorOffset = colorOffset + colorCount;
        resolveOffset = resolveOffset + resolveCount;
        dsOffset = dsOffset + dsCount;
    }

    for (add_ref<VkSubpassDescription> desc : subpassDescriptions)
    {
        desc.pColorAttachments = desc.colorAttachmentCount ?
            &colorAttachments[static_cast<uint32_t>(reinterpret_cast<uintptr_t>(desc.pColorAttachments))] : nullptr;
        desc.pInputAttachments = desc.inputAttachmentCount ?
            &inputAttachments[static_cast<uint32_t>(reinterpret_cast<uintptr_t>(desc.pInputAttachments))] : nullptr;

        dsOffset = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(desc.pDepthStencilAttachment));
        if (dsOffset) desc.pDepthStencilAttachment = &dsAttachments[dsOffset - 1u];

        resolveOffset = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(desc.pResolveAttachments));
        desc.pResolveAttachments = (resolveOffset < data_count(resolveAttachments)) ? &resolveAttachments[resolveOffset] : nullptr;
    }

    if (dependencyCount && pDependencies)
    {
        for (uint32_t i = 0; i < dependencyCount; ++i)
        {
            const ZSubpassDependency2 z = pDependencies[i];
            VkSubpassDependency dep = reduce(z());
            subpassDependices.push_back(dep);
        }
    }

    VkRenderPassCreateInfo c{};
    c.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    c.pNext = nullptr;
    c.flags = VkRenderPassCreateFlags(0);
    c.attachmentCount = data_count(attachmentDescriptions);
    c.pAttachments = data_or_null(attachmentDescriptions);
    c.subpassCount = data_count(subpassDescriptions);
    c.pSubpasses = subpassDescriptions.data();
    c.dependencyCount = data_count(subpassDependices);
    c.pDependencies = data_or_null(subpassDependices);
    // c.correlatedViewMaskCount = 0u; introduced in VkSubpassDescription2
    // c.pCorrelatedViewMasks = (const uint32_t*)nullptr; introduced in VkSubpassDescription2

    const VkAllocationCallbacksPtr	callbacks = device.getParam<VkAllocationCallbacksPtr>();
    ZRenderPass	renderPass = ZRenderPass::create(VK_NULL_HANDLE, device, callbacks, 1,
        c.attachmentCount, c.subpassCount,
        clearValues, VK_FORMAT_UNDEFINED, VK_IMAGE_LAYOUT_UNDEFINED,
        std::any(pool), {/*subpassDescriptions*/ });

    renderPass.verbose(getGlobalAppFlags().verbose);
    add_ref<std::vector<ZDistType<SomeTwo, std::any>>> ss =
        renderPass.getParamRef<std::vector<ZDistType<SomeTwo, std::any>>>();
    for (uint32_t i = 0u; i < subpassCount; ++i)
    {
        ss.push_back(std::any(pSubpasses[i]));
    }

    add_cref<ZDeviceInterface> di = device.getInterface();
    ASSERTMSG(di.vkCreateRenderPass, "vkCreateRenderPass() must not be null");
    VKASSERT(di.vkCreateRenderPass(*device, &c, callbacks, renderPass.setter()));

    return renderPass;
}

ZFramebuffer _createFramebuffer (VkImage presentImage, ZRenderPass renderpass, uint32_t width, uint32_t height)
{
    ZDevice device = renderpass.getParam<ZDevice>();
    ZAttachmentPool pool = std::any_cast<ZAttachmentPool>(renderpass.getParamRef<ZDistType<SomeOne, std::any>>().get());
    add_cref<ItemVec> descriptions = pool.getParamRef<ItemVec>();
    const uint32_t descriptionCount = data_count(descriptions);
    std::vector<ZImageView> views(descriptionCount);
    uint32_t viewCount = 0u;
    bool wasPresentation, hasPresentation = false;
    for (uint32_t i = 0u; i < descriptionCount; ++i)
    {
        wasPresentation = false;
        add_cref<RPA> desc = descriptions.at(i).second;
        if (RPA::isResourceAttachment(desc.mDescription))
        {
            const VkImageUsageFlagBits vkUsage = formatIsDepthStencil(ZPhysicalDevice(), desc.format, false).first
                                                    ? VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT
                                                    : VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
            const ZImageUsageFlags		zUsage  (vkUsage, VK_IMAGE_USAGE_TRANSFER_SRC_BIT, VK_IMAGE_USAGE_TRANSFER_DST_BIT);

            if (desc.mDescription == AttachmentDesc::Presentation && false == hasPresentation)
            {
                hasPresentation = true;
                wasPresentation = true;
                ZNonDeletableImage image = ZNonDeletableImage::create(presentImage, device, desc.format, width, height, zUsage);
                ZImageView view = ::vtf::createImageView(image);
                views[viewCount++] = view;
            }

            if (false == wasPresentation)
            {
                ZImage image = createImage(device, desc.format, VK_IMAGE_TYPE_2D, width, height, zUsage, desc.samples);
                ZImageView view = ::vtf::createImageView(image);
                views[viewCount++] = view;
            }
        }
    }

    return createFramebuffer(renderpass, width, height, views, viewCount);
}

ZRenderPass createSinglePresentationRenderPass (
    ZDevice device, VkFormat presentationImageFormat, add_cref<VkClearValue> clearColor)
{
    const std::vector<RPA>		colors{ RPA(AttachmentDesc::Presentation, presentationImageFormat, clearColor,
                                                        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR) };
    const ZAttachmentPool		attachmentPool(colors);
    const ZSubpassDescription2	subpass({ RPAR(0u, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) });
    
    return createRenderPass(device, attachmentPool, subpass);
}

} // namespace vtf
