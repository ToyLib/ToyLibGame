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
#include "Render/VK/Pipeline/VKPipelinePresets.h"
#include "Render/VK/VKSceneRenderTarget.h"
#include "Render/VK/VKUtil.h"

#include <SDL3/SDL_vulkan.h>
#include <vulkan/vulkan.h>

#include <algorithm>
#include <iostream>
#include <set>
#include <vector>

namespace toy
{

//--------------------------------------------------------------
// ctor/dtor
//--------------------------------------------------------------
VKRenderer::VKRenderer() : IRenderer()
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
    mScreenWidth = static_cast<float>(pixelW);
    mScreenHeight = static_cast<float>(pixelH);

    mWindowDisplayScale = SDL_GetWindowDisplayScale(mWindow);
    if (mWindowDisplayScale <= 0.0f)
    {
        mWindowDisplayScale = 1.0f;
    }

    if (!CreateInstance())
    {
        Shutdown();
        return false;
    }
    if (!CreateSurface())
    {
        Shutdown();
        return false;
    }
    if (!PickPhysicalDevice())
    {
        Shutdown();
        return false;
    }
    if (!CreateDeviceAndQueues())
    {
        Shutdown();
        return false;
    }

    // RenderBackendState は VKリソース生成の前にセット
    RenderBackendState::Get().SetVKPhysicalDevice(mPhysicalDevice);
    RenderBackendState::Get().SetVKDevice(mDevice);
    RenderBackendState::Get().SetVKGraphicsQueue(mQueueGraphics);

    if (!CreateSwapchainAndViews())
    {
        Shutdown();
        return false;
    }
    if (!CreateDepthForSwapchain())
    {
        Shutdown();
        return false;
    }
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
    if (!CreateCommandPoolAndBuffers())
    {
        Shutdown();
        return false;
    }

    RenderBackendState::Get().SetVKCommandPool(mCommandPool);

    if (!CreateSyncObjects())
    {
        Shutdown();
        return false;
    }

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
    mViewMatrix = Matrix4::CreateLookAt(Vector3(0, 0.5f, -3), Vector3(0, 0, 10), Vector3::UnitY);
    mProjectionMatrix =
        Matrix4::CreatePerspectiveFOV(Math::ToRadians(mPerspectiveFOV), mScreenWidth, mScreenHeight, 1.0f, 2000.0f);

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
    if (!CreateSkyUBO())
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

    // テクスチャアンロード通知を登録（BaseMapSetCache のダングリングポインタ対策）
    RenderBackendState::Get().SetTextureUnloadCallback(
        [this](const Texture* tex)
        {
            OnTextureUnloaded(tex);
        });

    // VKTextureGPUのVulkanハンドル破棄を遅延させるためのコールバック。
    // 即destroyすると、in-flightな別フレームのコマンドバッファがまだ
    // 参照しているDescriptorSet/Samplerを壊してしまう（テキストテクスチャの
    // 再生成などで実際にAMD実機のdevice lostを引き起こしていた）。
    RenderBackendState::Get().SetGpuHandleRetireCallback(
        [this](void* sampler, void* view, void* image, void* mem)
        {
            RetireTextureHandles(static_cast<VkSampler>(sampler), static_cast<VkImageView>(view),
                                  static_cast<VkImage>(image), static_cast<VkDeviceMemory>(mem));
        });

    std::cerr << "[VKRenderer] Init OK. Swapchain(" << mSwapchainExtent.width << "x" << mSwapchainExtent.height
              << ") Scale=" << mWindowDisplayScale << " Images=" << (int)mSwapchainImages.size() << "\n";

    return true;
}

//--------------------------------------------------------------
// Shutdown
//--------------------------------------------------------------
void VKRenderer::Shutdown()
{
    // ★mSceneCaptureQueue(IRenderer基底クラスのメンバ)はshared_ptr<IRenderTarget>を
    //   保持し得る。ここで先に空にしておかないと、VkDevice破棄後に
    //   ~IRenderer()側でRenderTargetが破棄されてvkDestroyFramebuffer等が
    //   解放済みdeviceに対して呼ばれクラッシュする（派生→基底の破棄順の問題）。
    mSceneCaptureQueue.clear();

    if (mDevice)
    {
        vkDeviceWaitIdle(mDevice);
    }

    // vkDeviceWaitIdle済みなので、遅延中の破棄は全て即座に実行してよい。
    FlushRetiredTextures(/*force=*/true);

    // コールバック解除（Texture デストラクタが Shutdown 後に走っても安全）
    RenderBackendState::Get().ClearTextureUnloadCallback();
    RenderBackendState::Get().ClearGpuHandleRetireCallback();

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
    DestroySceneUBO_Capture();
    DestroySceneUBO();
    ClearEmptySetCache();   // pool 破棄前に空セットを解放
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
            if (f.imageAvailable)
            {
                vkDestroySemaphore(mDevice, f.imageAvailable, nullptr);
            }
            if (f.inFlight)
            {
                vkDestroyFence(mDevice, f.inFlight, nullptr);
            }
        }
        f.imageAvailable = VK_NULL_HANDLE;
        f.inFlight = VK_NULL_HANDLE;
        f.cmd = VK_NULL_HANDLE;
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
    mQueueGraphics = VK_NULL_HANDLE;
    mQueuePresent = VK_NULL_HANDLE;
    mQueueFamilyGraphics = UINT32_MAX;
    mQueueFamilyPresent = UINT32_MAX;

    mNeedRecreateSwapchain = false;
    mFrameIndex = 0;
    mImageIndex = 0;

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
void VKRenderer::OnWindowResized(int width, int height)
{
    if (width <= 0 || height <= 0)
    {
        std::cerr << "[VKRenderer] OnWindowResized ignored: " << width << "x" << height << "\n";
        return;
    }

    mScreenWidth = static_cast<float>(width);
    mScreenHeight = static_cast<float>(height);
    mNeedRecreateSwapchain = true;
}

//--------------------------------------------------------------
// OnTextureUnloaded
//  Texture::Unload() → RenderBackendState 経由で呼ばれる
//  BaseMapSetCache からそのテクスチャのエントリを除去する
//--------------------------------------------------------------
void VKRenderer::OnTextureUnloaded(const Texture* tex)
{
    mBaseMapCache.RemoveTexture(tex);
}

//--------------------------------------------------------------
// Texture GPU handles: deferred destroy
//--------------------------------------------------------------
void VKRenderer::RetireTextureHandles(VkSampler sampler, VkImageView view, VkImage image, VkDeviceMemory mem)
{
    if (!mDevice)
    {
        // deviceが既に無いなら遅延させる意味が無い（待つ相手が居ない）
        if (sampler) vkDestroySampler(mDevice, sampler, nullptr);
        if (view) vkDestroyImageView(mDevice, view, nullptr);
        if (image) vkDestroyImage(mDevice, image, nullptr);
        if (mem) vkFreeMemory(mDevice, mem, nullptr);
        return;
    }

    RetiredGpuTexture r{};
    r.sampler = sampler;
    r.view = view;
    r.image = image;
    r.mem = mem;
    // mFrames.size()回分のBeginFrame()(=fence wait)を経れば、
    // このハンドルが積まれた時点で記録されていたin-flightな
    // コマンドバッファは全て完了していることが保証される。
    r.framesRemaining = static_cast<uint32_t>(mFrames.size() > 0 ? mFrames.size() : 1);

    mRetiredTextures.push_back(r);
}

void VKRenderer::FlushRetiredTextures(bool force)
{
    if (mRetiredTextures.empty())
    {
        return;
    }
    if (!mDevice)
    {
        mRetiredTextures.clear();
        return;
    }

    for (auto it = mRetiredTextures.begin(); it != mRetiredTextures.end();)
    {
        bool ready = force;
        if (!ready)
        {
            if (it->framesRemaining > 0)
            {
                --it->framesRemaining;
            }
            ready = (it->framesRemaining == 0);
        }

        if (ready)
        {
            if (it->sampler) vkDestroySampler(mDevice, it->sampler, nullptr);
            if (it->view) vkDestroyImageView(mDevice, it->view, nullptr);
            if (it->image) vkDestroyImage(mDevice, it->image, nullptr);
            if (it->mem) vkFreeMemory(mDevice, it->mem, nullptr);
            it = mRetiredTextures.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

} // namespace toy
