//======================================================================
// VKRenderer.cpp
//  - SDL3 + Vulkan
//  - Swapchain + Depth (Z enabled)
//  - RTT via VKSceneRenderTarget
//======================================================================

#include "Render/VK/VKRenderer.h"

#include "Engine/Core/Application.h"
#include "Render/RenderBackendState.h"

#include "Render/VK/VKSceneRenderTarget.h"

// util (あなたのVKUtilに合わせて適宜置換してOK)
#include "Render/VK/VKUtil.h"

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

    // IRenderer 側の protected mWindow を使う（前提どおり）
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

    // 1) Instance
    if (!CreateInstance())
    {
        Shutdown();
        return false;
    }

    // 2) Surface
    if (!CreateSurface())
    {
        Shutdown();
        return false;
    }

    // 3) Physical device
    if (!PickPhysicalDevice())
    {
        Shutdown();
        return false;
    }

    // 4) Logical device + queues
    if (!CreateDeviceAndQueues())
    {
        Shutdown();
        return false;
    }

    // ★ RenderBackendState は「VKリソース生成の前」に必ずセット
    RenderBackendState::Get().SetVKPhysicalDevice(mPhysicalDevice);
    RenderBackendState::Get().SetVKDevice(mDevice);
    RenderBackendState::Get().SetVKGraphicsQueue(mQueueGraphics);

    // 5) Swapchain + Views
    if (!CreateSwapchainAndViews())
    {
        Shutdown();
        return false;
    }

    // 6) Depth for swapchain
    if (!CreateDepthForSwapchain())
    {
        Shutdown();
        return false;
    }

    // 7) RenderPass + Framebuffers (swapchain)
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

    // 8) CommandPool + per-frame command buffers
    if (!CreateCommandPoolAndBuffers())
    {
        Shutdown();
        return false;
    }

    RenderBackendState::Get().SetVKCommandPool(mCommandPool);

    // 9) Sync
    if (!CreateSyncObjects())
    {
        Shutdown();
        return false;
    }

    // 10) Common geometry (既存経路)
    CreateSpriteVerts();
    CreateFullScreenQuad();
    CreateSurfaceQuad();

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

    // IRenderer resources
    mFullScreenQuad.reset();
    mSpriteQuad.reset();
    mSurfaceQuad.reset();

    // Sync objects
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

    // Device
    if (mDevice)
    {
        vkDestroyDevice(mDevice, nullptr);
        mDevice = VK_NULL_HANDLE;
    }

    // Surface
    if (mInstance && mSurface)
    {
        vkDestroySurfaceKHR(mInstance, mSurface, nullptr);
        mSurface = VK_NULL_HANDLE;
    }

    // Debug messenger (optional)
    if (mEnableValidation && mDebugMessenger && mInstance)
    {
        toy::vkutil::DestroyDebugUtilsMessengerEXT(mInstance, mDebugMessenger);
        mDebugMessenger = VK_NULL_HANDLE;
    }

    // Instance
    if (mInstance)
    {
        vkDestroyInstance(mInstance, nullptr);
        mInstance = VK_NULL_HANDLE;
    }

    // reset
    mPhysicalDevice = VK_NULL_HANDLE;
    mQueueGraphics  = VK_NULL_HANDLE;
    mQueuePresent   = VK_NULL_HANDLE;
    mQueueFamilyGraphics = UINT32_MAX;
    mQueueFamilyPresent  = UINT32_MAX;

    mNeedRecreateSwapchain = false;
    mFrameIndex = 0;
    mImageIndex = 0;
}

void VKRenderer::WaitIdle()
{
    if (mDevice != VK_NULL_HANDLE)
    {
        vkDeviceWaitIdle(mDevice);
    }
}

//--------------------------------------------------------------
// RTT: CreateRenderTarget
//--------------------------------------------------------------
std::shared_ptr<IRenderTarget> VKRenderer::CreateRenderTarget()
{
    // VK版 RTT
    return std::make_shared<VKSceneRenderTarget>();
}

//--------------------------------------------------------------
// RTT: DrawToRenderTarget
//  - まずは安全に「別submit」で描く（ビルド通し優先）
//  - 後で同一フレーム内に統合可能
//--------------------------------------------------------------
void VKRenderer::DrawToRenderTarget(const SceneCaptureRequest& req)
{
    if (!req.rt)
    {
        return;
    }

    auto* vkrt = dynamic_cast<VKSceneRenderTarget*>(req.rt.get());
    if (!vkrt)
    {
        // GLRenderTarget 等が来たらここでは何もしない
        return;
    }

    if (mDevice == VK_NULL_HANDLE || mQueueGraphics == VK_NULL_HANDLE || mCommandPool == VK_NULL_HANDLE)
    {
        return;
    }

    // 作成されていなければ Create（サイズは仮に screen と同じにしておく）
    if (vkrt->GetWidth() <= 0 || vkrt->GetHeight() <= 0 ||
        vkrt->GetFramebuffer() == VK_NULL_HANDLE)
    {
        const int w = (int)mScreenWidth;
        const int h = (int)mScreenHeight;
        if (!vkrt->Create(w, h))
        {
            std::cerr << "[VKRenderer] DrawToRenderTarget: vkrt->Create failed.\n";
            return;
        }
    }

    VkCommandBuffer cmd = BeginOneTimeCommands();
    if (cmd == VK_NULL_HANDLE)
    {
        return;
    }

    // camera override
    PushCameraState();
    {
        CameraState s{};
        s.view     = req.view;
        s.proj     = req.proj;
        s.invView  = req.view; // note: inv not required for minimal
        s.invView.Invert();
        SetCameraState(s);
        mViewProjMatrix = mViewMatrix * mProjectionMatrix;
    }

    // Begin RenderPass (RTT)
    VkClearValue clears[2]{};
    clears[0].color.float32[0] = mClearColor.x;
    clears[0].color.float32[1] = mClearColor.y;
    clears[0].color.float32[2] = mClearColor.z;
    clears[0].color.float32[3] = 1.0f;

    clears[1].depthStencil.depth   = 1.0f;
    clears[1].depthStencil.stencil = 0;

    VkRenderPassBeginInfo rp{};
    rp.sType       = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rp.renderPass  = vkrt->GetRenderPass();
    rp.framebuffer = vkrt->GetFramebuffer();
    rp.renderArea.offset = { 0, 0 };
    rp.renderArea.extent = vkrt->GetExtent();
    rp.clearValueCount   = 2;
    rp.pClearValues      = clears;

    vkCmdBeginRenderPass(cmd, &rp, VK_SUBPASS_CONTENTS_INLINE);

    // viewport/scissor
    VkViewport vp{};
    vp.x = 0.0f;
    vp.y = 0.0f;
    vp.width  = (float)rp.renderArea.extent.width;
    vp.height = (float)rp.renderArea.extent.height;
    vp.minDepth = 0.0f;
    vp.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &vp);

    VkRect2D sc{};
    sc.offset = { 0, 0 };
    sc.extent = rp.renderArea.extent;
    vkCmdSetScissor(cmd, 0, 1, &sc);

    // TODO: ここで bucket を回して DrawItem(...) を呼ぶ
    // - 今は「ビルド通し」なので、clear だけでもOK
    // - req.drawSky / req.drawWorld / req.drawUI に応じて bucketを分ける想定

    vkCmdEndRenderPass(cmd);

    // camera restore
    PopCameraState();

    EndOneTimeCommands(cmd);

    // Debug counter: RTT 側に切り替えて加算したいなら
    // ChangeDebugRTT(); AddDrawCall(); ChangeDebugOnScreen();
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

    VkCommandBufferBeginInfo bi{};
    bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    vkBeginCommandBuffer(frame.cmd, &bi);

    // swapchain renderpass begin（Depthあり）
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

    // dynamic viewport/scissor
    VkViewport vp{};
    vp.x = 0.0f;
    vp.y = 0.0f;
    vp.width  = (float)mSwapchainExtent.width;
    vp.height = (float)mSwapchainExtent.height;
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

//--------------------------------------------------------------
// Draw phases (今は clear-only / 既存に合わせて拡張)
//--------------------------------------------------------------
void VKRenderer::DrawShadowPass() {}
void VKRenderer::RestoreAfterShadowPass() {}
void VKRenderer::DrawSkyPass() {}

void VKRenderer::DrawWorldPass()
{
    // clear-only: ここで bucket を回して描画していく
}

void VKRenderer::DrawOverlayScreenPass() {}
void VKRenderer::DrawFadePass() {}
void VKRenderer::DrawPostEffectPass() {}
void VKRenderer::DrawUIPass() {}

//--------------------------------------------------------------
// DrawItem (bucketed draw path)
//--------------------------------------------------------------
void VKRenderer::DrawItem(const RenderItem& it, RenderPass pass, int cascadeIndex)
{
    (void)it;
    (void)pass;
    (void)cascadeIndex;

    // 次の Step で VKPipeline を入れて実装していく
}

//======================================================================
// Vulkan init steps
//======================================================================
bool VKRenderer::CreateInstance()
{
    // SDL required instance extensions
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

    // enumerate available
    uint32_t availExtCount = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &availExtCount, nullptr);
    std::vector<VkExtensionProperties> availExts(availExtCount);
    if (availExtCount)
    {
        vkEnumerateInstanceExtensionProperties(nullptr, &availExtCount, availExts.data());
    }

    uint32_t availLayerCount = 0;
    vkEnumerateInstanceLayerProperties(&availLayerCount, nullptr);
    std::vector<VkLayerProperties> availLayers(availLayerCount);
    if (availLayerCount)
    {
        vkEnumerateInstanceLayerProperties(&availLayerCount, availLayers.data());
    }

    // validation
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

    // portability enum (MoltenVK)
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
        // queue families
        const auto q = toy::vkutil::FindQueueFamilies(dev, mSurface);
        if (!q.IsComplete()) continue;

        // device exts
        uint32_t deCount = 0;
        vkEnumerateDeviceExtensionProperties(dev, nullptr, &deCount, nullptr);
        std::vector<VkExtensionProperties> de(deCount);
        if (deCount)
        {
            vkEnumerateDeviceExtensionProperties(dev, nullptr, &deCount, de.data());
        }

        if (!toy::vkutil::HasDeviceExt(VK_KHR_SWAPCHAIN_EXTENSION_NAME, de))
        {
            continue;
        }

        // swapchain support
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

    // MoltenVK portability subset (あるなら有効化しておく)
    // ※ device extension enumerate はここでは省略（後でVKUtilで統一してOK）

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
// Depth for swapchain
//--------------------------------------------------------------
bool VKRenderer::CreateDepthForSwapchain()
{
    mDepthFormat = ChooseDepthFormat();
    if (mDepthFormat == VK_FORMAT_UNDEFINED)
    {
        std::cerr << "[VKRenderer] ChooseDepthFormat failed.\n";
        return false;
    }

    // destroy old
    if (mDepthView)  { vkDestroyImageView(mDevice, mDepthView, nullptr); mDepthView = VK_NULL_HANDLE; }
    if (mDepthImage) { vkDestroyImage(mDevice, mDepthImage, nullptr);     mDepthImage = VK_NULL_HANDLE; }
    if (mDepthMem)   { vkFreeMemory(mDevice, mDepthMem, nullptr);         mDepthMem = VK_NULL_HANDLE; }

    if (!CreateImage2D(
            mSwapchainExtent.width,
            mSwapchainExtent.height,
            mDepthFormat,
            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            mDepthImage,
            mDepthMem))
    {
        return false;
    }

    VkImageAspectFlags aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
    if (mDepthFormat == VK_FORMAT_D24_UNORM_S8_UINT ||
        mDepthFormat == VK_FORMAT_D32_SFLOAT_S8_UINT)
    {
        aspect |= VK_IMAGE_ASPECT_STENCIL_BIT;
    }

    if (!CreateImageView2D(mDepthImage, mDepthFormat, aspect, mDepthView))
    {
        return false;
    }

    return true;
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

    VkSubpassDescription sub{};
    sub.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
    sub.colorAttachmentCount    = 1;
    sub.pColorAttachments       = &colorRef;
    sub.pDepthStencilAttachment = &depthRef;

    VkSubpassDependency dep{};
    dep.srcSubpass    = VK_SUBPASS_EXTERNAL;
    dep.dstSubpass    = 0;
    dep.srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkAttachmentDescription atts[2] = { color, depth };

    VkRenderPassCreateInfo rpci{};
    rpci.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rpci.attachmentCount = 2;
    rpci.pAttachments    = atts;
    rpci.subpassCount    = 1;
    rpci.pSubpasses      = &sub;
    rpci.dependencyCount = 1;
    rpci.pDependencies   = &dep;

    VkResult vr = vkCreateRenderPass(mDevice, &rpci, nullptr, &mRenderPass);
    if (vr != VK_SUCCESS)
    {
        std::cerr << "[VKRenderer] vkCreateRenderPass failed: " << vr << "\n";
        return false;
    }

    return true;
}

bool VKRenderer::CreateFramebuffers()
{
    for (auto fb : mFramebuffers)
    {
        if (fb) vkDestroyFramebuffer(mDevice, fb, nullptr);
    }
    mFramebuffers.clear();

    mFramebuffers.resize(mSwapchainImageViews.size(), VK_NULL_HANDLE);

    for (size_t i = 0; i < mSwapchainImageViews.size(); ++i)
    {
        VkImageView atts[2] = { mSwapchainImageViews[i], mDepthView };

        VkFramebufferCreateInfo fci{};
        fci.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fci.renderPass      = mRenderPass;
        fci.attachmentCount = 2;
        fci.pAttachments    = atts;
        fci.width           = mSwapchainExtent.width;
        fci.height          = mSwapchainExtent.height;
        fci.layers          = 1;

        VkResult vr = vkCreateFramebuffer(mDevice, &fci, nullptr, &mFramebuffers[i]);
        if (vr != VK_SUCCESS)
        {
            std::cerr << "[VKRenderer] vkCreateFramebuffer failed: " << vr << " (i=" << i << ")\n";
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
        return true; // minimized etc.
    }

    vkDeviceWaitIdle(mDevice);

    CleanupSwapchain();

    if (!CreateSwapchainAndViews()) return false;
    if (!CreateDepthForSwapchain()) return false;

    // renderpass は swapchain format に依存 → 作り直すのが安全
    if (mRenderPass)
    {
        vkDestroyRenderPass(mDevice, mRenderPass, nullptr);
        mRenderPass = VK_NULL_HANDLE;
    }
    if (!CreateRenderPass()) return false;

    if (!CreateFramebuffers()) return false;

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

    // depth
    if (mDepthView)  { vkDestroyImageView(mDevice, mDepthView, nullptr); mDepthView = VK_NULL_HANDLE; }
    if (mDepthImage) { vkDestroyImage(mDevice, mDepthImage, nullptr);     mDepthImage = VK_NULL_HANDLE; }
    if (mDepthMem)   { vkFreeMemory(mDevice, mDepthMem, nullptr);         mDepthMem = VK_NULL_HANDLE; }

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
    vkEndCommandBuffer(cmd);

    VkSubmitInfo si{};
    si.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    si.commandBufferCount = 1;
    si.pCommandBuffers    = &cmd;

    vkQueueSubmit(mQueueGraphics, 1, &si, VK_NULL_HANDLE);
    vkQueueWaitIdle(mQueueGraphics);

    vkFreeCommandBuffers(mDevice, mCommandPool, 1, &cmd);
}

//--------------------------------------------------------------
// Format / Memory helpers
//--------------------------------------------------------------
VkFormat VKRenderer::ChooseDepthFormat() const
{
    const VkFormat candidates[] =
    {
        VK_FORMAT_D24_UNORM_S8_UINT,
        VK_FORMAT_D32_SFLOAT,
        VK_FORMAT_D32_SFLOAT_S8_UINT
    };

    for (VkFormat fmt : candidates)
    {
        VkFormatProperties fp{};
        vkGetPhysicalDeviceFormatProperties(mPhysicalDevice, fmt, &fp);
        if ((fp.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0)
        {
            return fmt;
        }
    }
    return VK_FORMAT_UNDEFINED;
}

uint32_t VKRenderer::FindMemoryType(uint32_t typeBits, VkMemoryPropertyFlags props) const
{
    VkPhysicalDeviceMemoryProperties mp{};
    vkGetPhysicalDeviceMemoryProperties(mPhysicalDevice, &mp);

    for (uint32_t i = 0; i < mp.memoryTypeCount; ++i)
    {
        const bool typeOk = (typeBits & (1u << i)) != 0;
        const bool propOk = (mp.memoryTypes[i].propertyFlags & props) == props;
        if (typeOk && propOk)
        {
            return i;
        }
    }
    return UINT32_MAX;
}

bool VKRenderer::CreateImage2D(
    uint32_t w,
    uint32_t h,
    VkFormat format,
    VkImageUsageFlags usage,
    VkMemoryPropertyFlags memProps,
    VkImage& outImage,
    VkDeviceMemory& outMem)
{
    VkImageCreateInfo ici{};
    ici.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    ici.imageType     = VK_IMAGE_TYPE_2D;
    ici.format        = format;
    ici.extent.width  = w;
    ici.extent.height = h;
    ici.extent.depth  = 1;
    ici.mipLevels     = 1;
    ici.arrayLayers   = 1;
    ici.samples       = VK_SAMPLE_COUNT_1_BIT;
    ici.tiling        = VK_IMAGE_TILING_OPTIMAL;
    ici.usage         = usage;
    ici.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
    ici.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VkResult vr = vkCreateImage(mDevice, &ici, nullptr, &outImage);
    if (vr != VK_SUCCESS)
    {
        std::cerr << "[VKRenderer] vkCreateImage failed: " << vr << "\n";
        return false;
    }

    VkMemoryRequirements req{};
    vkGetImageMemoryRequirements(mDevice, outImage, &req);

    VkMemoryAllocateInfo mai{};
    mai.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    mai.allocationSize  = req.size;
    mai.memoryTypeIndex = FindMemoryType(req.memoryTypeBits, memProps);
    if (mai.memoryTypeIndex == UINT32_MAX)
    {
        std::cerr << "[VKRenderer] FindMemoryType failed.\n";
        return false;
    }

    vr = vkAllocateMemory(mDevice, &mai, nullptr, &outMem);
    if (vr != VK_SUCCESS)
    {
        std::cerr << "[VKRenderer] vkAllocateMemory failed: " << vr << "\n";
        return false;
    }

    vr = vkBindImageMemory(mDevice, outImage, outMem, 0);
    if (vr != VK_SUCCESS)
    {
        std::cerr << "[VKRenderer] vkBindImageMemory failed: " << vr << "\n";
        return false;
    }

    return true;
}

bool VKRenderer::CreateImageView2D(
    VkImage image,
    VkFormat format,
    VkImageAspectFlags aspect,
    VkImageView& outView)
{
    VkImageViewCreateInfo iv{};
    iv.sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    iv.image    = image;
    iv.viewType = VK_IMAGE_VIEW_TYPE_2D;
    iv.format   = format;
    iv.subresourceRange.aspectMask     = aspect;
    iv.subresourceRange.baseMipLevel   = 0;
    iv.subresourceRange.levelCount     = 1;
    iv.subresourceRange.baseArrayLayer = 0;
    iv.subresourceRange.layerCount     = 1;

    VkResult vr = vkCreateImageView(mDevice, &iv, nullptr, &outView);
    if (vr != VK_SUCCESS)
    {
        std::cerr << "[VKRenderer] vkCreateImageView failed: " << vr << "\n";
        return false;
    }
    return true;
}

PipelineHandle VKRenderer::GetPipelineHandle(const std::string& name)
{
    (void)name;
    return {}; // 次の Step: VKPipeline を入れて返す
}

} // namespace toy
