//======================================================================
// Render/VK/VKRenderer.h  (整理版：Core/Drawpass分割に合わせて統一)
//  - swapchain depth は CreateDepthForSwapchain/DestroyDepthForSwapchain のみ
//  - ChooseDepthFormat/FindMemoryType/CreateImage2D/CreateImageView2D は vkutil に寄せる
//  - Sprite(Texture) 表示までの最低限 descriptor を保持
//======================================================================
#pragma once

#include "Render/IRenderer.h"
#include "Render/VK/Pipeline/VKPipelineLibrary.h"
#include "Asset/Material/Texture.h"

#include <vulkan/vulkan.h>
#include <SDL3/SDL.h>

#include <memory>
#include <vector>
#include <string>
#include <cstdint>
#include <unordered_map>

namespace toy
{

class Application;

struct RenderItem;
enum class RenderPass;

class VKRenderer final : public IRenderer
{
public:
    VKRenderer();
    ~VKRenderer() override;

    bool Initialize(const Application* app) override;
    void Shutdown() override;

    void WaitIdle() override;

    std::shared_ptr<IRenderTarget> CreateRenderTarget() override;
    void DrawToRenderTarget(const SceneCaptureRequest& req) override;

    void OnWindowResized(int pixelW, int pixelH) override;

    bool BeginFrame() override;
    void EndFrame() override;

    // draw phases
    void DrawShadowPass() override;
    void RestoreAfterShadowPass() override;
    void DrawSkyPass() override;
    void DrawWorldPass() override;
    void DrawOverlayScreenPass() override;
    void DrawFadePass() override;
    void DrawPostEffectPass() override;
    void DrawUIPass() override;

    void DrawItem(const RenderItem& it, RenderPass pass, int cascadeIndex) override;

    PipelineHandle GetPipelineHandle(const std::string& name) override;

private:
    //==============================================================
    // Core: init steps
    //==============================================================
    bool CreateInstance();
    bool CreateSurface();
    bool PickPhysicalDevice();
    bool CreateDeviceAndQueues();
    bool CreateSwapchainAndViews();

    bool CreateDepthForSwapchain();
    void DestroyDepthForSwapchain();

    bool CreateRenderPass();
    bool CreateFramebuffers();
    bool CreateCommandPoolAndBuffers();
    bool CreateSyncObjects();

    bool RecreateSwapchain();
    void CleanupSwapchain();

    // one-time command helpers（安全版：submitだけ待つ想定に拡張しやすい）
    VkCommandBuffer BeginOneTimeCommands();
    void EndOneTimeCommands(VkCommandBuffer cmd);

    // current frame helpers（Drawpass で使う）
    VkCommandBuffer GetCurrentCommandBuffer() const
    {
        if (mFrames.empty())
        {
            return VK_NULL_HANDLE;
        }
        if (mFrameIndex >= (uint32_t)mFrames.size())
        {
            return VK_NULL_HANDLE;
        }
        return mFrames[mFrameIndex].cmd;
    }

    std::string MakeVKSpvPath(const std::string& filename) const;
    VkShaderModule LoadShaderModule(const std::string& spvFile);

private:
    //==============================================================
    // Per-frame sync
    //==============================================================
    struct FrameSync
    {
        VkCommandBuffer cmd            = VK_NULL_HANDLE;
        VkSemaphore     imageAvailable = VK_NULL_HANDLE;
        VkSemaphore     renderFinished = VK_NULL_HANDLE;
        VkFence         inFlight       = VK_NULL_HANDLE;
    };

private:
    //==============================================================
    // Vulkan core handles
    //==============================================================
    VkInstance       mInstance        = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT mDebugMessenger = VK_NULL_HANDLE;

    VkSurfaceKHR     mSurface         = VK_NULL_HANDLE;

    VkPhysicalDevice mPhysicalDevice  = VK_NULL_HANDLE;
    VkDevice         mDevice          = VK_NULL_HANDLE;

    VkQueue          mQueueGraphics   = VK_NULL_HANDLE;
    VkQueue          mQueuePresent    = VK_NULL_HANDLE;
    uint32_t         mQueueFamilyGraphics = UINT32_MAX;
    uint32_t         mQueueFamilyPresent  = UINT32_MAX;

    VkSwapchainKHR               mSwapchain = VK_NULL_HANDLE;
    std::vector<VkImage>         mSwapchainImages;
    std::vector<VkImageView>     mSwapchainImageViews;
    VkSurfaceFormatKHR           mSwapchainFormat{};
    VkExtent2D                   mSwapchainExtent{};

    // swapchain render pass / FB
    VkRenderPass                 mRenderPass = VK_NULL_HANDLE;
    std::vector<VkFramebuffer>   mFramebuffers;

    // swapchain depth (single shared depth)
    VkFormat       mDepthFormat    = VK_FORMAT_UNDEFINED;
    VkImage        mDepthImage     = VK_NULL_HANDLE;
    VkDeviceMemory mDepthMemory    = VK_NULL_HANDLE;
    VkImageView    mDepthImageView = VK_NULL_HANDLE;

    // command pool + per-frame command buffers
    VkCommandPool              mCommandPool = VK_NULL_HANDLE;
    std::vector<FrameSync>     mFrames;
    uint32_t                   mFrameIndex = 0;
    uint32_t                   mImageIndex = 0;

    // state
    bool   mEnableValidation = true;
    bool   mNeedRecreateSwapchain = false;

    // window scale
    float  mWindowDisplayScale = 1.0f;

private:
    //==============================================================
    // Pipelines
    //==============================================================
    VKPipelineLibrary mPipelines;
    bool BuildDefaultPipelines();

private:
    //==============================================================
    // Descriptors (最低限：Sprite(Texture) まで)
    //==============================================================
    // Descriptor pool（Sprite の set=0/1 を確保する）
    VkDescriptorPool mDescPool = VK_NULL_HANDLE;

    bool CreateDescriptorPool();
    void DestroyDescriptorPool();

    // set=0 : Scene UBO（今は最小でOK。後で Scene/Object/Material に拡張）
    VkDescriptorSet mSceneSet = VK_NULL_HANDLE;

    // Scene UBO buffer（set=0 binding=0 を満たすために必要）
    VkBuffer        mSceneUBO     = VK_NULL_HANDLE;
    VkDeviceMemory  mSceneUBOMem  = VK_NULL_HANDLE;
    size_t          mSceneUBOSize = 0;

    bool CreateSceneUBO();
    void DestroySceneUBO();
    void UpdateSceneUBO(); // frameごとに更新（中身は後で拡張）
    void UpdateSceneUBO(const Matrix4& viewProjOverride);
    // VKRenderer.h（クラス内 public か private に追加）
    void UpdateSceneUBOFromMatrix(const Matrix4& viewProj);
    
    
    bool CreateSceneDescriptorSet(); // set=0 を確保して write

    // set=1 : Sprite Texture（Texture* をキーに descriptor set をキャッシュ）
    std::unordered_map<const Texture*, VkDescriptorSet> mSpriteTexSetCache;

    VkDescriptorSet GetOrCreateSpriteTextureSet(const Texture* tex);
    void ClearSpriteTextureSetCache();

private:
    //==============================================================
    // Small helpers (resource)
    //==============================================================
    // ※ FindMemoryType / CreateImage2D / CreateImageView2D 等は vkutil に寄せる前提
    // ここには「VKRendererが使う最小のラッパ」だけ残す

    bool CreateBufferHostVisible(VkDeviceSize size,
                                 VkBufferUsageFlags usage,
                                 VkBuffer& outBuf,
                                 VkDeviceMemory& outMem);

    bool UploadToBuffer(VkDeviceMemory mem, const void* data, VkDeviceSize size);
    
};

} // namespace toy
