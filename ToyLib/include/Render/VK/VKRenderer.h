//======================================================================
// Render/VK/VKRenderer.h  (整理版：Core/Drawpass分割に合わせて統一)
//  - swapchain depth は CreateDepthForSwapchain/DestroyDepthForSwapchain のみ
//  - ChooseDepthFormat/FindMemoryType/CreateImage2D/CreateImageView2D は vkutil に寄せる
//  - Descriptor は Scene(set=0) + BaseMap(set=1) を最低限保持
//======================================================================
#pragma once

#include "Render/IRenderer.h"
#include "Render/VK/Pipeline/VKPipelineLibrary.h"
#include "Asset/Material/Texture.h"

// Matrix4 / Vector3 などがここにある前提（パスはプロジェクトに合わせて調整）
#include "Utils/MathUtil.h"

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

    // one-time command helpers
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
    VkInstance               mInstance        = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT mDebugMessenger  = VK_NULL_HANDLE;

    VkSurfaceKHR     mSurface        = VK_NULL_HANDLE;

    VkPhysicalDevice mPhysicalDevice = VK_NULL_HANDLE;
    VkDevice         mDevice         = VK_NULL_HANDLE;

    VkQueue          mQueueGraphics  = VK_NULL_HANDLE;
    VkQueue          mQueuePresent   = VK_NULL_HANDLE;
    uint32_t         mQueueFamilyGraphics = UINT32_MAX;
    uint32_t         mQueueFamilyPresent  = UINT32_MAX;

    VkSwapchainKHR           mSwapchain = VK_NULL_HANDLE;
    std::vector<VkImage>     mSwapchainImages;
    std::vector<VkImageView> mSwapchainImageViews;
    VkSurfaceFormatKHR       mSwapchainFormat{};
    VkExtent2D               mSwapchainExtent{};

    // swapchain render pass / FB
    VkRenderPass               mRenderPass = VK_NULL_HANDLE;
    std::vector<VkFramebuffer> mFramebuffers;

    // swapchain depth (single shared depth)
    VkFormat       mDepthFormat    = VK_FORMAT_UNDEFINED;
    VkImage        mDepthImage     = VK_NULL_HANDLE;
    VkDeviceMemory mDepthMemory    = VK_NULL_HANDLE;
    VkImageView    mDepthImageView = VK_NULL_HANDLE;

    // command pool + per-frame command buffers
    VkCommandPool          mCommandPool = VK_NULL_HANDLE;
    std::vector<FrameSync> mFrames;
    uint32_t               mFrameIndex = 0;
    uint32_t               mImageIndex = 0;

    // state
    bool   mEnableValidation     = true;
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
    // Descriptors (最低限：Sprite(Texture) + Mesh(BaseMap) まで)
    //==============================================================
    VkDescriptorPool mDescPool = VK_NULL_HANDLE;

    bool CreateDescriptorPool();
    void DestroyDescriptorPool();

    // set=0 : Scene UBO
    VkDescriptorSet mSceneSet = VK_NULL_HANDLE;

    // Scene UBO buffer（set=0 binding=0）
    VkBuffer        mSceneUBO     = VK_NULL_HANDLE;
    VkDeviceMemory  mSceneUBOMem  = VK_NULL_HANDLE;
    size_t          mSceneUBOSize = 0;

    bool CreateSceneUBO();
    void DestroySceneUBO();

    // frameごとに更新（中身は後で拡張）
    void UpdateSceneUBO();
    // 任意の viewProj を流し込む（RTTやデバッグ用途）
    void UpdateSceneUBOFromMatrix(const Matrix4& viewProj);

    bool CreateSceneDescriptorSet(); // set=0 を確保して write

    // set=1 : BaseMap(Texture) キャッシュ
    struct BaseMapKey
    {
        const Texture* tex = nullptr;
        std::string    pipe; // "Sprite" / "Mesh" / "SkinnedMesh" など

        bool operator==(const BaseMapKey& rhs) const
        {
            return tex == rhs.tex && pipe == rhs.pipe;
        }
    };

    struct BaseMapKeyHash
    {
        size_t operator()(const BaseMapKey& k) const
        {
            const size_t h1 = std::hash<const void*>{}(k.tex);
            const size_t h2 = std::hash<std::string>{}(k.pipe);
            return h1 ^ (h2 + 0x9e3779b97f4a7c15ULL + (h1 << 6) + (h1 >> 2));
        }
    };

    std::unordered_map<BaseMapKey, VkDescriptorSet, BaseMapKeyHash> mBaseMapSetCache;

    void ClearBaseMapSetCache();
    VkDescriptorSet GetOrCreateBaseMapSet(const Texture* tex, const char* pipelineName);

private:
    //==============================================================
    // Small helpers (resource)
    //==============================================================
    bool CreateBufferHostVisible(VkDeviceSize size,
                                VkBufferUsageFlags usage,
                                VkBuffer& outBuf,
                                VkDeviceMemory& outMem);

    bool UploadToBuffer(VkDeviceMemory mem, const void* data, VkDeviceSize size);

    // 既存コード互換で使ってるなら残す（Descriptors.cpp 内で使う想定）
    bool CreateBufferHostVisible(VkDeviceSize size,
                                VkBufferUsageFlags usage,
                                VkBuffer& outBuf,
                                VkDeviceMemory& outMem) const = delete;

    // ※上の delete は「誤って const 版を作らない」ための安全策
};

} // namespace toy
