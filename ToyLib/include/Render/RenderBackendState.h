// RenderBackendState.h
#pragma once
#include <cstdint>
#include <functional>

namespace toy {

class Texture; // forward decl（循環 include 回避）

enum class RenderBackendType : uint8_t
{
    Unknown = 0,
    OpenGL,
    Vulkan
};

class RenderBackendState
{
public:
    static RenderBackendState& Get()
    {
        static RenderBackendState s;
        return s;
    }

    //==============================================================
    // Query
    //==============================================================

    RenderBackendType Type() const { return mType; }

    bool IsGL() const { return mType == RenderBackendType::OpenGL; }
    bool IsVK() const { return mType == RenderBackendType::Vulkan; }

    //==============================================================
    // Backend selection (Application only)
    //==============================================================

    void Set(RenderBackendType t) { mType = t; }

    //==============================================================
    // Vulkan Context (set by VKRenderer::Initialize)
    //==============================================================

    // --- Physical Device ---
    void SetVKPhysicalDevice(void* phys) { mVKPhysicalDevice = phys; }
    void* GetVKPhysicalDevice() const { return mVKPhysicalDevice; }

    // --- Logical Device ---
    void SetVKDevice(void* device) { mVKDevice = device; }
    void* GetVKDevice() const { return mVKDevice; }

    // --- Graphics Queue ---
    void SetVKGraphicsQueue(void* queue) { mVKGraphicsQueue = queue; }
    void* GetVKGraphicsQueue() const { return mVKGraphicsQueue; }

    // --- Command Pool ---
    void SetVKCommandPool(void* pool) { mVKCommandPool = pool; }
    void* GetVKCommandPool() const { return mVKCommandPool; }

    //==============================================================
    // Texture アンロード通知（VKRenderer が登録）
    //==============================================================
    using TextureUnloadCallback = std::function<void(const Texture*)>;

    void SetTextureUnloadCallback(TextureUnloadCallback cb) { mTextureUnloadCallback = std::move(cb); }
    void ClearTextureUnloadCallback()                       { mTextureUnloadCallback = nullptr; }

    void NotifyTextureUnloaded(const Texture* tex)
    {
        if (mTextureUnloadCallback)
        {
            mTextureUnloadCallback(tex);
        }
    }

    //==============================================================
    // GPUハンドルの遅延破棄（VKRenderer が登録）
    //  - VkSampler/VkImageView/VkImage/VkDeviceMemoryを即座にdestroyすると、
    //    それを参照するDescriptorSetがまだ別フレームのin-flightな
    //    コマンドバッファで使用中の場合に破棄済みオブジェクト参照になる
    //    (テキストテクスチャの毎フレーム再生成などで実際に発生)。
    //    登録されていれば、破棄はコールバック先(VKRenderer)に委譲し、
    //    GPU使用完了が保証されたタイミングまで遅延させる。
    //==============================================================
    using GpuHandleRetireCallback = std::function<void(void* sampler, void* view, void* image, void* memory)>;

    void SetGpuHandleRetireCallback(GpuHandleRetireCallback cb) { mGpuHandleRetireCallback = std::move(cb); }
    void ClearGpuHandleRetireCallback()                         { mGpuHandleRetireCallback = nullptr; }

    // 戻り値: 遅延破棄を引き受けたら true（呼び出し側は即座にdestroyしてはいけない）。
    //         falseならコールバック未登録＝呼び出し側で従来通り即座にdestroyする。
    bool RetireGpuHandles(void* sampler, void* view, void* image, void* memory)
    {
        if (!mGpuHandleRetireCallback)
        {
            return false;
        }
        mGpuHandleRetireCallback(sampler, view, image, memory);
        return true;
    }

private:
    RenderBackendType mType { RenderBackendType::Unknown };

    // Vulkan handles (opaque to avoid including vulkan.h here)
    void* mVKPhysicalDevice { nullptr }; // VkPhysicalDevice
    void* mVKDevice         { nullptr }; // VkDevice
    void* mVKGraphicsQueue  { nullptr }; // VkQueue
    void* mVKCommandPool    { nullptr }; // VkCommandPool

    TextureUnloadCallback mTextureUnloadCallback;
    GpuHandleRetireCallback mGpuHandleRetireCallback;
};

} // namespace toy
