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
    if (!CreateSceneUBO_Capture())
    {
        Shutdown();
        return false;
    }
    if (!CreateSkyUBO())
    {
        Shutdown();
        return false;
    }
    if (!CreateOverlayUBO())
    {
        Shutdown();
        return false;
    }
    if (!CreateSceneDescriptorSet())
    {
        Shutdown();
        return false;
    }
    if (!CreateSkyDescriptorSet())
    {
        Shutdown();
        return false;
    }
    if (!CreateOverlayDescriptorSet())
    {
        Shutdown();
        return false;
    }
    if (!CreatePostEffectDescriptorSets())
    {
        Shutdown();
        return false;
    }

    // Shadow
    CreateShadowResources();

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
    
    mPost.paperTex.reset();
    DestroyShadowResources();
    if (mSceneRT)
    {
        mSceneRT->Unload();
        mSceneRT.reset();
    }
    
    
    //==========================================================
    // Descriptors (must be destroyed before VkDevice)
    //==========================================================
    DestroyOverlayUBO();
    DestroySkyUBO();
    DestroySkinnedSlots();
    DestroySkyUBO();
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

} // namespace toy
