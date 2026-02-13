// Asset/Geometry/VK/VKVertexArrayBackend.h
#pragma once

#include "Asset/Geometry/VertexArrayBackend.h"

#include <vulkan/vulkan.h>

namespace toy {

class VKVertexArrayBackend : public IVertexArrayBackend
{
public:
    // skinned
    VKVertexArrayBackend(unsigned int numVerts,
                         const float* verts,
                         const float* norms,
                         const float* uvs,
                         const unsigned int* boneids,
                         const float* weights,
                         unsigned int numIndices,
                         const unsigned int* indices);

    // static mesh
    VKVertexArrayBackend(unsigned int numVerts,
                         const float* verts,
                         const float* norms,
                         const float* uvs,
                         unsigned int numIndices,
                         const unsigned int* indices);

    // sprite (interleaved 8float: pos3+norm3+uv2)
    VKVertexArrayBackend(const float* verts,
                         unsigned int numVerts,
                         const unsigned int* indices,
                         unsigned int numIndices);

    // vec2 only
    VKVertexArrayBackend(const float* verts,
                         unsigned int numVerts,
                         const unsigned int* indices,
                         unsigned int numIndices,
                         bool isVec2Only);

    ~VKVertexArrayBackend() override;

    void Unload() override;

    bool IsVK() const override { return true; }

    void* GetVKVertexBuffer() const override { return (void*)mVB; }
    void* GetVKIndexBuffer()  const override { return (void*)mIB; }
    uint64_t GetVKVertexOffset() const override { return 0; }
    uint64_t GetVKIndexOffset()  const override { return 0; }
    uint32_t GetVKIndexType()    const override { return (uint32_t)mIndexType; }

private:
    VkDevice mDevice { VK_NULL_HANDLE };

    VkBuffer       mVB { VK_NULL_HANDLE };
    VkDeviceMemory mVBMem { VK_NULL_HANDLE };

    VkBuffer       mIB { VK_NULL_HANDLE };
    VkDeviceMemory mIBMem { VK_NULL_HANDLE };

    VkIndexType mIndexType { VK_INDEX_TYPE_UINT32 };

    bool CreateBufferHostVisible(VkDeviceSize size,
                                 VkBufferUsageFlags usage,
                                 VkBuffer& outBuf,
                                 VkDeviceMemory& outMem);

    bool UploadToBuffer(VkDeviceMemory mem, const void* data, VkDeviceSize size);
};

} // namespace toy
