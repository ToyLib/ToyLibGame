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

    // SceneCapture（RenderTargetへ描画）
    void DrawToRenderTarget(const SceneCaptureRequest& req) override;

    // “描画1単位”
    void DrawItem(const RenderItem& it, RenderPass pass, int cascadeIndex) override;

    // UI bucket（VKRenderer専用補助）
    void DrawBucket_UI(const std::vector<uint32_t>& bucket);

    PipelineHandle GetPipelineHandle(const std::string& name) override;

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
    void UpdateSceneUBO_World();                       // mSceneUBO[frame]
    void UpdateSceneUBO_UI(const Matrix4& uiViewProj); // mSceneUBO_UI[frame]

    // SceneSet は World/UI 両方作る（set=0）
    bool CreateSceneDescriptorSet();

    // BaseMap(set=1)
    VkDescriptorSet GetOrCreateBaseMapSet(const Texture* tex, const char* pipelineName);

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

    // 画面DPI等（必要に応じて）
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
    //  - mDescPool は UBO 系（Scene set=0 / Skinned set=2）用
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

    // per-frame / per-texture / per-pipeline の DS キャッシュ
    std::unordered_map<BaseMapKey, CachedDescriptorSet, BaseMapKeyHash> mBaseMapSetCache;

    // BaseMap pool chain（枯れたら増やす）
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

    // [frame][slot] で保持（frame毎にカーソルで使い回す）
    std::vector<std::vector<SkinnedPaletteSlot>> mSkinnedSlots;
    std::vector<uint32_t>                        mSkinnedSlotCursor;

    VkDescriptorSet AcquireSkinnedSet(const Matrix4* palette,
                                      uint32_t paletteCount,
                                      const char* pipelineName);

    void DestroySkinnedSlots();

private:
    //==========================================================
    // Shadow mapping（Vulkan）
    //  - depth-only pass（GL同様 2 cascades）
    //  - per-cascade depth image + framebuffer
    //  - shadow scene UBO(set=0) for LightVP
    //==========================================================
    bool CreateShadowResources();
    void DestroyShadowResources();

    // shadow scene UBO/set（set=0 binding=0）
    bool CreateShadowSceneUBOAndSet();
    void DestroyShadowSceneUBOAndSet();

    // 行列更新
    void UpdateShadowLightMatrices();
    void UpdateShadowSceneUBO(int cascadeIndex);

    // pipeline（shadow専用だけ）
    bool BuildShadowPipelinesOnly();

    // shadow pass 中のカスケード番号（DrawItem に渡す用）
    int  mShadowCascadeIndex{ -1 };

    struct ShadowCascade
    {
        VkImage        depthImg{ VK_NULL_HANDLE };
        VkDeviceMemory depthMem{ VK_NULL_HANDLE };
        VkImageView    depthView{ VK_NULL_HANDLE };
        VkFramebuffer  fb{ VK_NULL_HANDLE };

        Matrix4 lightVP{ Matrix4::Identity };
        Matrix4 lightVP_Biased{ Matrix4::Identity }; // サンプリング用（0..1空間を含む前提）
    };

    static constexpr int kShadowCascadeCount = 2;

    // depth targets
    VkExtent2D mShadowExtent{ 0, 0 };
    VkFormat   mShadowDepthFormat{ VK_FORMAT_UNDEFINED };

    std::vector<ShadowCascade> mShadowCascades;

    // depth-only render pass + compare sampler
    VkRenderPass mShadowRenderPass{ VK_NULL_HANDLE };
    VkSampler    mShadowSampler{ VK_NULL_HANDLE };

    // bias matrix（必要なら）
    Matrix4 mShadowBias{ Matrix4::Identity };

    // shadow scene UBO/set（per frame）
    std::vector<VkBuffer>        mShadowSceneUBO;
    std::vector<VkDeviceMemory>  mShadowSceneUBOMem;
    std::vector<VkDescriptorSet> mShadowSceneSet; // set=0

private:
    //==========================================================
    // Shadow: sampled descriptor（set=3）
    //==========================================================
    VkDescriptorSetLayout        mShadowMapSetLayout{ VK_NULL_HANDLE };
    std::vector<VkDescriptorSet> mShadowMapSet; // per-frame

    // “depth → sampled” へ遷移済みかの追跡（最小）
    std::array<bool, 2> mShadowIsSampledLayout { false, false }; // kShadowCascadeCount=2 前提

    bool CreateShadowMapSetLayoutAndSets();
    void DestroyShadowMapSetLayoutAndSets();

    // TODO: ここの重複は後で整理（CreateShadowMapSetLayoutAndSets() と被りがち）
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
    // 空 DescriptorSet（setの穴埋め用）
    //  - 例: Mesh は set=2 を使わないが、0..3 を連番bindしたいケース
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
};

} // namespace toy
