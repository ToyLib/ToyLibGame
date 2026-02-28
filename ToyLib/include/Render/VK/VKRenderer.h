//======================================================================
// Render/VK/VKRenderer.h
//  - World / UI の SceneUBO & SceneSet を分離（事故らない最小構成）
//  - DrawItem は 引数 pass で SceneSet を選ぶ（RenderItemに依存しない）
//  - Skinned は “set=2 を draw ごとに切る” 方式（同一cmd内の上書き事故を回避）
//  - BaseMap(set=1) は “専用DescriptorPoolを増設” して枯れを回避
//======================================================================
#pragma once

#include "Render/IRenderer.h"
#include "Render/VK/Pipeline/VKPipelineLibrary.h"
#include "Utils/MathUtil.h"

#include <vulkan/vulkan.h>
#include <SDL3/SDL.h>

#include <vector>
#include <unordered_map>
#include <memory>
#include <string>
#include <cstdint>

namespace toy
{

class Application;
class Texture;

//======================================================================
// VKRenderer
//======================================================================
class VKRenderer : public IRenderer
{
public:
    VKRenderer();
    virtual ~VKRenderer();

    bool Initialize(const Application* app) override;
    void Shutdown() override;

    void WaitIdle() override;

    std::shared_ptr<IRenderTarget> CreateRenderTarget() override;

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

    void DrawToRenderTarget(const SceneCaptureRequest& req) override;

    void DrawItem(const RenderItem& it, RenderPass pass, int cascadeIndex) override;
    void DrawBucket_UI(const std::vector<uint32_t>& bucket);

    PipelineHandle GetPipelineHandle(const std::string& name) override;

    //==========================================================
    // Descriptors / UBO
    //==========================================================
    bool CreateDescriptorPool();
    void DestroyDescriptorPool();

    bool CreateSceneUBO();      // world + ui を両方作る
    void DestroySceneUBO();

    // ★更新は必ず「どっちに書くか」明示する（上書き事故防止）
    void UpdateSceneUBO_World();                       // mSceneUBO[frame]
    void UpdateSceneUBO_UI(const Matrix4& uiViewProj); // mSceneUBO_UI[frame]

    bool CreateSceneDescriptorSet(); // world + ui を両方作る（set=0）

    // BaseMap (set=1)
    VkDescriptorSet GetOrCreateBaseMapSet(const Texture* tex, const char* pipelineName);

private:
    //==========================================================
    // Vulkan init
    //==========================================================
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

    bool BuildDefaultPipelines();

    // one-time cmd
    VkCommandBuffer BeginOneTimeCommands();
    void EndOneTimeCommands(VkCommandBuffer cmd);

    // buffers (host-visible)
    bool CreateBufferHostVisible(VkDeviceSize size,
                                VkBufferUsageFlags usage,
                                VkBuffer& outBuf,
                                VkDeviceMemory& outMem);

    bool UploadToBuffer(VkDeviceMemory mem, const void* data, VkDeviceSize size);

    // BaseMap cache
    void ClearBaseMapSetCache();

    // Fallback 1x1 white texture + descriptor set(set=1)
    bool CreateFallbackWhiteTexture();
    void DestroyFallbackWhiteTexture();
    bool CreateFallbackBaseMapSet(const char* pipelineName);
    void DestroyFallbackBaseMapSet();

private:
    SDL_Window* mWindow{ nullptr };

    VkInstance       mInstance{ VK_NULL_HANDLE };
    VkSurfaceKHR     mSurface{ VK_NULL_HANDLE };
    VkDebugUtilsMessengerEXT mDebugMessenger{ VK_NULL_HANDLE };
    bool             mEnableValidation{ true };

    VkPhysicalDevice mPhysicalDevice{ VK_NULL_HANDLE };
    VkDevice         mDevice{ VK_NULL_HANDLE };

    VkQueue          mQueueGraphics{ VK_NULL_HANDLE };
    VkQueue          mQueuePresent{ VK_NULL_HANDLE };
    uint32_t         mQueueFamilyGraphics{ UINT32_MAX };
    uint32_t         mQueueFamilyPresent{ UINT32_MAX };

    VkSwapchainKHR     mSwapchain{ VK_NULL_HANDLE };
    VkSurfaceFormatKHR mSwapchainFormat{};
    VkExtent2D         mSwapchainExtent{};

    std::vector<VkImage>     mSwapchainImages;
    std::vector<VkImageView> mSwapchainImageViews;

    // depth
    VkFormat        mDepthFormat{ VK_FORMAT_UNDEFINED };
    VkImage         mDepthImage{ VK_NULL_HANDLE };
    VkDeviceMemory  mDepthMemory{ VK_NULL_HANDLE };
    VkImageView     mDepthImageView{ VK_NULL_HANDLE };

    // render pass / fb
    VkRenderPass               mRenderPass{ VK_NULL_HANDLE };
    std::vector<VkFramebuffer> mFramebuffers;

    // cmd
    VkCommandPool mCommandPool{ VK_NULL_HANDLE };

    struct FrameSync
    {
        VkCommandBuffer cmd{ VK_NULL_HANDLE };
        VkSemaphore     imageAvailable{ VK_NULL_HANDLE };
        VkSemaphore     renderFinished{ VK_NULL_HANDLE };
        VkFence         inFlight{ VK_NULL_HANDLE };
    };
    std::vector<FrameSync> mFrames;
    uint32_t mFrameIndex{ 0 };
    uint32_t mImageIndex{ 0 };
    bool     mNeedRecreateSwapchain{ false };

    float mWindowDisplayScale{ 1.0f };

    // pipelines
    VKPipelineLibrary mPipelines;

    //==========================================================
    // DescriptorPool / SceneUBO / SceneSet
    //  - mDescPool は UBO系（Scene set=0 / Skinned set=2）
    //==========================================================
    VkDescriptorPool mDescPool{ VK_NULL_HANDLE };

    // SceneUBO size
    size_t mSceneUBOSize{ 0 };

    // SceneUBO (per frame) : World
    std::vector<VkBuffer>       mSceneUBO;
    std::vector<VkDeviceMemory> mSceneUBOMem;

    // SceneUBO (per frame) : UI
    std::vector<VkBuffer>       mSceneUBO_UI;
    std::vector<VkDeviceMemory> mSceneUBOMem_UI;

    // SceneSet (per frame) : World / UI それぞれ set=0
    std::vector<VkDescriptorSet> mSceneSet;     // world
    std::vector<VkDescriptorSet> mSceneSet_UI;  // ui

    //==========================================================
    // BaseMap (set=1)
    //  - “専用 pool を増設” して枯れを回避
    //==========================================================
    struct BaseMapKey
    {
        uint32_t frame = 0;
        const Texture* tex = nullptr;
        std::string pipelineName;

        bool operator==(const BaseMapKey& o) const
        {
            return frame == o.frame &&
                   tex == o.tex &&
                   pipelineName == o.pipelineName;
        }
    };

    struct BaseMapKeyHash
    {
        size_t operator()(const BaseMapKey& k) const noexcept
        {
            size_t h = 1469598103934665603ull;
            auto mix = [&](size_t v){ h ^= v; h *= 1099511628211ull; };

            mix(std::hash<uint32_t>{}(k.frame));
            mix(std::hash<const void*>{}(k.tex));
            mix(std::hash<std::string>{}(k.pipelineName));
            return h;
        }
    };

    struct CachedDescriptorSet
    {
        VkDescriptorPool pool = VK_NULL_HANDLE;
        VkDescriptorSet  set  = VK_NULL_HANDLE;
    };

    std::unordered_map<BaseMapKey, CachedDescriptorSet, BaseMapKeyHash> mBaseMapSetCache;

    // BaseMap pool chain
    std::vector<VkDescriptorPool> mBaseMapPools;
    uint32_t mBaseMapPoolCursor = 0;

    VkDescriptorPool CreateBaseMapPool(uint32_t maxSets, uint32_t samplerCount);
    VkDescriptorPool GetActiveBaseMapPool();
    VkDescriptorPool GrowBaseMapPoolAndGet();

    //==========================================================
    // Fallback base map (1x1 white)
    //==========================================================
    VkImage        mFallbackWhiteImg{ VK_NULL_HANDLE };
    VkDeviceMemory mFallbackWhiteMem{ VK_NULL_HANDLE };
    VkImageView    mFallbackWhiteView{ VK_NULL_HANDLE };
    VkSampler      mFallbackWhiteSampler{ VK_NULL_HANDLE };

    // backward compat (旧名だけ残す)
    VkDescriptorSet mFallbackBaseMapSet{ VK_NULL_HANDLE };

    // pipeline ごとの fallback DS（poolも持つ）
    std::unordered_map<std::string, CachedDescriptorSet> mFallbackBaseMapSetByPipe;

private:
    //==========================================================
    // Skinned palette slot pool (set=2)
    //  - draw ごとに UBO/DS を切る（同一cmd内の上書き事故を避ける）
    //==========================================================
    struct SkinnedPaletteSlot
    {
        VkBuffer        ubo{ VK_NULL_HANDLE };
        VkDeviceMemory  mem{ VK_NULL_HANDLE };
        VkDescriptorSet set{ VK_NULL_HANDLE }; // set=2 (mDescPool)
    };

    static constexpr uint32_t     kMaxPalette       = 96;
    static constexpr VkDeviceSize kSkinnedUBOSize   = sizeof(float) * 16 * kMaxPalette;

    std::vector<std::vector<SkinnedPaletteSlot>> mSkinnedSlots;
    std::vector<uint32_t>                        mSkinnedSlotCursor;

    VkDescriptorSet AcquireSkinnedSet(const Matrix4* palette,
                                      uint32_t paletteCount,
                                      const char* pipelineName);

    void DestroySkinnedSlots();
    
private:
    //==========================================================
    // Shadow mapping (Vulkan)
    //  - depth-only pass (2 cascades like GL)
    //  - per-cascade depth image + framebuffer
    //  - shadow scene UBO(set=0) for LightVP
    //==========================================================

    // resources create/destroy
    bool CreateShadowResources();
    void DestroyShadowResources();

    // shadow scene UBO/set (set=0 binding=0)
    bool CreateShadowSceneUBOAndSet();
    void DestroyShadowSceneUBOAndSet();

    // update matrices
    void UpdateShadowLightMatrices();
    void UpdateShadowSceneUBO(int cascadeIndex);

    //----------------------------------------------------------
    // shadow data
    //----------------------------------------------------------
    struct ShadowCascade
    {
        VkImage        depthImg{ VK_NULL_HANDLE };
        VkDeviceMemory depthMem{ VK_NULL_HANDLE };
        VkImageView    depthView{ VK_NULL_HANDLE };
        VkFramebuffer  fb{ VK_NULL_HANDLE };

        Matrix4 lightVP{ Matrix4::Identity };
        Matrix4 lightVP_Biased{ Matrix4::Identity }; // optional (for sampling later)
    };

    // constants (合わせて cpp 側でも使う)
    static constexpr int kShadowCascadeCount = 2;

    // depth targets
    VkExtent2D mShadowExtent{ 0, 0 };
    VkFormat   mShadowDepthFormat{ VK_FORMAT_UNDEFINED };

    std::vector<ShadowCascade> mShadowCascades;

    // depth-only render pass + compare sampler
    VkRenderPass mShadowRenderPass{ VK_NULL_HANDLE };
    VkSampler    mShadowSampler{ VK_NULL_HANDLE };

    // bias matrix (optional)
    Matrix4 mShadowBias{ Matrix4::Identity };

    // shadow scene UBO/set (per frame)
    std::vector<VkBuffer>       mShadowSceneUBO;
    std::vector<VkDeviceMemory> mShadowSceneUBOMem;
    std::vector<VkDescriptorSet> mShadowSceneSet; // set=0

    // VKRenderer.h（private: あたり）
    bool mIsInRenderPass{ false };

    // shadow pass 中のカスケード番号（DrawItem に渡す用）
    int  mShadowCascadeIndex{ -1 };
    
};

} // namespace toy
