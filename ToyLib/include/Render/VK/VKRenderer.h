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
#include "Render/VK/VKBaseMapDescriptorCache.h"
#include "Render/VK/VKUniformSet.h"
#include "Utils/MathUtil.h"

#include <SDL3/SDL.h>
#include <vulkan/vulkan.h>

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace toy
{

class Application;
class Texture;
class VKParticleBackend;

struct ParticleComputeJob
{
    VKParticleBackend* backend{nullptr};
    float deltaTime{0.0f};
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

    // Texture アンロード時にキャッシュから該当エントリを除去
    void OnTextureUnloaded(const class Texture* tex);

    bool BeginFrame() override;
    void EndFrame() override;

    //==========================================================
    // 描画フェーズ（IRenderer）
    //  - swapchain renderpass は World/UI を同一pass内で描く前提
    //==========================================================
    Matrix4 GetLightSpaceMatrix(int cascadeIndex) const override;

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

    // DrawItem タイプ別分割（private helpers）
    void DrawItem_Shadow(VkCommandBuffer cmd, const RenderItem& it, VkDescriptorSet sceneSet, int cascadeIndex);
    void DrawItem_Sprite(VkCommandBuffer cmd, const RenderItem& it, VkDescriptorSet sceneSet);
    void DrawItem_Mesh(VkCommandBuffer cmd, const RenderItem& it, VkDescriptorSet sceneSet, const char* pipelineName);
    void DrawItem_Skinned(VkCommandBuffer cmd, const RenderItem& it, VkDescriptorSet sceneSet,
                          const char* pipelineName);
    void DrawItem_UnlitQuad(VkCommandBuffer cmd, const RenderItem& it, VkDescriptorSet sceneSet,
                            const char* pipelineName);
    void DrawItem_SkyDome(VkCommandBuffer cmd, const RenderItem& it, VkDescriptorSet sceneSet,
                          const char* pipelineName);
    void DrawItem_Overlay(VkCommandBuffer cmd, const RenderItem& it, const char* pipelineName);
    void DrawItem_Debug(VkCommandBuffer cmd, const RenderItem& it, VkDescriptorSet sceneSet, const char* pipelineName);
    void DrawItem_Surface(VkCommandBuffer cmd, const RenderItem& it, VkDescriptorSet sceneSet,
                          const char* pipelineName);
    void DrawItem_Particle(VkCommandBuffer cmd, const RenderItem& it, VkDescriptorSet sceneSet,
                           const char* pipelineName);

    // bucket draw（VKRenderer専用補助）
    void DrawBucket_UI(const std::vector<uint32_t>& bucket);
    void DrawBucket_Sky(const std::vector<uint32_t>& bucket);
    void DrawBucket_OverlayScreen(const std::vector<uint32_t>& bucket);

    PipelineHandle GetPipelineHandle(const std::string& name) override;
    uint64_t GetPipelineSortKey(const PipelineHandle& h) const override;

    const VkDevice GetVKDevice() const
    {
        return mDevice;
    }
    const VkPhysicalDevice GetVKPhysicalDevice() const
    {
        return mPhysicalDevice;
    }

public:
    //==========================================================
    // Descriptor / UBO（set=0/1/2/3 の管理）
    //==========================================================
    bool CreateDescriptorPool();
    void DestroyDescriptorPool();

    // SceneUBO + SceneSet(set=0) は World/UI 両方作る
    bool CreateSceneUBO();
    void DestroySceneUBO();

    // ★更新は必ず「どっちに書くか」を明示する（上書き事故防止）
    void UpdateSceneUBO_World();                       // mSceneUniformWorld
    void UpdateSceneUBO_UI(const Matrix4& uiViewProj); // mSceneUniformUI

    // fallback texture（set=1）のセットアップ
    bool CreateSceneDescriptorSet();

    // BaseMap(set=1)
    VkDescriptorSet GetOrCreateBaseMapSet(const Texture* tex, const char* pipelineName);

    VkDescriptorPool GetDescriptorPool() const
    {
        return mDescPool;
    }
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
    bool CreateBufferHostVisible(VkDeviceSize size, VkBufferUsageFlags usage, VkBuffer& outBuf, VkDeviceMemory& outMem);

    bool UploadToBuffer(VkDeviceMemory mem, const void* data, VkDeviceSize size);

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
    SDL_Window* mWindow{nullptr};

    VkInstance mInstance{VK_NULL_HANDLE};
    VkSurfaceKHR mSurface{VK_NULL_HANDLE};
    VkDebugUtilsMessengerEXT mDebugMessenger{VK_NULL_HANDLE};
    bool mEnableValidation{true};

    VkPhysicalDevice mPhysicalDevice{VK_NULL_HANDLE};
    VkDevice mDevice{VK_NULL_HANDLE};

    VkQueue mQueueGraphics{VK_NULL_HANDLE};
    VkQueue mQueuePresent{VK_NULL_HANDLE};
    uint32_t mQueueFamilyGraphics{UINT32_MAX};
    uint32_t mQueueFamilyPresent{UINT32_MAX};

private:
    //==========================================================
    // Swapchain / Views
    //==========================================================
    VkSwapchainKHR mSwapchain{VK_NULL_HANDLE};
    VkSurfaceFormatKHR mSwapchainFormat{};
    VkExtent2D mSwapchainExtent{};

    std::vector<VkImage> mSwapchainImages;
    std::vector<VkImageView> mSwapchainImageViews;

private:
    //==========================================================
    // Depth（swapchain 用）
    //==========================================================
    VkFormat mDepthFormat{VK_FORMAT_UNDEFINED};
    VkImage mDepthImage{VK_NULL_HANDLE};
    VkDeviceMemory mDepthMemory{VK_NULL_HANDLE};
    VkImageView mDepthImageView{VK_NULL_HANDLE};

private:
    //==========================================================
    // RenderPass / Framebuffers（swapchain）
    //==========================================================
    VkRenderPass mRenderPass{VK_NULL_HANDLE};
    std::vector<VkFramebuffer> mFramebuffers;

private:
    //==========================================================
    // Command buffers / Synchronization（FrameSync）
    //==========================================================
    VkCommandPool mCommandPool{VK_NULL_HANDLE};

    struct FrameSync
    {
        VkCommandBuffer cmd{VK_NULL_HANDLE};
        VkSemaphore imageAvailable{VK_NULL_HANDLE};
        VkSemaphore renderFinished{VK_NULL_HANDLE};
        VkFence inFlight{VK_NULL_HANDLE};
    };

    std::vector<FrameSync> mFrames;
    uint32_t mFrameIndex{0};
    uint32_t mImageIndex{0};
    bool mNeedRecreateSwapchain{false};

    float mWindowDisplayScale{1.0f};

    // swapchain renderpass の中に居るか（World/UI を同一passで描くため）
    bool mIsInRenderPass{false};

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
    VkDescriptorPool mDescPool{VK_NULL_HANDLE};

    // SceneUBO + SceneSet（per frame, set=0）: World / UI
    VKUniformSet mSceneUniformWorld;
    VKUniformSet mSceneUniformUI;

    std::vector<VkFence> mImagesInFlight;

private:
    //==========================================================
    // BaseMap(set=1) + fallback(1x1 white)
    //  - テクスチャ→DescriptorSetのキャッシュ/プール管理はVKBaseMapDescriptorCacheに委譲
    //==========================================================
    VKBaseMapDescriptorCache mBaseMapCache;

private:
    //==========================================================
    // Skinned palette slot pool（set=2）
    //  - draw ごとに UBO/DS を切る（同一cmd内の上書き事故を避ける）
    //==========================================================
    struct SkinnedPaletteSlot
    {
        VkBuffer ubo{VK_NULL_HANDLE};
        VkDeviceMemory mem{VK_NULL_HANDLE};
        VkDescriptorSet set{VK_NULL_HANDLE}; // set=2（mDescPool）
        uint32_t capacity{0};                // 確保済みのボーン数（このバッファが保持できる上限）
    };

    // シェーダ側 (VK/src/SkinnedMesh.vert, Shadow_SkinnedMesh.vert) は
    // runtime-sized array（SSBO, mat4 matrixPalette[];）なのでコンパイル時の上限は無い。
    // バッファは AcquireSkinnedSet() 内で必要なボーン数ぶんだけ確保/成長させる。
    // これは設計上の上限ではなく、壊れたデータによる暴走確保を防ぐための安全弁。
    static constexpr uint32_t kPaletteSanityCap = 8192;

    std::vector<std::vector<SkinnedPaletteSlot>> mSkinnedSlots;
    std::vector<uint32_t> mSkinnedSlotCursor;

    VkDescriptorSet AcquireSkinnedSet(const Matrix4* palette, uint32_t paletteCount, const char* pipelineName);

    void DestroySkinnedSlots();

private:
    //==========================================================
    // Shadow mapping（Vulkan）
    //==========================================================
    bool CreateShadowResources();
    void DestroyShadowResources();

    bool CreateShadowSceneUBOAndSet();
    void DestroyShadowSceneUBOAndSet();

    void UpdateShadowLightMatrices() override;
    void UpdateShadowSceneUBO(int cascadeIndex);

    bool BuildShadowPipelinesOnly();

    int mShadowCascadeIndex{-1};

    struct ShadowCascade
    {
        VkImage depthImg{VK_NULL_HANDLE};
        VkDeviceMemory depthMem{VK_NULL_HANDLE};
        VkImageView depthView{VK_NULL_HANDLE};
        VkFramebuffer fb{VK_NULL_HANDLE};

        Matrix4 lightVP{Matrix4::Identity};
    };

    static constexpr int kShadowCascadeCount = 2;

    VkExtent2D mShadowExtent{0, 0};
    VkFormat mShadowDepthFormat{VK_FORMAT_UNDEFINED};

    std::vector<ShadowCascade> mShadowCascades;

    VkRenderPass mShadowRenderPass{VK_NULL_HANDLE};
    VkSampler mShadowSampler{VK_NULL_HANDLE};

    std::array<VKUniformSet, kShadowCascadeCount> mShadowUniform;

private:
    //==========================================================
    // Shadow: sampled descriptor（set=3）
    //==========================================================
    VkDescriptorSetLayout mShadowMapSetLayout{VK_NULL_HANDLE};
    std::vector<VkDescriptorSet> mShadowMapSet;

    std::array<bool, 2> mShadowIsSampledLayout{false, false};

    bool CreateShadowMapSetLayoutAndSets();
    void DestroyShadowMapSetLayoutAndSets();

    bool CreateShadowSampleSet();
    void UpdateShadowSampleSet();

    VkDescriptorSet GetShadowMapSetForCurrentFrame() const
    {
        if (mFrameIndex >= mShadowMapSet.size())
        {
            return VK_NULL_HANDLE;
        }
        return mShadowMapSet[mFrameIndex];
    }

    VkDescriptorPool mShadowDescPoolUsed{VK_NULL_HANDLE};

    void TransitionShadowDepthToSampledIfNeeded(VkCommandBuffer cmd);

private:
    //==========================================================
    // SkyDome (set=1 UBO)
    //==========================================================
    bool CreateSkyUBO();
    void DestroySkyUBO();
    void UpdateSkyUBO(const SkyDomePayload& sky);

    VKUniformSet mSkyUniform;

private:
    //==========================================================
    // OverlayScreen / WeatherOverlay (set=1 UBO)
    //==========================================================
    bool CreateOverlayUBO();
    void DestroyOverlayUBO();
    void UpdateOverlayUBO(const OverlayPayload& overlay);

    VKUniformSet mOverlayUniform;

private:
    //==========================================================
    // 空 DescriptorSet（setの穴埋め用）
    //==========================================================
    struct EmptySetKey
    {
        // frame は含めない — 空セットはバインディングなしで全フレーム共用可能
        std::string pipelineName;
        uint32_t setIndex = 0;

        bool operator==(const EmptySetKey& o) const
        {
            return setIndex == o.setIndex && pipelineName == o.pipelineName;
        }
    };

    struct EmptySetKeyHash
    {
        size_t operator()(const EmptySetKey& k) const noexcept
        {
            size_t h = 1469598103934665603ull;
            auto mix = [&](size_t v)
            {
                h ^= v;
                h *= 1099511628211ull;
            };

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
    //  - mIsDrawingCapture は IRenderer 側で共有定義（BuildFrameQueues が参照するため）
    //==========================================================
    static constexpr uint32_t kMaxSceneCaptureSlots = 8;

    std::array<VKUniformSet, kMaxSceneCaptureSlots> mCaptureUniform;

    uint32_t mCaptureSlotCursor{0};
    int mActiveCaptureSlot{-1};

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
    VkDescriptorSetLayout mPostEffectSetLayout{VK_NULL_HANDLE};

    bool CreatePostEffectDescriptorSets();
    void UpdatePostEffectDescriptorSet(uint32_t frameIndex, const Texture* sceneTex, const Texture* paperTex);

    bool mRenderToSceneRTThisFrame{false};

    struct PostEffectSetKey
    {
        uint32_t frame = 0;
        const Texture* sceneTex = nullptr;
        const Texture* paperTex = nullptr;

        bool operator==(const PostEffectSetKey& o) const
        {
            return frame == o.frame && sceneTex == o.sceneTex && paperTex == o.paperTex;
        }
    };

    struct PostEffectSetKeyHash
    {
        size_t operator()(const PostEffectSetKey& k) const noexcept
        {
            size_t h = 1469598103934665603ull;
            auto mix = [&](size_t v)
            {
                h ^= v;
                h *= 1099511628211ull;
            };

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
    std::vector<ParticleComputeJob> mParticleComputeJobs{};
};

} // namespace toy
