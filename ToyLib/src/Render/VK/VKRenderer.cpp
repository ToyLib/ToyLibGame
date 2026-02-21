//======================================================================
// VKRenderer.cpp
//  - SDL3 + Vulkan
//  - “まずはウィンドウが出て、clearしてpresentできる土台” まで
//  - 新設計：VKUtil / RenderBackendState / PipelineHandle 対応
//  - 旧コード互換の堅牢化：
//      * Instance apiVersion を「ローダ対応にクランプ」（= -9 対策）
//      * SDL が返す拡張も「存在するものだけ」有効化（重複排除）
//      * Begin/End の整合性（mFrameBegan ガード）
//======================================================================

#include "Render/VK/VKRenderer.h"
#include "Render/VK/VKPipeline.h"
#include "Render/VK/VKUtil.h"
#include "Engine/Core/Application.h"
#include "Render/RenderBackendState.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <vulkan/vulkan.h>

#include <iostream>
#include <vector>
#include <string>
#include <cstring>
#include <set>
#include <algorithm>

#ifndef VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME
#define VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME "VK_KHR_portability_subset"
#endif

namespace toy
{

//--------------------------------------------------------------
// Validation layer name
//--------------------------------------------------------------
static const char* kValidationLayers[] =
{
    "VK_LAYER_KHRONOS_validation"
};

//--------------------------------------------------------------
// helper: query loader supported instance api version and clamp
//--------------------------------------------------------------
static uint32_t QueryLoaderInstanceApiVersion()
{
    uint32_t supported = VK_API_VERSION_1_0;

    auto fp = (PFN_vkEnumerateInstanceVersion)vkGetInstanceProcAddr(
        VK_NULL_HANDLE, "vkEnumerateInstanceVersion");

    if (fp)
    {
        fp(&supported);
    }

    return supported;
}

static uint32_t ChooseInstanceApiVersion(uint32_t want)
{
    const uint32_t supported = QueryLoaderInstanceApiVersion();
    return (want <= supported) ? want : supported;
}

static void PrintApiVersion(const char* label, uint32_t v)
{
    const uint32_t major = VK_API_VERSION_MAJOR(v);
    const uint32_t minor = VK_API_VERSION_MINOR(v);
    const uint32_t patch = VK_API_VERSION_PATCH(v);
    std::cerr << label << major << "." << minor << "." << patch << "\n";
}

//--------------------------------------------------------------
// ctor
//--------------------------------------------------------------
VKRenderer::VKRenderer()
: IRenderer()
{
}

//--------------------------------------------------------------
// Initialize
//--------------------------------------------------------------
bool VKRenderer::Initialize(const Application* app)
{
    //==========================================================================
    // 0) basic checks / window
    //==========================================================================
    if (!app)
    {
        std::cerr << "[VKRenderer] Initialize failed: app is null\n";
        return false;
    }

    mWindow = app->GetSDLWindow(); // non-owning
    if (!mWindow)
    {
        std::cerr << "[VKRenderer] Initialize failed: SDL window is null\n";
        return false;
    }

    int pixelW = 0;
    int pixelH = 0;
    SDL_GetWindowSizeInPixels(mWindow, &pixelW, &pixelH);

    mScreenWidth  = static_cast<float>(pixelW);
    mScreenHeight = static_cast<float>(pixelH);

    mWindowDisplayScale = SDL_GetWindowDisplayScale(mWindow);
    if (mWindowDisplayScale <= 0.0f)
    {
        mWindowDisplayScale = 1.0f;
    }

    //==========================================================================
    // 1) instance (extensions/layers filtered + apiVersion clamped)
    //==========================================================================
    Uint32 sdlExtCount = 0;
    const char* const* sdlExts = SDL_Vulkan_GetInstanceExtensions(&sdlExtCount);
    if (!sdlExts || sdlExtCount == 0)
    {
        std::cerr << "[VKRenderer] SDL_Vulkan_GetInstanceExtensions failed: "
                  << SDL_GetError() << "\n";
        return false;
    }

    // enumerate available instance extensions
    uint32_t availExtCount = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &availExtCount, nullptr);

    std::vector<VkExtensionProperties> availExts(availExtCount);
    if (availExtCount)
    {
        vkEnumerateInstanceExtensionProperties(nullptr, &availExtCount, availExts.data());
    }

    // enumerate available instance layers
    uint32_t availLayerCount = 0;
    vkEnumerateInstanceLayerProperties(&availLayerCount, nullptr);

    std::vector<VkLayerProperties> availLayers(availLayerCount);
    if (availLayerCount)
    {
        vkEnumerateInstanceLayerProperties(&availLayerCount, availLayers.data());
    }

    // decide validation enable
    mEnableValidation =
        mEnableValidation &&
        toy::vkutil::HasLayer(kValidationLayers[0], availLayers);

    if (!mEnableValidation)
    {
        std::cerr << "[VK] validation layer not found (continuing without it)\n";
    }

    // build enabled instance extensions (unique + available only)
    std::vector<const char*> instanceExts;
    instanceExts.reserve(static_cast<size_t>(sdlExtCount) + 8);

    auto PushExtUniqueIfAvailable =
        [&](const char* name)
    {
        if (!name) return;
        if (!toy::vkutil::HasInstanceExt(name, availExts)) return;

        auto it = std::find_if(
            instanceExts.begin(),
            instanceExts.end(),
            [&](const char* s)
            {
                return std::strcmp(s, name) == 0;
            });

        if (it == instanceExts.end())
        {
            instanceExts.push_back(name);
        }
    };

    // SDL required exts (filtered)
    for (Uint32 i = 0; i < sdlExtCount; ++i)
    {
        PushExtUniqueIfAvailable(sdlExts[i]);
    }

    // debug utils
    if (mEnableValidation)
    {
        PushExtUniqueIfAvailable(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    // portability enumeration (MoltenVK)
    const bool hasPortabilityEnum =
        toy::vkutil::HasInstanceExt(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME, availExts);

    if (hasPortabilityEnum)
    {
        PushExtUniqueIfAvailable(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
    }

    // decide instance api version (clamp to loader)
    // 旧コード互換で “必ず通したい” 場合は VK_API_VERSION_1_0 固定にしてOK
    const uint32_t wantApi = VK_API_VERSION_1_2;
    const uint32_t chosenApi = ChooseInstanceApiVersion(wantApi);

    VkApplicationInfo appInfo{};
    appInfo.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName   = "ToyLibGame";
    appInfo.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
    appInfo.pEngineName        = "ToyLib";
    appInfo.engineVersion      = VK_MAKE_VERSION(0, 1, 0);
    appInfo.apiVersion         = chosenApi;

    VkInstanceCreateInfo ici{};
    ici.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    ici.pApplicationInfo        = &appInfo;
    ici.enabledExtensionCount   = static_cast<uint32_t>(instanceExts.size());
    ici.ppEnabledExtensionNames = instanceExts.data();

    if (hasPortabilityEnum)
    {
        ici.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
    }

    std::vector<const char*> layers;
    VkDebugUtilsMessengerCreateInfoEXT dbgCI{};
    if (mEnableValidation)
    {
        layers.push_back(kValidationLayers[0]);
        ici.enabledLayerCount   = static_cast<uint32_t>(layers.size());
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

        // validation messenger create-info can be chained
        ici.pNext = &dbgCI;
    }

    VkResult vr = vkCreateInstance(&ici, nullptr, &mInstance);
    if (vr != VK_SUCCESS || mInstance == VK_NULL_HANDLE)
    {
        std::cerr << "[VK] vkCreateInstance failed: " << (int)vr << "\n";
        PrintApiVersion("  apiVersion requested: ", wantApi);
        PrintApiVersion("  apiVersion chosen:    ", chosenApi);

        std::cerr << "  enabled extensions:\n";
        for (auto* e : instanceExts) std::cerr << "    " << e << "\n";
        std::cerr << "  enabled layers:\n";
        for (auto* l : layers) std::cerr << "    " << l << "\n";

        return false;
    }

    if (mEnableValidation)
    {
        toy::vkutil::CreateDebugUtilsMessengerEXT(mInstance, &dbgCI, &mDebugMessenger);
    }

    //==========================================================================
    // 2) surface (SDL)
    //==========================================================================
    if (!SDL_Vulkan_CreateSurface(mWindow, mInstance, nullptr, &mSurface))
    {
        std::cerr << "[VKRenderer] SDL_Vulkan_CreateSurface failed: "
                  << SDL_GetError() << "\n";
        Shutdown();
        return false;
    }

    //==========================================================================
    // 3) select physical device
    //==========================================================================
    uint32_t gpuCount = 0;
    vr = vkEnumeratePhysicalDevices(mInstance, &gpuCount, nullptr);
    if (vr != VK_SUCCESS || gpuCount == 0)
    {
        std::cerr << "[VKRenderer] No Vulkan physical devices found. vr=" << (int)vr << "\n";
        Shutdown();
        return false;
    }

    std::vector<VkPhysicalDevice> gpus(gpuCount);
    vr = vkEnumeratePhysicalDevices(mInstance, &gpuCount, gpus.data());
    if (vr != VK_SUCCESS)
    {
        std::cerr << "[VKRenderer] vkEnumeratePhysicalDevices(list) failed. vr=" << (int)vr << "\n";
        Shutdown();
        return false;
    }

    VkPhysicalDevice best = VK_NULL_HANDLE;
    std::vector<const char*> bestDevExts;

    for (VkPhysicalDevice gpu : gpus)
    {
        // device extensions
        uint32_t deCount = 0;
        vkEnumerateDeviceExtensionProperties(gpu, nullptr, &deCount, nullptr);
        std::vector<VkExtensionProperties> de(deCount);
        if (deCount)
        {
            vkEnumerateDeviceExtensionProperties(gpu, nullptr, &deCount, de.data());
        }

        // require swapchain
        if (!toy::vkutil::HasDeviceExt(VK_KHR_SWAPCHAIN_EXTENSION_NAME, de))
        {
            continue;
        }

        // queue families
        const auto q = toy::vkutil::FindQueueFamilies(gpu, mSurface);
        if (!q.IsComplete())
        {
            continue;
        }

        // swapchain support
        const auto sc = toy::vkutil::QuerySwapchainSupport(gpu, mSurface);
        if (sc.formats.empty() || sc.presentModes.empty())
        {
            continue;
        }

        std::vector<const char*> devExts;
        devExts.reserve(4);
        devExts.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

        // portability subset (MoltenVK) - optional but safe to enable if present
        if (toy::vkutil::HasDeviceExt(VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME, de))
        {
            devExts.push_back(VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME);
        }

        best = gpu;
        bestDevExts = std::move(devExts);
        mQueueFamilyGraphics = q.graphics.value();
        mQueueFamilyPresent  = q.present.value();
        break;
    }

    if (best == VK_NULL_HANDLE)
    {
        std::cerr << "[VKRenderer] No suitable GPU found.\n";
        Shutdown();
        return false;
    }

    mPhysicalDevice   = best;
    mDeviceExtensions = std::move(bestDevExts);

    {
        VkPhysicalDeviceProperties props{};
        vkGetPhysicalDeviceProperties(mPhysicalDevice, &props);
        std::cerr << "[VKRenderer] GPU: " << props.deviceName << "\n";
    }

    //==========================================================================
    // 4) logical device + queues
    //==========================================================================
    std::set<uint32_t> uniqueFamilies = { mQueueFamilyGraphics, mQueueFamilyPresent };

    std::vector<VkDeviceQueueCreateInfo> qcis;
    qcis.reserve(uniqueFamilies.size());

    float qprio = 1.0f;
    for (uint32_t fam : uniqueFamilies)
    {
        VkDeviceQueueCreateInfo qci{};
        qci.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        qci.queueFamilyIndex = fam;
        qci.queueCount       = 1;
        qci.pQueuePriorities = &qprio;
        qcis.push_back(qci);
    }

    VkPhysicalDeviceFeatures features{};

    VkDeviceCreateInfo dci{};
    dci.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    dci.queueCreateInfoCount    = static_cast<uint32_t>(qcis.size());
    dci.pQueueCreateInfos       = qcis.data();
    dci.enabledExtensionCount   = static_cast<uint32_t>(mDeviceExtensions.size());
    dci.ppEnabledExtensionNames = mDeviceExtensions.data();
    dci.pEnabledFeatures        = &features;

    // NOTE: device layers are deprecated; keep for compatibility if you want,
    // but not required for modern loaders. We'll omit to reduce variability.

    vr = vkCreateDevice(mPhysicalDevice, &dci, nullptr, &mDevice);
    if (vr != VK_SUCCESS || mDevice == VK_NULL_HANDLE)
    {
        std::cerr << "[VKRenderer] vkCreateDevice failed: " << (int)vr << "\n";
        Shutdown();
        return false;
    }

    vkGetDeviceQueue(mDevice, mQueueFamilyGraphics, 0, &mQueueGraphics);
    vkGetDeviceQueue(mDevice, mQueueFamilyPresent,  0, &mQueuePresent);

    //==========================================================================
    // 5) swapchain + images + imageviews
    //==========================================================================
    {
        toy::vkutil::SwapchainSupport sc =
            toy::vkutil::QuerySwapchainSupport(mPhysicalDevice, mSurface);

        if (sc.formats.empty() || sc.presentModes.empty())
        {
            std::cerr << "[VKRenderer] Swapchain support incomplete.\n";
            Shutdown();
            return false;
        }

        mSwapchainFormat = toy::vkutil::ChooseSurfaceFormat(sc.formats);
        mPresentMode     = toy::vkutil::ChoosePresentMode(sc.presentModes, /*vsync*/ true);
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
        sci.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

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
            sci.queueFamilyIndexCount = 0;
            sci.pQueueFamilyIndices   = nullptr;
        }

        sci.preTransform   = sc.caps.currentTransform;
        sci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        sci.presentMode    = mPresentMode;
        sci.clipped        = VK_TRUE;
        sci.oldSwapchain   = VK_NULL_HANDLE;

        vr = vkCreateSwapchainKHR(mDevice, &sci, nullptr, &mSwapchain);
        if (vr != VK_SUCCESS || mSwapchain == VK_NULL_HANDLE)
        {
            std::cerr << "[VKRenderer] vkCreateSwapchainKHR failed: " << (int)vr << "\n";
            Shutdown();
            return false;
        }

        uint32_t scImgCount = 0;
        vr = vkGetSwapchainImagesKHR(mDevice, mSwapchain, &scImgCount, nullptr);
        if (vr != VK_SUCCESS || scImgCount == 0)
        {
            std::cerr << "[VKRenderer] vkGetSwapchainImagesKHR(count) failed: " << (int)vr << "\n";
            Shutdown();
            return false;
        }

        mSwapchainImages.resize(scImgCount, VK_NULL_HANDLE);
        vr = vkGetSwapchainImagesKHR(mDevice, mSwapchain, &scImgCount, mSwapchainImages.data());
        if (vr != VK_SUCCESS)
        {
            std::cerr << "[VKRenderer] vkGetSwapchainImagesKHR(list) failed: " << (int)vr << "\n";
            Shutdown();
            return false;
        }

        mSwapchainImageViews.resize(scImgCount, VK_NULL_HANDLE);
        for (uint32_t i = 0; i < scImgCount; ++i)
        {
            VkImageViewCreateInfo iv{};
            iv.sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            iv.image    = mSwapchainImages[i];
            iv.viewType = VK_IMAGE_VIEW_TYPE_2D;
            iv.format   = mSwapchainFormat.format;

            iv.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
            iv.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
            iv.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
            iv.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

            iv.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
            iv.subresourceRange.baseMipLevel   = 0;
            iv.subresourceRange.levelCount     = 1;
            iv.subresourceRange.baseArrayLayer = 0;
            iv.subresourceRange.layerCount     = 1;

            vr = vkCreateImageView(mDevice, &iv, nullptr, &mSwapchainImageViews[i]);
            if (vr != VK_SUCCESS || mSwapchainImageViews[i] == VK_NULL_HANDLE)
            {
                std::cerr << "[VKRenderer] vkCreateImageView failed: " << (int)vr << " (i=" << i << ")\n";
                Shutdown();
                return false;
            }
        }

        std::cerr << "[VKRenderer] Swapchain created: "
                  << scImgCount << " images, extent="
                  << mSwapchainExtent.width << "x" << mSwapchainExtent.height
                  << "\n";
    }

    //==========================================================================
    // 6) command pool + per-frame cmd + sync
    //==========================================================================
    {
        VkCommandPoolCreateInfo pci{};
        pci.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        pci.queueFamilyIndex = mQueueFamilyGraphics;
        pci.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

        vr = vkCreateCommandPool(mDevice, &pci, nullptr, &mCommandPool);
        if (vr != VK_SUCCESS || mCommandPool == VK_NULL_HANDLE)
        {
            std::cerr << "[VKRenderer] vkCreateCommandPool failed: " << (int)vr << "\n";
            Shutdown();
            return false;
        }

        const uint32_t kFrames = 2;
        mFrames.clear();
        mFrames.resize(kFrames);

        std::vector<VkCommandBuffer> cmds(kFrames, VK_NULL_HANDLE);

        VkCommandBufferAllocateInfo ai{};
        ai.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        ai.commandPool        = mCommandPool;
        ai.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        ai.commandBufferCount = kFrames;

        vr = vkAllocateCommandBuffers(mDevice, &ai, cmds.data());
        if (vr != VK_SUCCESS)
        {
            std::cerr << "[VKRenderer] vkAllocateCommandBuffers failed: " << (int)vr << "\n";
            Shutdown();
            return false;
        }

        for (uint32_t i = 0; i < kFrames; ++i)
        {
            mFrames[i].cmd = cmds[i];
        }

        VkSemaphoreCreateInfo sci{};
        sci.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkFenceCreateInfo fci{};
        fci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fci.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        for (auto& f : mFrames)
        {
            if (vkCreateSemaphore(mDevice, &sci, nullptr, &f.imageAvailable) != VK_SUCCESS ||
                vkCreateSemaphore(mDevice, &sci, nullptr, &f.renderFinished) != VK_SUCCESS ||
                vkCreateFence(mDevice, &fci, nullptr, &f.inFlight) != VK_SUCCESS)
            {
                std::cerr << "[VKRenderer] Create sync objects failed.\n";
                Shutdown();
                return false;
            }
        }
    }

    //==========================================================================
    // 6.5) RenderBackendState を確定（後続のVKリソース生成の保険）
    //==========================================================================
    RenderBackendState::Get().SetVKPhysicalDevice(mPhysicalDevice);
    RenderBackendState::Get().SetVKDevice(mDevice);
    RenderBackendState::Get().SetVKGraphicsQueue(mQueueGraphics);
    RenderBackendState::Get().SetVKCommandPool(mCommandPool);

    //==========================================================================
    // 7) render pass + framebuffers
    //==========================================================================
    if (!CreateRenderPass())
    {
        Shutdown();
        return false;
    }

    if (!CreateFramebuffers())
    {
        Shutdown();
        return false;
    }

    //==========================================================================
    // 8) pipelines (あなたの既存実装に合わせる)
    //==========================================================================
    /*if (!CreateSpritePipeline())
    {
        Shutdown();
        return false;
    }
    if (!CreateMeshPipeline())
    {
        Shutdown();
        return false;
    }
    if (!CreateSkinnedMeshPipeline())
    {
        Shutdown();
        return false;
    }
     */

    //==========================================================================
    // 9) default view/proj
    //==========================================================================
    mViewMatrix = Matrix4::CreateLookAt(
        Vector3(0, 0.5f, -3),
        Vector3(0, 0, 10),
        Vector3::UnitY
    );

    mProjectionMatrix = Matrix4::CreatePerspectiveFOV(
        Math::ToRadians(mPerspectiveFOV),
        mScreenWidth,
        mScreenHeight,
        1.0f,
        2000.0f
    );

    //==========================================================================
    // 10) resources (dummy white / common geometry)
    //==========================================================================
    /*
    if (!CreateDummyWhiteResources())
    {
        std::cerr << "[VKRenderer] DummyWhite create failed\n";
        Shutdown();
        return false;
    }
     */

    // NOTE:
    // CreateSpriteVerts() は現状 VertexArray を作るなら GL 寄り。
    // ただ “いまは動かす” を優先して呼ぶ（後でVKジオメトリに置換）
    CreateSpriteVerts();

    std::cerr << "[Renderer] VK Init Complete. "
              << "Pixels(" << pixelW << "x" << pixelH << ") "
              << "Scale="  << mWindowDisplayScale
              << " SwapchainImages=" << (int)mSwapchainImages.size()
              << std::endl;

    return true;
}

//--------------------------------------------------------------
// Shutdown
//--------------------------------------------------------------
void VKRenderer::Shutdown()
{
    // BeginFrame が成功して renderpass を開いたまま落ちないように
    mFrameBegan = false;

    if (mDevice)
    {
        vkDeviceWaitIdle(mDevice);
    }

    mPipelines.clear();
    mFullScreenQuad.reset();
    mSpriteQuad.reset();
    mSurfaceQuad.reset();

    // framebuffers
    if (mDevice)
    {
        for (auto fb : mFramebuffers)
        {
            if (fb) vkDestroyFramebuffer(mDevice, fb, nullptr);
        }
    }
    mFramebuffers.clear();

    // render pass
    if (mDevice && mRenderPass)
    {
        vkDestroyRenderPass(mDevice, mRenderPass, nullptr);
        mRenderPass = VK_NULL_HANDLE;
    }

    // sync objects
    for (auto& f : mFrames)
    {
        if (mDevice)
        {
            if (f.imageAvailable) vkDestroySemaphore(mDevice, f.imageAvailable, nullptr);
            if (f.renderFinished) vkDestroySemaphore(mDevice, f.renderFinished, nullptr);
            if (f.inFlight)       vkDestroyFence(mDevice, f.inFlight, nullptr);
        }

        f.imageAvailable = VK_NULL_HANDLE;
        f.renderFinished = VK_NULL_HANDLE;
        f.inFlight       = VK_NULL_HANDLE;
        f.cmd            = VK_NULL_HANDLE;
    }
    mFrames.clear();

    // command pool (also frees command buffers)
    if (mDevice && mCommandPool)
    {
        vkDestroyCommandPool(mDevice, mCommandPool, nullptr);
        mCommandPool = VK_NULL_HANDLE;
    }

    // swapchain image views
    if (mDevice)
    {
        for (auto v : mSwapchainImageViews)
        {
            if (v) vkDestroyImageView(mDevice, v, nullptr);
        }
    }
    mSwapchainImageViews.clear();
    mSwapchainImages.clear();

    // swapchain
    if (mDevice && mSwapchain)
    {
        vkDestroySwapchainKHR(mDevice, mSwapchain, nullptr);
        mSwapchain = VK_NULL_HANDLE;
    }

    // device
    if (mDevice)
    {
        vkDestroyDevice(mDevice, nullptr);
        mDevice = VK_NULL_HANDLE;
    }

    // surface
    if (mSurface && mInstance)
    {
        vkDestroySurfaceKHR(mInstance, mSurface, nullptr);
        mSurface = VK_NULL_HANDLE;
    }

    // debug messenger
    if (mEnableValidation && mDebugMessenger && mInstance)
    {
        toy::vkutil::DestroyDebugUtilsMessengerEXT(mInstance, mDebugMessenger);
        mDebugMessenger = VK_NULL_HANDLE;
    }

    // instance
    if (mInstance)
    {
        vkDestroyInstance(mInstance, nullptr);
        mInstance = VK_NULL_HANDLE;
    }

    // reset misc
    mPhysicalDevice = VK_NULL_HANDLE;
    mQueueGraphics  = VK_NULL_HANDLE;
    mQueuePresent   = VK_NULL_HANDLE;

    mQueueFamilyGraphics = UINT32_MAX;
    mQueueFamilyPresent  = UINT32_MAX;

    mDeviceExtensions.clear();

    mSwapchainFormat = VkSurfaceFormatKHR{};
    mPresentMode     = VK_PRESENT_MODE_FIFO_KHR;
    mSwapchainExtent = VkExtent2D{};

    mImageIndex = 0;
    mFrameIndex = 0;

    mNeedRecreateSwapchain = false;
}

//--------------------------------------------------------------
// WaitIdle
//--------------------------------------------------------------
void VKRenderer::WaitIdle()
{
    if (mDevice == VK_NULL_HANDLE)
    {
        mDevice = (VkDevice)RenderBackendState::Get().GetVKDevice();
    }

    if (mDevice != VK_NULL_HANDLE)
    {
        vkDeviceWaitIdle(mDevice);
    }
}

//--------------------------------------------------------------
// BeginFrame
//--------------------------------------------------------------
bool VKRenderer::BeginFrame()
{
    if (!mDevice || !mSwapchain || mFrames.empty())
    {
        return false;
    }

    if (mNeedRecreateSwapchain)
    {
        // ここでは作り直しをしない設計なら、外側が処理する
        return false;
    }

    if (mRenderPass == VK_NULL_HANDLE || mFramebuffers.empty())
    {
        return false;
    }

    FrameSync& frame = mFrames[mFrameIndex];

    vkWaitForFences(mDevice, 1, &frame.inFlight, VK_TRUE, UINT64_MAX);
    vkResetFences(mDevice, 1, &frame.inFlight);

    VkResult ar = vkAcquireNextImageKHR(
        mDevice,
        mSwapchain,
        UINT64_MAX,
        frame.imageAvailable,
        VK_NULL_HANDLE,
        &mImageIndex);

    if (ar == VK_ERROR_OUT_OF_DATE_KHR)
    {
        mNeedRecreateSwapchain = true;
        return false;
    }

    if (ar != VK_SUCCESS && ar != VK_SUBOPTIMAL_KHR)
    {
        std::cerr << "[VKRenderer] Acquire failed: " << (int)ar << "\n";
        return false;
    }

    if (mImageIndex >= mFramebuffers.size())
    {
        return false;
    }

    vkResetCommandBuffer(frame.cmd, 0);

    VkCommandBufferBeginInfo cbBegin{};
    cbBegin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    VkResult br = vkBeginCommandBuffer(frame.cmd, &cbBegin);
    if (br != VK_SUCCESS)
    {
        std::cerr << "[VKRenderer] vkBeginCommandBuffer failed: " << (int)br << "\n";
        return false;
    }

    VkClearValue clear{};
    clear.color.float32[0] = mClearColor.x;
    clear.color.float32[1] = mClearColor.y;
    clear.color.float32[2] = mClearColor.z;
    clear.color.float32[3] = 1.0f;

    VkRenderPassBeginInfo rpBegin{};
    rpBegin.sType       = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rpBegin.renderPass  = mRenderPass;
    rpBegin.framebuffer = mFramebuffers[mImageIndex];
    rpBegin.renderArea.offset = { 0, 0 };
    rpBegin.renderArea.extent = mSwapchainExtent;
    rpBegin.clearValueCount   = 1;
    rpBegin.pClearValues      = &clear;

    vkCmdBeginRenderPass(frame.cmd, &rpBegin, VK_SUBPASS_CONTENTS_INLINE);

    // dynamic viewport/scissor
    VkViewport vp{};
    vp.x        = 0.0f;
    vp.y        = 0.0f;
    vp.width    = (float)mSwapchainExtent.width;
    vp.height   = (float)mSwapchainExtent.height;
    vp.minDepth = 0.0f;
    vp.maxDepth = 1.0f;
    vkCmdSetViewport(frame.cmd, 0, 1, &vp);

    VkRect2D sc{};
    sc.offset = { 0, 0 };
    sc.extent = mSwapchainExtent;
    vkCmdSetScissor(frame.cmd, 0, 1, &sc);

    mFrameBegan = true;
    return true;
}

//--------------------------------------------------------------
// EndFrame
//--------------------------------------------------------------
void VKRenderer::EndFrame()
{
    if (!mDevice || !mSwapchain || mFrames.empty())
    {
        return;
    }

    if (!mFrameBegan)
    {
        return;
    }

    FrameSync& frame = mFrames[mFrameIndex];

    vkCmdEndRenderPass(frame.cmd);

    VkResult er = vkEndCommandBuffer(frame.cmd);
    if (er != VK_SUCCESS)
    {
        std::cerr << "[VKRenderer] vkEndCommandBuffer failed: " << (int)er << "\n";
        mFrameBegan = false;
        return;
    }

    VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    VkSubmitInfo submit{};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.waitSemaphoreCount   = 1;
    submit.pWaitSemaphores      = &frame.imageAvailable;
    submit.pWaitDstStageMask    = &waitStage;
    submit.commandBufferCount   = 1;
    submit.pCommandBuffers      = &frame.cmd;
    submit.signalSemaphoreCount = 1;
    submit.pSignalSemaphores    = &frame.renderFinished;

    VkResult sr = vkQueueSubmit(mQueueGraphics, 1, &submit, frame.inFlight);
    if (sr != VK_SUCCESS)
    {
        std::cerr << "[VKRenderer] vkQueueSubmit failed: " << (int)sr << "\n";
        mFrameBegan = false;
        return;
    }

    VkPresentInfoKHR present{};
    present.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present.waitSemaphoreCount = 1;
    present.pWaitSemaphores    = &frame.renderFinished;
    present.swapchainCount     = 1;
    present.pSwapchains        = &mSwapchain;
    present.pImageIndices      = &mImageIndex;

    VkResult pr = vkQueuePresentKHR(mQueuePresent, &present);

    if (pr == VK_ERROR_OUT_OF_DATE_KHR || pr == VK_SUBOPTIMAL_KHR)
    {
        mNeedRecreateSwapchain = true;
    }
    else if (pr != VK_SUCCESS)
    {
        std::cerr << "[VKRenderer] vkQueuePresentKHR failed: " << (int)pr << "\n";
    }

    mFrameIndex = (mFrameIndex + 1) % static_cast<uint32_t>(mFrames.size());
    mFrameBegan = false;
}

//--------------------------------------------------------------
// CreateRenderPass (swapchain dependent)
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

    VkAttachmentDescription color{};
    color.format         = mSwapchainFormat.format;
    color.samples        = VK_SAMPLE_COUNT_1_BIT;
    color.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
    color.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    color.finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorRef{};
    colorRef.attachment = 0;
    colorRef.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments    = &colorRef;

    VkSubpassDependency dep{};
    dep.srcSubpass    = VK_SUBPASS_EXTERNAL;
    dep.dstSubpass    = 0;
    dep.srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo rpci{};
    rpci.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rpci.attachmentCount = 1;
    rpci.pAttachments    = &color;
    rpci.subpassCount    = 1;
    rpci.pSubpasses      = &subpass;
    rpci.dependencyCount = 1;
    rpci.pDependencies   = &dep;

    VkResult vr = vkCreateRenderPass(mDevice, &rpci, nullptr, &mRenderPass);
    if (vr != VK_SUCCESS || mRenderPass == VK_NULL_HANDLE)
    {
        std::cerr << "[VKRenderer] vkCreateRenderPass failed: " << (int)vr << "\n";
        return false;
    }

    return true;
}

//--------------------------------------------------------------
// CreateFramebuffers (swapchain dependent)
//--------------------------------------------------------------
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

    for (auto fb : mFramebuffers)
    {
        if (fb) vkDestroyFramebuffer(mDevice, fb, nullptr);
    }
    mFramebuffers.clear();

    mFramebuffers.resize(mSwapchainImageViews.size(), VK_NULL_HANDLE);

    for (size_t i = 0; i < mSwapchainImageViews.size(); ++i)
    {
        VkImageView attachments[] = { mSwapchainImageViews[i] };

        VkFramebufferCreateInfo fci{};
        fci.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fci.renderPass      = mRenderPass;
        fci.attachmentCount = 1;
        fci.pAttachments    = attachments;
        fci.width           = mSwapchainExtent.width;
        fci.height          = mSwapchainExtent.height;
        fci.layers          = 1;

        VkResult vr = vkCreateFramebuffer(mDevice, &fci, nullptr, &mFramebuffers[i]);
        if (vr != VK_SUCCESS || mFramebuffers[i] == VK_NULL_HANDLE)
        {
            std::cerr << "[VKRenderer] vkCreateFramebuffer failed: " << (int)vr
                      << " (i=" << i << ")\n";
            return false;
        }
    }

    return true;
}

//--------------------------------------------------------------
// Pipeline handle lookup
//--------------------------------------------------------------
PipelineHandle VKRenderer::GetPipelineHandle(const std::string& name)
{
    PipelineHandle h{};
    h.backend = PipelineBackend::VK;

    auto it = mPipelines.find(name);
    if (it == mPipelines.end())
    {
        return {};
    }

    h.ptrVKPipeline = it->second.get();
    return h;
}

//==============================================================================
// DrawItem (moved from VKRenderer_Drawpass.cpp)
//==============================================================================
void VKRenderer::DrawItem(const RenderItem& it, RenderPass pass, int cascadeIndex)
{
    (void)cascadeIndex;

    if (it.pass != pass)
    {
        return;
    }

    if (pass == RenderPass::World)
    {
        // World は “World専用経路” に一本化（現状は未実装）
        (void)it;
        return;
    }

    // UI/Sprite等は既存の経路へ（あなたの実装に合わせて）
}

} // namespace toy
