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

private:
    RenderBackendType mType { RenderBackendType::Unknown };

    // Vulkan handles (opaque to avoid including vulkan.h here)
    void* mVKPhysicalDevice { nullptr }; // VkPhysicalDevice
    void* mVKDevice         { nullptr }; // VkDevice
    void* mVKGraphicsQueue  { nullptr }; // VkQueue
    void* mVKCommandPool    { nullptr }; // VkCommandPool

    TextureUnloadCallback mTextureUnloadCallback;
};

} // namespace toy
