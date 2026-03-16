//======================================================================
// Render/VK/VKRenderer_Core.cpp
//  - SDL3 + Vulkan (MoltenVK)
//  - Init / Shutdown / Swapchain / Depth / RenderPass / Cmd / Sync
//  - DescriptorPool / SceneUBO / SceneSet
//
// 方針（確定）:
//  - SceneUBO は World/UI 分離（mSceneUBO / mSceneUBO_UI）
//  - BeginFrame() で World UBO を更新（UpdateSceneUBO_World）
//  - DrawUIPass() 側で UI UBO を更新（UpdateSceneUBO_UI）
//  - Swapchain recreate 時は Pipeline → SceneSet の順で作り直す
//  - ★Skinned palette は slot pool を持ち、recreate時は DestroySkinnedSlots() で破棄
//======================================================================
#include "Render/VK/VKRenderer.h"

#include "Engine/Core/Application.h"
#include "Render/RenderBackendState.h"
#include "Render/VK/VKUtil.h"
#include "Render/VK/VKSceneRenderTarget.h"
#include "Render/VK/Pipeline/VKPipelinePresets.h"

#include <vulkan/vulkan.h>
#include <SDL3/SDL_vulkan.h>

#include <iostream>
#include <vector>
#include <set>
#include <algorithm>

namespace toy
{

static const char* kValidationLayers[] =
{
    "VK_LAYER_KHRONOS_validation"
};

//--------------------------------------------------------------
// ctor/dtor
//--------------------------------------------------------------
bool VKRenderer::CreateInstance()
{
    Uint32 sdlExtCount = 0;
    const char* const* sdlExts = SDL_Vulkan_GetInstanceExtensions(&sdlExtCount);
    if (!sdlExts || sdlExtCount == 0)
    {
        std::cerr << "[VKRenderer] SDL_Vulkan_GetInstanceExtensions failed: " << SDL_GetError() << "\n";
        return false;
    }

    std::vector<const char*> exts;
    exts.reserve((size_t)sdlExtCount + 8);
    for (Uint32 i = 0; i < sdlExtCount; ++i)
    {
        exts.push_back(sdlExts[i]);
    }

    const auto availExts   = toy::vkutil::GetInstanceExts();
    const auto availLayers = toy::vkutil::GetInstanceLayers();

#if !defined(NDEBUG)
    mEnableValidation =
        mEnableValidation &&
        toy::vkutil::HasLayer(kValidationLayers[0], availLayers);

    if (mEnableValidation)
    {
        if (toy::vkutil::HasInstanceExt(VK_EXT_DEBUG_UTILS_EXTENSION_NAME, availExts))
        {
            exts.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }
    }
#else
    mEnableValidation = false;
#endif

    const bool hasPortEnum =
        toy::vkutil::HasInstanceExt(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME, availExts);
    if (hasPortEnum)
    {
        exts.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
    }

    VkApplicationInfo appInfo{};
    appInfo.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName   = "ToyLibGame";
    appInfo.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
    appInfo.pEngineName        = "ToyLib";
    appInfo.engineVersion      = VK_MAKE_VERSION(0, 1, 0);
    appInfo.apiVersion         = VK_API_VERSION_1_2;

    VkInstanceCreateInfo ici{};
    ici.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    ici.pApplicationInfo        = &appInfo;
    ici.enabledExtensionCount   = (uint32_t)exts.size();
    ici.ppEnabledExtensionNames = exts.data();

    if (hasPortEnum)
    {
        ici.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
    }

    std::vector<const char*> layers;
    VkDebugUtilsMessengerCreateInfoEXT dbgCI{};
#if !defined(NDEBUG)
    if (mEnableValidation)
    {
        layers.push_back(kValidationLayers[0]);
        ici.enabledLayerCount   = (uint32_t)layers.size();
        ici.ppEnabledLayerNames = layers.data();

        dbgCI.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        dbgCI.messageSeverity =
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        dbgCI.messageType =
            VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        dbgCI.pfnUserCallback = toy::vkutil::DebugCallback;

        ici.pNext = &dbgCI;
    }
#endif

    VkResult vr = vkCreateInstance(&ici, nullptr, &mInstance);
    if (vr != VK_SUCCESS || mInstance == VK_NULL_HANDLE)
    {
        std::cerr << "[VKRenderer] vkCreateInstance failed: " << vr << "\n";
        return false;
    }

#if !defined(NDEBUG)
    if (mEnableValidation)
    {
        toy::vkutil::CreateDebugUtilsMessengerEXT(mInstance, &dbgCI, &mDebugMessenger);
    }
#endif

    return true;
}

bool VKRenderer::CreateSurface()
{
    if (!SDL_Vulkan_CreateSurface(mWindow, mInstance, nullptr, &mSurface))
    {
        std::cerr << "[VKRenderer] SDL_Vulkan_CreateSurface failed: " << SDL_GetError() << "\n";
        return false;
    }
    return true;
}

bool VKRenderer::PickPhysicalDevice()
{
    uint32_t count = 0;
    VkResult vr = vkEnumeratePhysicalDevices(mInstance, &count, nullptr);
    if (vr != VK_SUCCESS || count == 0)
    {
        std::cerr << "[VKRenderer] No Vulkan physical devices. vr=" << vr << "\n";
        return false;
    }

    std::vector<VkPhysicalDevice> devs(count);
    vr = vkEnumeratePhysicalDevices(mInstance, &count, devs.data());
    if (vr != VK_SUCCESS)
    {
        std::cerr << "[VKRenderer] vkEnumeratePhysicalDevices(list) failed. vr=" << vr << "\n";
        return false;
    }

    for (auto dev : devs)
    {
        const auto q = toy::vkutil::FindQueueFamilies(dev, mSurface);
        if (!q.IsComplete()) continue;

        const auto de = toy::vkutil::GetDeviceExts(dev);
        if (!toy::vkutil::HasDeviceExt(VK_KHR_SWAPCHAIN_EXTENSION_NAME, de))
        {
            continue;
        }

        const auto sc = toy::vkutil::QuerySwapchainSupport(dev, mSurface);
        if (sc.formats.empty() || sc.presentModes.empty())
        {
            continue;
        }

        mPhysicalDevice = dev;
        mQueueFamilyGraphics = q.graphics.value();
        mQueueFamilyPresent  = q.present.value();

        VkPhysicalDeviceProperties props{};
        vkGetPhysicalDeviceProperties(mPhysicalDevice, &props);
        std::cerr << "[VKRenderer] GPU: " << props.deviceName << "\n";
        mDeviceName = props.deviceName;
        
        return true;
    }

    std::cerr << "[VKRenderer] No suitable GPU found.\n";
    return false;
}

bool VKRenderer::CreateDeviceAndQueues()
{
    std::set<uint32_t> uniqueFamilies = { mQueueFamilyGraphics, mQueueFamilyPresent };

    float prio = 1.0f;
    std::vector<VkDeviceQueueCreateInfo> qcis;
    qcis.reserve(uniqueFamilies.size());

    for (uint32_t fam : uniqueFamilies)
    {
        VkDeviceQueueCreateInfo qci{};
        qci.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        qci.queueFamilyIndex = fam;
        qci.queueCount       = 1;
        qci.pQueuePriorities = &prio;
        qcis.push_back(qci);
    }

    std::vector<const char*> devExts;
    devExts.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

    VkPhysicalDeviceFeatures features{};

    VkDeviceCreateInfo dci{};
    dci.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    dci.queueCreateInfoCount    = (uint32_t)qcis.size();
    dci.pQueueCreateInfos       = qcis.data();
    dci.enabledExtensionCount   = (uint32_t)devExts.size();
    dci.ppEnabledExtensionNames = devExts.data();
    dci.pEnabledFeatures        = &features;

#if !defined(NDEBUG)
    std::vector<const char*> layers;
    if (mEnableValidation)
    {
        layers.push_back(kValidationLayers[0]);
        dci.enabledLayerCount   = (uint32_t)layers.size();
        dci.ppEnabledLayerNames = layers.data();
    }
#endif

    VkResult vr = vkCreateDevice(mPhysicalDevice, &dci, nullptr, &mDevice);
    if (vr != VK_SUCCESS || mDevice == VK_NULL_HANDLE)
    {
        std::cerr << "[VKRenderer] vkCreateDevice failed: " << vr << "\n";
        return false;
    }

    vkGetDeviceQueue(mDevice, mQueueFamilyGraphics, 0, &mQueueGraphics);
    vkGetDeviceQueue(mDevice, mQueueFamilyPresent,  0, &mQueuePresent);

    return (mQueueGraphics != VK_NULL_HANDLE && mQueuePresent != VK_NULL_HANDLE);
}

bool VKRenderer::CreateSwapchainAndViews()
{
    int pixelW = 0;
    int pixelH = 0;
    SDL_GetWindowSizeInPixels(mWindow, &pixelW, &pixelH);

    const auto sc = toy::vkutil::QuerySwapchainSupport(mPhysicalDevice, mSurface);
    if (sc.formats.empty() || sc.presentModes.empty())
    {
        std::cerr << "[VKRenderer] Swapchain support incomplete.\n";
        return false;
    }

    mSwapchainFormat = toy::vkutil::ChooseSurfaceFormat(sc.formats);
    const VkPresentModeKHR pm = toy::vkutil::ChoosePresentMode(sc.presentModes, /*vsync*/ true);
    mSwapchainExtent = toy::vkutil::ChooseExtent(sc.caps, pixelW, pixelH);

    uint32_t imageCount = sc.caps.minImageCount + 1;
    if (sc.caps.maxImageCount > 0 && imageCount > sc.caps.maxImageCount)
    {
        imageCount = sc.caps.maxImageCount;
    }

    VkSwapchainCreateInfoKHR sci{};
    sci.sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    sci.surface          = mSurface;
    sci.minImageCount    = imageCount;
    sci.imageFormat      = mSwapchainFormat.format;
    sci.imageColorSpace  = mSwapchainFormat.colorSpace;
    sci.imageExtent      = mSwapchainExtent;
    sci.imageArrayLayers = 1;
    sci.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    uint32_t qIdx[] = { mQueueFamilyGraphics, mQueueFamilyPresent };
    if (mQueueFamilyGraphics != mQueueFamilyPresent)
    {
        sci.imageSharingMode      = VK_SHARING_MODE_CONCURRENT;
        sci.queueFamilyIndexCount = 2;
        sci.pQueueFamilyIndices   = qIdx;
    }
    else
    {
        sci.imageSharingMode      = VK_SHARING_MODE_EXCLUSIVE;
    }

    sci.preTransform   = sc.caps.currentTransform;
    sci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    sci.presentMode    = pm;
    sci.clipped        = VK_TRUE;
    sci.oldSwapchain   = VK_NULL_HANDLE;

    VkResult vr = vkCreateSwapchainKHR(mDevice, &sci, nullptr, &mSwapchain);
    if (vr != VK_SUCCESS || mSwapchain == VK_NULL_HANDLE)
    {
        std::cerr << "[VKRenderer] vkCreateSwapchainKHR failed: " << vr << "\n";
        return false;
    }

    uint32_t scImgCount = 0;
    vr = vkGetSwapchainImagesKHR(mDevice, mSwapchain, &scImgCount, nullptr);
    if (vr != VK_SUCCESS || scImgCount == 0)
    {
        std::cerr << "[VKRenderer] vkGetSwapchainImagesKHR(count) failed: " << vr << "\n";
        return false;
    }

    mSwapchainImages.resize(scImgCount, VK_NULL_HANDLE);
    vr = vkGetSwapchainImagesKHR(mDevice, mSwapchain, &scImgCount, mSwapchainImages.data());
    if (vr != VK_SUCCESS)
    {
        std::cerr << "[VKRenderer] vkGetSwapchainImagesKHR(list) failed: " << vr << "\n";
        return false;
    }

    // ★追加
    mImagesInFlight.assign(scImgCount, VK_NULL_HANDLE);
    
    mSwapchainImageViews.resize(scImgCount, VK_NULL_HANDLE);
    for (uint32_t i = 0; i < scImgCount; ++i)
    {
        VkImageViewCreateInfo iv{};
        iv.sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        iv.image    = mSwapchainImages[i];
        iv.viewType = VK_IMAGE_VIEW_TYPE_2D;
        iv.format   = mSwapchainFormat.format;
        iv.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        iv.subresourceRange.baseMipLevel   = 0;
        iv.subresourceRange.levelCount     = 1;
        iv.subresourceRange.baseArrayLayer = 0;
        iv.subresourceRange.layerCount     = 1;

        vr = vkCreateImageView(mDevice, &iv, nullptr, &mSwapchainImageViews[i]);
        if (vr != VK_SUCCESS)
        {
            std::cerr << "[VKRenderer] vkCreateImageView failed: " << vr << "\n";
            return false;
        }
    }

    return true;
}

//--------------------------------------------------------------
// Depth for swapchain (唯一のdepth生成経路)
//--------------------------------------------------------------
bool VKRenderer::CreateDepthForSwapchain()
{
    if (!mPhysicalDevice || !mDevice)
    {
        return false;
    }

    mDepthFormat = toy::vkutil::ChooseDepthFormat(mPhysicalDevice);
    if (mDepthFormat == VK_FORMAT_UNDEFINED)
    {
        std::cerr << "[VKRenderer] ChooseDepthFormat returned UNDEFINED\n";
        return false;
    }

    DestroyDepthForSwapchain();

    if (!toy::vkutil::CreateImage2D(
            mPhysicalDevice,
            mDevice,
            mSwapchainExtent.width,
            mSwapchainExtent.height,
            mDepthFormat,
            VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            mDepthImage,
            mDepthMemory,
            VK_IMAGE_LAYOUT_UNDEFINED))
    {
        std::cerr << "[VKRenderer] CreateImage2D(depth) failed\n";
        return false;
    }

    VkImageAspectFlags aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
    if (toy::vkutil::HasStencilComponent(mDepthFormat))
    {
        aspect |= VK_IMAGE_ASPECT_STENCIL_BIT;
    }

    mDepthImageView = toy::vkutil::CreateImageView2D(mDevice, mDepthImage, mDepthFormat, aspect);
    if (mDepthImageView == VK_NULL_HANDLE)
    {
        std::cerr << "[VKRenderer] CreateImageView2D(depth) failed\n";
        return false;
    }

    return true;
}

void VKRenderer::DestroyDepthForSwapchain()
{
    if (!mDevice) return;

    if (mDepthImageView)
    {
        vkDestroyImageView(mDevice, mDepthImageView, nullptr);
        mDepthImageView = VK_NULL_HANDLE;
    }
    if (mDepthImage)
    {
        vkDestroyImage(mDevice, mDepthImage, nullptr);
        mDepthImage = VK_NULL_HANDLE;
    }
    if (mDepthMemory)
    {
        vkFreeMemory(mDevice, mDepthMemory, nullptr);
        mDepthMemory = VK_NULL_HANDLE;
    }
}

//--------------------------------------------------------------
// RenderPass (swapchain) : Color + Depth
//--------------------------------------------------------------
bool VKRenderer::CreateRenderPass()
{
    if (mRenderPass != VK_NULL_HANDLE)
    {
        return true;
    }
    if (!mDevice)
    {
        std::cerr << "[VKRenderer] CreateRenderPass: mDevice is null\n";
        return false;
    }
    if (mDepthFormat == VK_FORMAT_UNDEFINED)
    {
        std::cerr << "[VKRenderer] CreateRenderPass: depth format undefined\n";
        return false;
    }

    VkAttachmentDescription color{};
    color.format         = mSwapchainFormat.format;
    color.samples        = VK_SAMPLE_COUNT_1_BIT;
    color.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
    color.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    color.finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentDescription depth{};
    depth.format         = mDepthFormat;
    depth.samples        = VK_SAMPLE_COUNT_1_BIT;
    depth.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depth.storeOp        = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depth.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    depth.finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorRef{};
    colorRef.attachment = 0;
    colorRef.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthRef{};
    depthRef.attachment = 1;
    depthRef.layout     = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount    = 1;
    subpass.pColorAttachments       = &colorRef;
    subpass.pDepthStencilAttachment = &depthRef;

    VkSubpassDependency dep{};
    dep.srcSubpass    = VK_SUBPASS_EXTERNAL;
    dep.dstSubpass    = 0;
    dep.srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dep.dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    VkAttachmentDescription atts[2] = { color, depth };

    VkRenderPassCreateInfo rpci{};
    rpci.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rpci.attachmentCount = 2;
    rpci.pAttachments    = atts;
    rpci.subpassCount    = 1;
    rpci.pSubpasses      = &subpass;
    rpci.dependencyCount = 1;
    rpci.pDependencies   = &dep;

    VkResult vr = vkCreateRenderPass(mDevice, &rpci, nullptr, &mRenderPass);
    if (vr != VK_SUCCESS || mRenderPass == VK_NULL_HANDLE)
    {
        std::cerr << "[VKRenderer] vkCreateRenderPass failed: " << vr << "\n";
        return false;
    }

    return true;
}

bool VKRenderer::CreateFramebuffers()
{
    if (!mDevice)
    {
        std::cerr << "[VKRenderer] CreateFramebuffers: mDevice is null\n";
        return false;
    }
    if (mRenderPass == VK_NULL_HANDLE)
    {
        std::cerr << "[VKRenderer] CreateFramebuffers: mRenderPass is null\n";
        return false;
    }
    if (mSwapchainImageViews.empty())
    {
        std::cerr << "[VKRenderer] CreateFramebuffers: no swapchain image views\n";
        return false;
    }
    if (mDepthImageView == VK_NULL_HANDLE)
    {
        std::cerr << "[VKRenderer] CreateFramebuffers: depth view is null\n";
        return false;
    }

    for (auto fb : mFramebuffers)
    {
        if (fb) vkDestroyFramebuffer(mDevice, fb, nullptr);
    }
    mFramebuffers.clear();

    mFramebuffers.resize(mSwapchainImageViews.size(), VK_NULL_HANDLE);

    for (size_t i = 0; i < mSwapchainImageViews.size(); ++i)
    {
        VkImageView attachments[] =
        {
            mSwapchainImageViews[i],
            mDepthImageView
        };

        VkFramebufferCreateInfo fci{};
        fci.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fci.renderPass      = mRenderPass;
        fci.attachmentCount = 2;
        fci.pAttachments    = attachments;
        fci.width           = mSwapchainExtent.width;
        fci.height          = mSwapchainExtent.height;
        fci.layers          = 1;

        VkResult vr = vkCreateFramebuffer(mDevice, &fci, nullptr, &mFramebuffers[i]);
        if (vr != VK_SUCCESS || mFramebuffers[i] == VK_NULL_HANDLE)
        {
            std::cerr << "[VKRenderer] vkCreateFramebuffer failed: " << vr
                      << " (i=" << i << ")\n";
            return false;
        }
    }

    return true;
}

} // namespace toy
