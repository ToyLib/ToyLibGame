//======================================================================
// Render/VK/VKRenderer.h
//  - World / UI の SceneUBO & SceneSet を分離（事故らない最小構成）
//  - DrawItem は 引数 pass で SceneSet を選ぶ（RenderItemに依存しない）
//  - Skinned は “set=2 を draw ごとに切る” 方式（同一cmd内の上書き事故を回避）
//======================================================================
#pragma once

#include "Render/IRenderer.h"
#include "Render/VK/Pipeline/VKPipelineLibrary.h"

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
    // DescriptorPool / SceneUBO / SceneSet / BaseMapSet cache
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

    // BaseMap DS cache : set=1
    //  - pipeline ごとに set=1 layout が異なる可能性があるため、(pipeline, texture) をキーにする
    struct BaseMapKey
    {
    const Texture* tex{ nullptr };
    std::string    pipelineName; // ★hash衝突/レイアウト混線を避ける
    };

    struct BaseMapKeyHash
    {
        size_t operator()(const BaseMapKey& k) const noexcept
        {
        size_t h = 0;
        h ^= std::hash<const void*>{}(k.tex) + 0x9e3779b97f4a7c15ull + (h << 6) + (h >> 2);
        h ^= std::hash<std::string>{}(k.pipelineName) + 0x9e3779b97f4a7c15ull + (h << 6) + (h >> 2);
        return h;
        }
    };

    struct BaseMapKeyEq
    {
        bool operator()(const BaseMapKey& a, const BaseMapKey& b) const noexcept
        {
        return a.tex == b.tex && a.pipelineName == b.pipelineName;
        }
    };

    std::unordered_map<BaseMapKey, VkDescriptorSet, BaseMapKeyHash, BaseMapKeyEq> mBaseMapSetCache;

    // backward compat (旧名だけ残す)
    std::unordered_map<const Texture*, VkDescriptorSet> mSpriteTexSetCache;

    //==========================================================
    // Fallback base map (1x1 white)
    //==========================================================
    VkImage        mFallbackWhiteImg{ VK_NULL_HANDLE };
    VkDeviceMemory mFallbackWhiteMem{ VK_NULL_HANDLE };
    VkImageView    mFallbackWhiteView{ VK_NULL_HANDLE };
    VkSampler      mFallbackWhiteSampler{ VK_NULL_HANDLE };

    VkDescriptorSet mFallbackBaseMapSet{ VK_NULL_HANDLE };
    std::unordered_map<std::string, VkDescriptorSet> mFallbackBaseMapSetByPipe;

private:
    //==========================================================
    // Skinned palette slot pool (set=2)
    //  - draw ごとに UBO/DS を切る（同一cmd内の上書き事故を避ける）
    //  - swapchain recreate で pipeline layout が変わる場合があるので、
    //    RecreateSwapchain() 側で DestroySkinnedSlots() を呼ぶ
    //==========================================================
    struct SkinnedPaletteSlot
    {
        VkBuffer        ubo{ VK_NULL_HANDLE };
        VkDeviceMemory  mem{ VK_NULL_HANDLE };
        VkDescriptorSet set{ VK_NULL_HANDLE }; // set=2
    };

    static constexpr uint32_t   kMaxPalette     = 96;
    static constexpr VkDeviceSize kSkinnedUBOSize = sizeof(float) * 16 * kMaxPalette;

    // per-frame slot pool
    std::vector<std::vector<SkinnedPaletteSlot>> mSkinnedSlots;
    std::vector<uint32_t>                        mSkinnedSlotCursor;

    // acquire (allocate slot if needed + upload)
    VkDescriptorSet AcquireSkinnedSet(const Matrix4* palette,
                                      uint32_t paletteCount,
                                      const char* pipelineName);

    // destroy (buffers/mem + descriptor sets if possible)
    void DestroySkinnedSlots();
};

} // namespace toy
