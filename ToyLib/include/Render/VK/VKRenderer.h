//======================================================================
// Render/VK/VKRenderer.h
//
// VKRenderer は現状 “全部入り” で肥大化しやすいので、まずは
//  - セクション分け（責務ごと）
//  - 並び順の統一（public → private、初期化→描画→補助）
//  - コメント（日本語、目的/注意点）
// を入れて「見通しを良くする」段階の整理を行う。
//
// 方針（現状の確定事項）:
//  - SceneUBO / SceneSet は World と UI で分離（set=0）
//  - DrawItem は 引数 pass で SceneSet を選ぶ（RenderItemに依存しない）
//  - Skinned は set=2 を “draw 単位で確保＆更新” して上書き事故を回避
//  - BaseMap(set=1) は “専用DescriptorPoolを増設” して枯れを回避
//  - Shadow は depth-only pass + sampled set=3（2 cascades）
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
#include <array>

namespace toy
{

class Application;
class Texture;
class VKParticleBackend;

struct ParticleComputeJob
{
    VKParticleBackend* backend { nullptr };
    float deltaTime { 0.0f };
};



//======================================================================
// VKRenderer
//======================================================================
class VKRenderer : public IRenderer
{
public:
    //==========================================================
    // ライフサイクル（IRenderer）
    //==========================================================
    VKRenderer();
    virtual ~VKRenderer();
    
    bool Initialize(const Application* app) override;
    void Shutdown() override;
    
    void WaitIdle() override;
    
    std::shared_ptr<IRenderTarget> CreateRenderTarget() override;
    
    void OnWindowResized(int pixelW, int pixelH) override;
    
    bool BeginFrame() override;
    void EndFrame() override;
    
    //==========================================================
    // 描画フェーズ（IRenderer）
    //  - swapchain renderpass は World/UI を同一pass内で描く前提
    //==========================================================
    void DrawShadowPass() override;
    void RestoreAfterShadowPass() override;
    void DrawSkyPass() override;
    void DrawWorldPass() override;
    void DrawOverlayScreenPass() override;
    void DrawFadePass() override;
    void DrawPostEffectPass() override;
    void DrawUIPass() override;
    
    
    // “描画1単位”
    void DrawItem(const RenderItem& it, RenderPass pass, int cascadeIndex) override;
    
    // bucket draw（VKRenderer専用補助）
    void DrawBucket_UI(const std::vector<uint32_t>& bucket);
    void DrawBucket_Sky(const std::vector<uint32_t>& bucket);
    void DrawBucket_OverlayScreen(const std::vector<uint32_t>& bucket);
    
    PipelineHandle GetPipelineHandle(const std::string& name) override;
    
    const VkDevice GetVKDevice() const { return mDevice; }
    const VkPhysicalDevice GetVKPhysicalDevice() const { return mPhysicalDevice; }
public:
    //==========================================================
    // Descriptor / UBO（set=0/1/2/3 の管理）
    //==========================================================
    bool CreateDescriptorPool();
    void DestroyDescriptorPool();
    
    // SceneUBO は World/UI 両方作る
    bool CreateSceneUBO();
    void DestroySceneUBO();
    
    // ★更新は必ず「どっちに書くか」を明示する（上書き事故防止）
    void UpdateSceneUBO_World();                        // mSceneUBO[frame]
    void UpdateSceneUBO_UI(const Matrix4& uiViewProj); // mSceneUBO_UI[frame]
    
    // SceneSet は World/UI 両方作る（set=0）
    bool CreateSceneDescriptorSet();
    
    // BaseMap(set=1)
    VkDescriptorSet GetOrCreateBaseMapSet(const Texture* tex, const char* pipelineName);
    
    VkDescriptorPool GetDescriptorPool() const { return mDescPool; }
    VkCommandBuffer GetCurrentCommandBuffer() const
    {
        if (mFrames.empty())
        {
            return VK_NULL_HANDLE;
        }
        return mFrames[mFrameIndex].cmd;
    }
    
private:
    //==========================================================
    // Vulkan 初期化（大枠）
    //==========================================================
    bool CreateInstance();
    bool CreateSurface();
    bool PickPhysicalDevice();
    bool CreateDeviceAndQueues();
    bool CreateSwapchainAndViews();
    
    // swapchain depth
    bool CreateDepthForSwapchain();
    void DestroyDepthForSwapchain();
    
    // swapchain pass
    bool CreateRenderPass();
    bool CreateFramebuffers();
    
    // cmd / sync
    bool CreateCommandPoolAndBuffers();
    bool CreateSyncObjects();
    
    // swapchain recreate
    bool RecreateSwapchain();
    void CleanupSwapchain();
    
    // pipelines
    bool BuildDefaultPipelines();
    
private:
    //==========================================================
    // コマンド（one-time cmd）
    //==========================================================
    VkCommandBuffer BeginOneTimeCommands();
    void EndOneTimeCommands(VkCommandBuffer cmd);
    
private:
    //==========================================================
    // バッファ（host-visible）
    //==========================================================
    bool CreateBufferHostVisible(VkDeviceSize size,
                                 VkBufferUsageFlags usage,
                                 VkBuffer& outBuf,
                                 VkDeviceMemory& outMem);
    
    bool UploadToBuffer(VkDeviceMemory mem, const void* data, VkDeviceSize size);
    
private:
    //==========================================================
    // BaseMap(set=1) キャッシュ
    //==========================================================
    void ClearBaseMapSetCache();
    
    // Fallback 1x1 white texture + descriptor set(set=1)
    bool CreateFallbackWhiteTexture();
    void DestroyFallbackWhiteTexture();
    bool CreateFallbackBaseMapSet(const char* pipelineName);
    void DestroyFallbackBaseMapSet();
    
private:
    //==========================================================
    // Swapchain renderpass 制御
    //  - World と UI を “同一 renderpass 内” で描くための補助
    //==========================================================
    void BeginSwapchainRenderPassIfNeeded();
    void EndSwapchainRenderPassIfNeeded();
    
private:
    //==========================================================
    // 主要ハンドル
    //==========================================================
    SDL_Window* mWindow{ nullptr };
    
    VkInstance               mInstance{ VK_NULL_HANDLE };
    VkSurfaceKHR             mSurface{ VK_NULL_HANDLE };
    VkDebugUtilsMessengerEXT mDebugMessenger{ VK_NULL_HANDLE };
    bool                     mEnableValidation{ true };
    
    VkPhysicalDevice mPhysicalDevice{ VK_NULL_HANDLE };
    VkDevice         mDevice{ VK_NULL_HANDLE };
    
    VkQueue   mQueueGraphics{ VK_NULL_HANDLE };
    VkQueue   mQueuePresent{ VK_NULL_HANDLE };
    uint32_t  mQueueFamilyGraphics{ UINT32_MAX };
    uint32_t  mQueueFamilyPresent{ UINT32_MAX };
    
private:
    //==========================================================
    // Swapchain / Views
    //==========================================================
    VkSwapchainKHR     mSwapchain{ VK_NULL_HANDLE };
    VkSurfaceFormatKHR mSwapchainFormat{};
    VkExtent2D         mSwapchainExtent{};
    
    std::vector<VkImage>     mSwapchainImages;
    std::vector<VkImageView> mSwapchainImageViews;
    
private:
    //==========================================================
    // Depth（swapchain 用）
    //==========================================================
    VkFormat       mDepthFormat{ VK_FORMAT_UNDEFINED };
    VkImage        mDepthImage{ VK_NULL_HANDLE };
    VkDeviceMemory mDepthMemory{ VK_NULL_HANDLE };
    VkImageView    mDepthImageView{ VK_NULL_HANDLE };
    
private:
    //==========================================================
    // RenderPass / Framebuffers（swapchain）
    //==========================================================
    VkRenderPass               mRenderPass{ VK_NULL_HANDLE };
    std::vector<VkFramebuffer> mFramebuffers;
    
private:
    //==========================================================
    // Command buffers / Synchronization（FrameSync）
    //==========================================================
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
    
    // swapchain renderpass の中に居るか（World/UI を同一passで描くため）
    bool mIsInRenderPass{ false };
    
private:
    //==========================================================
    // Pipeline
    //==========================================================
    VKPipelineLibrary mPipelines;
    
private:
    //==========================================================
    // DescriptorPool / SceneUBO / SceneSet（set=0）
    //  - mDescPool は UBO 系（Scene set=0 / Skinned set=2 / Sky / Overlay）用
    //==========================================================
    VkDescriptorPool mDescPool{ VK_NULL_HANDLE };
    
    // SceneUBO size（構造体サイズと一致させる）
    size_t mSceneUBOSize{ 0 };
    
    // SceneUBO（per frame）: World
    std::vector<VkBuffer>       mSceneUBO;
    std::vector<VkDeviceMemory> mSceneUBOMem;
    
    // SceneUBO（per frame）: UI
    std::vector<VkBuffer>       mSceneUBO_UI;
    std::vector<VkDeviceMemory> mSceneUBOMem_UI;
    
    // SceneSet（per frame）: set=0
    std::vector<VkDescriptorSet> mSceneSet;     // world
    std::vector<VkDescriptorSet> mSceneSet_UI;  // ui
    
    // Sky UBO / set
    std::vector<VkBuffer>        mSkyUBO;
    std::vector<VkDeviceMemory>  mSkyUBOMem;
    std::vector<VkDescriptorSet> mSkySet;
    size_t mSkyUBOSize{ 0 };
    
    // OverlayScreen UBO / set
    std::vector<VkBuffer>        mOverlayUBO;
    std::vector<VkDeviceMemory>  mOverlayUBOMem;
    std::vector<VkDescriptorSet> mOverlaySet;
    size_t mOverlayUBOSize{ 0 };
    
    std::vector<VkFence> mImagesInFlight;
    
private:
    //==========================================================
    // BaseMap(set=1)
    //  - “専用 pool を増設”して枯れを回避
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
    
    std::vector<VkDescriptorPool> mBaseMapPools;
    uint32_t mBaseMapPoolCursor = 0;
    
    VkDescriptorPool CreateBaseMapPool(uint32_t maxSets, uint32_t samplerCount);
    VkDescriptorPool GetActiveBaseMapPool();
    VkDescriptorPool GrowBaseMapPoolAndGet();
    
private:
    //==========================================================
    // Fallback base map（1x1 white）
    //==========================================================
    VkImage        mFallbackWhiteImg{ VK_NULL_HANDLE };
    VkDeviceMemory mFallbackWhiteMem{ VK_NULL_HANDLE };
    VkImageView    mFallbackWhiteView{ VK_NULL_HANDLE };
    VkSampler      mFallbackWhiteSampler{ VK_NULL_HANDLE };
    
    // backward compat（旧名だけ残す）
    VkDescriptorSet mFallbackBaseMapSet{ VK_NULL_HANDLE };
    
    // pipeline ごとの fallback DS（pool も保持）
    std::unordered_map<std::string, CachedDescriptorSet> mFallbackBaseMapSetByPipe;
    
private:
    //==========================================================
    // Skinned palette slot pool（set=2）
    //  - draw ごとに UBO/DS を切る（同一cmd内の上書き事故を避ける）
    //==========================================================
    struct SkinnedPaletteSlot
    {
        VkBuffer        ubo{ VK_NULL_HANDLE };
        VkDeviceMemory  mem{ VK_NULL_HANDLE };
        VkDescriptorSet set{ VK_NULL_HANDLE }; // set=2（mDescPool）
    };
    
    static constexpr uint32_t     kMaxPalette     = 96;
    static constexpr VkDeviceSize kSkinnedUBOSize = sizeof(float) * 16 * kMaxPalette;
    
    std::vector<std::vector<SkinnedPaletteSlot>> mSkinnedSlots;
    std::vector<uint32_t>                        mSkinnedSlotCursor;
    
    VkDescriptorSet AcquireSkinnedSet(const Matrix4* palette,
                                      uint32_t paletteCount,
                                      const char* pipelineName);
    
    void DestroySkinnedSlots();
    
private:
    //==========================================================
    // Shadow mapping（Vulkan）
    //==========================================================
    bool CreateShadowResources();
    void DestroyShadowResources();
    
    bool CreateShadowSceneUBOAndSet();
    void DestroyShadowSceneUBOAndSet();
    
    void UpdateShadowLightMatrices();
    void UpdateShadowSceneUBO(int cascadeIndex);
    
    bool BuildShadowPipelinesOnly();
    
    int  mShadowCascadeIndex{ -1 };
    
    struct ShadowCascade
    {
        VkImage        depthImg{ VK_NULL_HANDLE };
        VkDeviceMemory depthMem{ VK_NULL_HANDLE };
        VkImageView    depthView{ VK_NULL_HANDLE };
        VkFramebuffer  fb{ VK_NULL_HANDLE };
        
        Matrix4 lightVP{ Matrix4::Identity };
    };
    
    static constexpr int kShadowCascadeCount = 2;
    
    VkExtent2D mShadowExtent{ 0, 0 };
    VkFormat   mShadowDepthFormat{ VK_FORMAT_UNDEFINED };
    
    std::vector<ShadowCascade> mShadowCascades;
    
    VkRenderPass mShadowRenderPass{ VK_NULL_HANDLE };
    VkSampler    mShadowSampler{ VK_NULL_HANDLE };
    
    std::array<std::vector<VkBuffer>,        kShadowCascadeCount> mShadowSceneUBO;
    std::array<std::vector<VkDeviceMemory>,  kShadowCascadeCount> mShadowSceneUBOMem;
    std::array<std::vector<VkDescriptorSet>, kShadowCascadeCount> mShadowSceneSet;
    
private:
    //==========================================================
    // Shadow: sampled descriptor（set=3）
    //==========================================================
    VkDescriptorSetLayout        mShadowMapSetLayout{ VK_NULL_HANDLE };
    std::vector<VkDescriptorSet> mShadowMapSet;
    
    std::array<bool, 2> mShadowIsSampledLayout { false, false };
    
    bool CreateShadowMapSetLayoutAndSets();
    void DestroyShadowMapSetLayoutAndSets();
    
    bool CreateShadowSampleSet();
    void UpdateShadowSampleSet();
    
    VkDescriptorSet GetShadowMapSetForCurrentFrame() const
    {
        if (mFrameIndex >= mShadowMapSet.size()) return VK_NULL_HANDLE;
        return mShadowMapSet[mFrameIndex];
    }
    
    VkDescriptorPool mShadowDescPoolUsed{ VK_NULL_HANDLE };
    
    void TransitionShadowDepthToSampledIfNeeded(VkCommandBuffer cmd);
    
private:
    //==========================================================
    // SkyDome (set=1 UBO)
    //==========================================================
    bool CreateSkyUBO();
    void DestroySkyUBO();
    void UpdateSkyUBO(const SkyDomePayload& sky);
    bool CreateSkyDescriptorSet();
    
private:
    //==========================================================
    // OverlayScreen / WeatherOverlay (set=1 UBO)
    //==========================================================
    bool CreateOverlayUBO();
    void DestroyOverlayUBO();
    void UpdateOverlayUBO(const OverlayPayload& overlay);
    bool CreateOverlayDescriptorSet();
    
private:
    //==========================================================
    // 空 DescriptorSet（setの穴埋め用）
    //==========================================================
    struct EmptySetKey
    {
        uint32_t frame = 0;
        std::string pipelineName;
        uint32_t setIndex = 0;
        
        bool operator==(const EmptySetKey& o) const
        {
            return frame == o.frame &&
            setIndex == o.setIndex &&
            pipelineName == o.pipelineName;
        }
    };
    
    struct EmptySetKeyHash
    {
        size_t operator()(const EmptySetKey& k) const noexcept
        {
            size_t h = 1469598103934665603ull;
            auto mix = [&](size_t v){ h ^= v; h *= 1099511628211ull; };
            
            mix(std::hash<uint32_t>{}(k.frame));
            mix(std::hash<std::string>{}(k.pipelineName));
            mix(std::hash<uint32_t>{}(k.setIndex));
            return h;
        }
    };
    
    std::unordered_map<EmptySetKey, VkDescriptorSet, EmptySetKeyHash> mEmptySetCache;
    
    VkDescriptorSet GetOrCreateEmptySet(const char* pipelineName, uint32_t setIndex);
    void ClearEmptySetCache();
    
private:
    //==========================================================
    // SceneCapture
    //==========================================================
    bool mIsDrawingCapture { false };
    
    static constexpr uint32_t kMaxSceneCaptureSlots = 8;
    
    std::vector<std::vector<VkBuffer>>        mSceneUBO_Capture;     // [frame][slot]
    std::vector<std::vector<VkDeviceMemory>>  mSceneUBOMem_Capture;  // [frame][slot]
    std::vector<std::vector<VkDescriptorSet>> mSceneSet_Capture;     // [frame][slot]
    
    uint32_t mCaptureSlotCursor { 0 };
    int      mActiveCaptureSlot { -1 };
    
    // SceneCapture
    bool CreateSceneUBO_Capture();
    void DestroySceneUBO_Capture();
    void UpdateSceneUBO_Capture(const Matrix4& viewProj);
    void DrawToRenderTarget(const SceneCaptureRequest& req) override;

private:
private:
    //==========================================================
    // PostEffect
    //  - descriptor は per-frame 固定
    //==========================================================
    std::vector<VkDescriptorSet> mPostEffectSets;
    VkDescriptorSetLayout        mPostEffectSetLayout { VK_NULL_HANDLE };

    bool CreatePostEffectDescriptorSets();
    void UpdatePostEffectDescriptorSet(uint32_t frameIndex,
                                       const Texture* sceneTex,
                                       const Texture* paperTex);
    
    bool mRenderToSceneRTThisFrame { false };
    
    struct PostEffectSetKey
    {
        uint32_t frame = 0;
        const Texture* sceneTex = nullptr;
        const Texture* paperTex = nullptr;

        bool operator==(const PostEffectSetKey& o) const
        {
            return frame == o.frame &&
                   sceneTex == o.sceneTex &&
                   paperTex == o.paperTex;
        }
    };

    struct PostEffectSetKeyHash
    {
        size_t operator()(const PostEffectSetKey& k) const noexcept
        {
            size_t h = 1469598103934665603ull;
            auto mix = [&](size_t v){ h ^= v; h *= 1099511628211ull; };

            mix(std::hash<uint32_t>{}(k.frame));
            mix(std::hash<const void*>{}(k.sceneTex));
            mix(std::hash<const void*>{}(k.paperTex));
            return h;
        }
    };
    

public:
    void EnqueueParticleCompute(VKParticleBackend* backend, float deltaTime);
    void DequeueParticleCompute(VKParticleBackend* backend);
    void RecordQueuedParticleComputes(VkCommandBuffer cmd);

private:
    std::vector<ParticleComputeJob> mParticleComputeJobs {};
    
};

} // namespace toy
