// Asset/Geometry/VertexArrayBackend.h
#pragma once

#include <cstdint>

namespace toy {

class IVertexArrayBackend
{
public:
    virtual ~IVertexArrayBackend() = default;

    virtual void Unload() = 0;

    // GL: glBindVertexArray etc.
    // VK: 基本 no-op（cmd が必要なので）
    virtual void Bind() {}

    // --- VK accessors (default: not supported) ---
    virtual bool IsVK() const { return false; }

    // Returns VK buffers (if IsVK()==true)
    virtual void* GetVKVertexBuffer() const { return nullptr; } // VkBuffer
    virtual void* GetVKIndexBuffer()  const { return nullptr; } // VkBuffer
    virtual uint64_t GetVKVertexOffset() const { return 0; }
    virtual uint64_t GetVKIndexOffset()  const { return 0; }
    virtual uint32_t GetVKIndexType()    const { return 0; } // VkIndexType
};

} // namespace toy
