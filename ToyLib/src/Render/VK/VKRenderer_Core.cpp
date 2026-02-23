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
VKRenderer::VKRenderer()
    : IRenderer()
{
}

VKRenderer::~VKRenderer()
{
    Shutdown();
}

//--------------------------------------------------------------
// Initialize
//--------------------------------------------------------------
bool VKRenderer::Initialize(const Application* app)
{
    if (!app)
    {
        std::cerr << "[VKRenderer] Initialize failed: app is null\n";
        return false;
    }

    mWindow = app->GetSDLWindow();
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
    if (mWindowDisplayScale <= 0.0f) mWindowDisplayScale = 1.0f;

    if (!CreateInstance()) { Shutdown(); return false; }
    if (!CreateSurface())  { Shutdown(); return false; }
    if (!PickPhysicalDevice()) { Shutdown(); return false; }
    if (!CreateDeviceAndQueues()) { Shutdown(); return false; }

    // RenderBackendState は VKリソース生成の前にセット
    RenderBackendState::Get().SetVKPhysicalDevice(mPhysicalDevice);
    RenderBackendState::Get().SetVKDevice(mDevice);
    RenderBackendState::Get().SetVKGraphicsQueue(mQueueGraphics);

    if (!CreateSwapchainAndViews()) { Shutdown(); return false; }
    if (!CreateDepthForSwapchain()) { Shutdown(); return false; }
    if (!CreateRenderPass())        { Shutdown(); return false; }
    if (!CreateFramebuffers())      { Shutdown(); return false; }
    if (!CreateCommandPoolAndBuffers()) { Shutdown(); return false; }

    RenderBackendState::Get().SetVKCommandPool(mCommandPool);

    if (!CreateSyncObjects()) { Shutdown(); return false; }

    // common geometry (既存経路)
    CreateSpriteVerts();
    CreateFullScreenQuad();
    CreateSurfaceQuad();

    //==========================================================
    // Pipeline (RenderPass/Extent に依存するので先に作る)
    //==========================================================
    if (!BuildDefaultPipelines())
    {
        Shutdown();
        return false;
    }

    // Default view/proj
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

    //==========================================================
    // Descriptors
    //  - set=0 : Scene UBO (World/UI)
    //  - set=1 : Texture (BaseMap)
    //  - set=2 : Skinned palette UBO (slot pool)
    //==========================================================
    if (!CreateDescriptorPool())
    {
        Shutdown();
        return false;
    }
    if (!CreateSceneUBO())
    {
        Shutdown();
        return false;
    }
    if (!CreateSceneDescriptorSet())
    {
        Shutdown();
        return false;
    }

    //==========================================================
    // Skinned slots (set=2)
    //  - VKRenderer 側だけで完結（GL側・IRenderer側は触らない）
    //==========================================================
    //if (!CreateSkinnedSlots())
    {
    //    Shutdown();
    //    return false;
    }

    std::cerr << "[VKRenderer] Init OK. Swapchain("
              << mSwapchainExtent.width << "x" << mSwapchainExtent.height
              << ") Scale=" << mWindowDisplayScale
              << " Images=" << (int)mSwapchainImages.size()
              << "\n";

    return true;
}

//--------------------------------------------------------------
// Shutdown
//--------------------------------------------------------------
void VKRenderer::Shutdown()
{
    if (mDevice)
    {
        vkDeviceWaitIdle(mDevice);
    }

    //==========================================================
    // Descriptors (must be destroyed before VkDevice)
    //==========================================================
    DestroySkinnedSlots();
    DestroySceneUBO();
    DestroyDescriptorPool();

    // IRenderer resources
    mFullScreenQuad.reset();
    mSpriteQuad.reset();
    mSurfaceQuad.reset();

    mPipelines.DestroyAll();

    // Sync
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

    // Command pool
    if (mDevice && mCommandPool)
    {
        vkDestroyCommandPool(mDevice, mCommandPool, nullptr);
        mCommandPool = VK_NULL_HANDLE;
    }

    CleanupSwapchain();

    if (mDevice)
    {
        vkDestroyDevice(mDevice, nullptr);
        mDevice = VK_NULL_HANDLE;
    }

    if (mInstance && mSurface)
    {
        vkDestroySurfaceKHR(mInstance, mSurface, nullptr);
        mSurface = VK_NULL_HANDLE;
    }

    if (mEnableValidation && mDebugMessenger && mInstance)
    {
        toy::vkutil::DestroyDebugUtilsMessengerEXT(mInstance, mDebugMessenger);
        mDebugMessenger = VK_NULL_HANDLE;
    }

    if (mInstance)
    {
        vkDestroyInstance(mInstance, nullptr);
        mInstance = VK_NULL_HANDLE;
    }

    // RenderBackendState を “無効化” しておく（潜在バグ対策）
    RenderBackendState::Get().SetVKPhysicalDevice(VK_NULL_HANDLE);
    RenderBackendState::Get().SetVKDevice(VK_NULL_HANDLE);
    RenderBackendState::Get().SetVKGraphicsQueue(VK_NULL_HANDLE);
    RenderBackendState::Get().SetVKCommandPool(VK_NULL_HANDLE);

    mPhysicalDevice = VK_NULL_HANDLE;
    mQueueGraphics  = VK_NULL_HANDLE;
    mQueuePresent   = VK_NULL_HANDLE;
    mQueueFamilyGraphics = UINT32_MAX;
    mQueueFamilyPresent  = UINT32_MAX;

    mNeedRecreateSwapchain = false;
    mFrameIndex = 0;
    mImageIndex = 0;

    // scene set handle safety
    mSceneSet.clear();
    mSceneSet_UI.clear();

    // skinned slots safety
    mSkinnedSlots.clear();
    mSkinnedSlotCursor.clear();
}

void VKRenderer::WaitIdle()
{
    if (mDevice != VK_NULL_HANDLE)
    {
        vkDeviceWaitIdle(mDevice);
    }
}

//--------------------------------------------------------------
// CreateRenderTarget
//--------------------------------------------------------------
std::shared_ptr<IRenderTarget> VKRenderer::CreateRenderTarget()
{
    return std::make_shared<VKSceneRenderTarget>();
}

//--------------------------------------------------------------
// OnWindowResized
//--------------------------------------------------------------
void VKRenderer::OnWindowResized(int pixelW, int pixelH)
{
    if (pixelW <= 0 || pixelH <= 0)
    {
        return;
    }

    mScreenWidth  = (float)pixelW;
    mScreenHeight = (float)pixelH;

    mNeedRecreateSwapchain = true;
}

//--------------------------------------------------------------
// BeginFrame / EndFrame
//--------------------------------------------------------------
bool VKRenderer::BeginFrame()
{
    if (mDevice == VK_NULL_HANDLE || mSwapchain == VK_NULL_HANDLE || mFrames.empty())
    {
        return false;
    }

    if (mNeedRecreateSwapchain)
    {
        if (!RecreateSwapchain())
        {
            return false;
        }
        mNeedRecreateSwapchain = false;
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
        std::cerr << "[VKRenderer] Acquire failed: " << ar << "\n";
        return false;
    }

    vkResetCommandBuffer(frame.cmd, 0);

    // per-frame skinned slot cursor reset
    if (mSkinnedSlotCursor.size() != mFrames.size())
    {
        mSkinnedSlotCursor.resize(mFrames.size(), 0);
    }
    mSkinnedSlotCursor[mFrameIndex] = 0;

    VkCommandBufferBeginInfo bi{};
    bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    vkBeginCommandBuffer(frame.cmd, &bi);

    VkClearValue clears[2]{};
    clears[0].color.float32[0] = mClearColor.x;
    clears[0].color.float32[1] = mClearColor.y;
    clears[0].color.float32[2] = mClearColor.z;
    clears[0].color.float32[3] = 1.0f;

    clears[1].depthStencil.depth   = 1.0f;
    clears[1].depthStencil.stencil = 0;

    VkRenderPassBeginInfo rp{};
    rp.sType       = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rp.renderPass  = mRenderPass;
    rp.framebuffer = mFramebuffers[mImageIndex];
    rp.renderArea.offset = { 0, 0 };
    rp.renderArea.extent = mSwapchainExtent;
    rp.clearValueCount   = 2;
    rp.pClearValues      = clears;

    vkCmdBeginRenderPass(frame.cmd, &rp, VK_SUBPASS_CONTENTS_INLINE);

    //----------------------------------------------------------
    // ★最重要：World の SceneUBO 更新
    //----------------------------------------------------------
    UpdateSceneUBO_World();

    VkViewport vp{};
    vp.x = 0.0f;
    vp.y = (float)rp.renderArea.extent.height;
    vp.width  = (float)rp.renderArea.extent.width;
    vp.height = -(float)rp.renderArea.extent.height;
    vp.minDepth = 0.0f;
    vp.maxDepth = 1.0f;
    vkCmdSetViewport(frame.cmd, 0, 1, &vp);

    VkRect2D sc{};
    sc.offset = { 0, 0 };
    sc.extent = mSwapchainExtent;
    vkCmdSetScissor(frame.cmd, 0, 1, &sc);

    return true;
}

void VKRenderer::EndFrame()
{
    if (mDevice == VK_NULL_HANDLE || mSwapchain == VK_NULL_HANDLE || mFrames.empty())
    {
        return;
    }

    FrameSync& frame = mFrames[mFrameIndex];

    vkCmdEndRenderPass(frame.cmd);

    VkResult er = vkEndCommandBuffer(frame.cmd);
    if (er != VK_SUCCESS)
    {
        std::cerr << "[VKRenderer] vkEndCommandBuffer failed: " << er << "\n";
        return;
    }

    VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    VkSubmitInfo si{};
    si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    si.waitSemaphoreCount   = 1;
    si.pWaitSemaphores      = &frame.imageAvailable;
    si.pWaitDstStageMask    = &waitStage;
    si.commandBufferCount   = 1;
    si.pCommandBuffers      = &frame.cmd;
    si.signalSemaphoreCount = 1;
    si.pSignalSemaphores    = &frame.renderFinished;

    VkResult sr = vkQueueSubmit(mQueueGraphics, 1, &si, frame.inFlight);
    if (sr != VK_SUCCESS)
    {
        std::cerr << "[VKRenderer] vkQueueSubmit failed: " << sr << "\n";
        return;
    }

    VkPresentInfoKHR pi{};
    pi.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    pi.waitSemaphoreCount = 1;
    pi.pWaitSemaphores    = &frame.renderFinished;
    pi.swapchainCount     = 1;
    pi.pSwapchains        = &mSwapchain;
    pi.pImageIndices      = &mImageIndex;

    VkResult pr = vkQueuePresentKHR(mQueuePresent, &pi);

    if (pr == VK_ERROR_OUT_OF_DATE_KHR || pr == VK_SUBOPTIMAL_KHR)
    {
        mNeedRecreateSwapchain = true;
    }
    else if (pr != VK_SUCCESS)
    {
        std::cerr << "[VKRenderer] vkQueuePresentKHR failed: " << pr << "\n";
    }

    mFrameIndex = (mFrameIndex + 1) % (uint32_t)mFrames.size();
}

//======================================================================
// Vulkan init steps
//======================================================================
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

bool VKRenderer::CreateCommandPoolAndBuffers()
{
    VkCommandPoolCreateInfo pci{};
    pci.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pci.queueFamilyIndex = mQueueFamilyGraphics;
    pci.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    VkResult vr = vkCreateCommandPool(mDevice, &pci, nullptr, &mCommandPool);
    if (vr != VK_SUCCESS)
    {
        std::cerr << "[VKRenderer] vkCreateCommandPool failed: " << vr << "\n";
        return false;
    }

    const uint32_t kFrames = 2;
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
        std::cerr << "[VKRenderer] vkAllocateCommandBuffers failed: " << vr << "\n";
        return false;
    }

    for (uint32_t i = 0; i < kFrames; ++i)
    {
        mFrames[i].cmd = cmds[i];
    }

    return true;
}

bool VKRenderer::CreateSyncObjects()
{
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
            return false;
        }
    }

    return true;
}

//--------------------------------------------------------------
// RecreateSwapchain
//--------------------------------------------------------------
bool VKRenderer::RecreateSwapchain()
{
    int w = 0;
    int h = 0;
    SDL_GetWindowSizeInPixels(mWindow, &w, &h);
    if (w <= 0 || h <= 0)
    {
        return true;
    }

    if (mDevice == VK_NULL_HANDLE)
    {
        return false;
    }

    vkDeviceWaitIdle(mDevice);

    CleanupSwapchain();

    if (!CreateSwapchainAndViews()) return false;
    if (!CreateDepthForSwapchain()) return false;

    if (mRenderPass)
    {
        vkDestroyRenderPass(mDevice, mRenderPass, nullptr);
        mRenderPass = VK_NULL_HANDLE;
    }
    if (!CreateRenderPass()) return false;
    if (!CreateFramebuffers()) return false;

    //----------------------------------------------------------
    // Projection 更新
    //----------------------------------------------------------
    mScreenWidth  = (float)mSwapchainExtent.width;
    mScreenHeight = (float)mSwapchainExtent.height;

    mProjectionMatrix = Matrix4::CreatePerspectiveFOV(
        Math::ToRadians(mPerspectiveFOV),
        mScreenWidth,
        mScreenHeight,
        1.0f,
        2000.0f
    );

    //----------------------------------------------------------
    // Descriptor sets は Pipeline の Layout に依存するので
    // いったん解放 → Pipeline 再生成 → SceneSet 再生成
    //----------------------------------------------------------
    if (mDescPool != VK_NULL_HANDLE)
    {
        // SceneSets
        for (auto& ds : mSceneSet)
        {
            if (ds != VK_NULL_HANDLE)
            {
                vkFreeDescriptorSets(mDevice, mDescPool, 1, &ds);
                ds = VK_NULL_HANDLE;
            }
        }
        mSceneSet.clear();

        for (auto& ds : mSceneSet_UI)
        {
            if (ds != VK_NULL_HANDLE)
            {
                vkFreeDescriptorSets(mDevice, mDescPool, 1, &ds);
                ds = VK_NULL_HANDLE;
            }
        }
        mSceneSet_UI.clear();

        // BaseMap cache
        ClearBaseMapSetCache();

        // ★Skinned slot pool (layout change safety)
        DestroySkinnedSlots();
    }

    //----------------------------------------------------------
    // Pipelines rebuild
    //----------------------------------------------------------
    if (!BuildDefaultPipelines())
    {
        return false;
    }

    //----------------------------------------------------------
    // SceneSet 再生成
    //----------------------------------------------------------
    if (!CreateSceneDescriptorSet())
    {
        return false;
    }

    // Skinned slots (set=2)
    //if (!CreateSkinnedSlots())
    {
    //    return false;
    }

    //----------------------------------------------------------
    // UBO 初期更新
    //----------------------------------------------------------
    UpdateSceneUBO_World();

    return true;
}

void VKRenderer::CleanupSwapchain()
{
    if (mDevice == VK_NULL_HANDLE)
    {
        return;
    }

    for (auto fb : mFramebuffers)
    {
        if (fb) vkDestroyFramebuffer(mDevice, fb, nullptr);
    }
    mFramebuffers.clear();

    if (mRenderPass)
    {
        vkDestroyRenderPass(mDevice, mRenderPass, nullptr);
        mRenderPass = VK_NULL_HANDLE;
    }

    DestroyDepthForSwapchain();

    for (auto v : mSwapchainImageViews)
    {
        if (v) vkDestroyImageView(mDevice, v, nullptr);
    }
    mSwapchainImageViews.clear();
    mSwapchainImages.clear();

    if (mSwapchain)
    {
        vkDestroySwapchainKHR(mDevice, mSwapchain, nullptr);
        mSwapchain = VK_NULL_HANDLE;
    }
}

//--------------------------------------------------------------
// One-time command helpers
//--------------------------------------------------------------
VkCommandBuffer VKRenderer::BeginOneTimeCommands()
{
    if (!mDevice || !mCommandPool)
    {
        return VK_NULL_HANDLE;
    }

    VkCommandBufferAllocateInfo ai{};
    ai.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    ai.commandPool        = mCommandPool;
    ai.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    ai.commandBufferCount = 1;

    VkCommandBuffer cmd = VK_NULL_HANDLE;
    if (vkAllocateCommandBuffers(mDevice, &ai, &cmd) != VK_SUCCESS)
    {
        return VK_NULL_HANDLE;
    }

    VkCommandBufferBeginInfo bi{};
    bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(cmd, &bi);
    return cmd;
}

void VKRenderer::EndOneTimeCommands(VkCommandBuffer cmd)
{
    if (!cmd) return;

    vkEndCommandBuffer(cmd);

    VkFence fence = VK_NULL_HANDLE;
    VkFenceCreateInfo fci{};
    fci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    vkCreateFence(mDevice, &fci, nullptr, &fence);

    VkSubmitInfo si{};
    si.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    si.commandBufferCount = 1;
    si.pCommandBuffers    = &cmd;

    vkQueueSubmit(mQueueGraphics, 1, &si, fence);

    vkWaitForFences(mDevice, 1, &fence, VK_TRUE, UINT64_MAX);

    if (fence) vkDestroyFence(mDevice, fence, nullptr);

    vkFreeCommandBuffers(mDevice, mCommandPool, 1, &cmd);
}

bool VKRenderer::BuildDefaultPipelines()
{
    const std::string base = mShaderPath + "VK/spv/";

    //==========================================================
    // ★重要：swapchain recreate 等で呼ばれるので、古い pipeline を確実に破棄
    //==========================================================
    mPipelines.DestroyAll();

    //==========================================================
    // ★超重要：pipeline を作り直したら setLayout が変わる可能性がある
    //           → 古い DescriptorSet を使い回すと「テクスチャだけ出ない」が起きる
    //==========================================================
    ClearBaseMapSetCache();

    // Sprite（そのまま）
    {
        VKPipelineDesc sprite = toy::VKPipelinePresets::MakeSprite(base);
        if (!mPipelines.CreatePipeline("Sprite", mDevice, mRenderPass, mSwapchainExtent, sprite))
        {
            return false;
        }
    }

    //----------------------------------------------------------
    // Mesh（通常：ToyLib 標準 CCW を表）
    //----------------------------------------------------------
    {
        VKPipelineDesc mesh = toy::VKPipelinePresets::MakeMesh(base);

        // ★ここが正：CCW を表にするなら COUNTER_CLOCKWISE
        mesh.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        mesh.cullMode  = VK_CULL_MODE_BACK_BIT;

        if (!mPipelines.CreatePipeline("Mesh", mDevice, mRenderPass, mSwapchainExtent, mesh))
        {
            return false;
        }

        //------------------------------------------------------
        // Mesh_CW（裏表逆：CW を表）
        //------------------------------------------------------
        VKPipelineDesc meshCW = mesh;
        meshCW.frontFace = VK_FRONT_FACE_CLOCKWISE;

        if (!mPipelines.CreatePipeline("Mesh_CW", mDevice, mRenderPass, mSwapchainExtent, meshCW))
        {
            return false;
        }
    }

    //----------------------------------------------------------
    // SkinnedMesh（通常：CCW を表）
    //----------------------------------------------------------
    {
        VKPipelineDesc sk = toy::VKPipelinePresets::MakeSkinnedMesh(base);

        sk.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        sk.cullMode  = VK_CULL_MODE_BACK_BIT;

        if (!mPipelines.CreatePipeline("SkinnedMesh", mDevice, mRenderPass, mSwapchainExtent, sk))
        {
            return false;
        }

        //------------------------------------------------------
        // SkinnedMesh_CW（裏表逆：CW を表）
        //------------------------------------------------------
        VKPipelineDesc skCW = sk;
        skCW.frontFace = VK_FRONT_FACE_CLOCKWISE;

        if (!mPipelines.CreatePipeline("SkinnedMesh_CW", mDevice, mRenderPass, mSwapchainExtent, skCW))
        {
            return false;
        }
    }

    return true;
}

} // namespace toy
