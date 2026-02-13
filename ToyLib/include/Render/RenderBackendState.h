// RenderBackendState.h
#pragma once
#include <cstdint>

namespace toy {

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

private:
    RenderBackendType mType { RenderBackendType::Unknown };

    // Vulkan handles (opaque to avoid including vulkan.h here)
    void* mVKPhysicalDevice { nullptr }; // VkPhysicalDevice
    void* mVKDevice         { nullptr }; // VkDevice
    void* mVKGraphicsQueue  { nullptr }; // VkQueue
    void* mVKCommandPool    { nullptr }; // VkCommandPool
};

} // namespace toy
