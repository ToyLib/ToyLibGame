// Asset/Geometry/VK/VKVertexArrayBackend.cpp
#include "Asset/Geometry/VK/VKVertexArrayBackend.h"
#include "Render/RenderBackendState.h"

#include <iostream>
#include <cstring>
#include <vector>

namespace toy {

//------------------------------------------------------------------------------
// Helpers
//------------------------------------------------------------------------------
static uint32_t FindMemoryType(VkPhysicalDevice /*unused*/,
                              VkPhysicalDeviceMemoryProperties memProps,
                              uint32_t typeBits,
                              VkMemoryPropertyFlags props)
{
    for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i)
    {
        if ((typeBits & (1u << i)) &&
            (memProps.memoryTypes[i].propertyFlags & props) == props)
        {
            return i;
        }
    }
    return UINT32_MAX;
}

// NOTE:
//  - 今は最小実装として「vkGetPhysicalDeviceMemoryProperties」を取れないので、ここはVKRenderer側で
//    memProps を渡すのが本来。
//  - ただし、あなたの今の構造だと VertexArrayBackend 単体で GPU を触るので
//    “RenderBackendState に PhysicalDevice も持たせる” のが妥当。
//  - ここでは「まず動かす」ために、RenderBackendState に PhysicalDevice を足す前提にする。

// ---- 追加してほしい（必須）------------------------------------
// RenderBackendState に PhysicalDevice も保存しておく：
//   void SetVKPhysicalDevice(void* gpu) { mVKPhysicalDevice = gpu; }
//   void* GetVKPhysicalDevice() const { return mVKPhysicalDevice; }
// --------------------------------------------------------------

static VkPhysicalDevice GetVKPhysicalDevice()
{
    return (VkPhysicalDevice)RenderBackendState::Get().GetVKPhysicalDevice();
}

static VkDevice GetVKDevice()
{
    return (VkDevice)RenderBackendState::Get().GetVKDevice();
}

bool VKVertexArrayBackend::CreateBufferHostVisible(VkDeviceSize size,
                                                   VkBufferUsageFlags usage,
                                                   VkBuffer& outBuf,
                                                   VkDeviceMemory& outMem)
{
    outBuf = VK_NULL_HANDLE;
    outMem = VK_NULL_HANDLE;

    if (!mDevice) return false;

    VkBufferCreateInfo bci{};
    bci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bci.size  = size;
    bci.usage = usage;
    bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(mDevice, &bci, nullptr, &outBuf) != VK_SUCCESS)
    {
        return false;
    }

    VkMemoryRequirements req{};
    vkGetBufferMemoryRequirements(mDevice, outBuf, &req);

    VkPhysicalDevice gpu = GetVKPhysicalDevice();
    if (!gpu)
    {
        std::cerr << "[VKVertexArrayBackend] PhysicalDevice is null (store it in RenderBackendState)\n";
        vkDestroyBuffer(mDevice, outBuf, nullptr);
        outBuf = VK_NULL_HANDLE;
        return false;
    }

    VkPhysicalDeviceMemoryProperties memProps{};
    vkGetPhysicalDeviceMemoryProperties(gpu, &memProps);

    const uint32_t typeIndex = FindMemoryType(
        gpu,
        memProps,
        req.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    if (typeIndex == UINT32_MAX)
    {
        std::cerr << "[VKVertexArrayBackend] No suitable HOST_VISIBLE memory type\n";
        vkDestroyBuffer(mDevice, outBuf, nullptr);
        outBuf = VK_NULL_HANDLE;
        return false;
    }

    VkMemoryAllocateInfo mai{};
    mai.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    mai.allocationSize  = req.size;
    mai.memoryTypeIndex = typeIndex;

    if (vkAllocateMemory(mDevice, &mai, nullptr, &outMem) != VK_SUCCESS)
    {
        vkDestroyBuffer(mDevice, outBuf, nullptr);
        outBuf = VK_NULL_HANDLE;
        return false;
    }

    if (vkBindBufferMemory(mDevice, outBuf, outMem, 0) != VK_SUCCESS)
    {
        vkFreeMemory(mDevice, outMem, nullptr);
        vkDestroyBuffer(mDevice, outBuf, nullptr);
        outMem = VK_NULL_HANDLE;
        outBuf = VK_NULL_HANDLE;
        return false;
    }

    return true;
}

bool VKVertexArrayBackend::UploadToBuffer(VkDeviceMemory mem, const void* data, VkDeviceSize size)
{
    if (!mDevice || !mem || !data || size == 0) return false;

    void* mapped = nullptr;
    if (vkMapMemory(mDevice, mem, 0, size, 0, &mapped) != VK_SUCCESS)
    {
        return false;
    }

    std::memcpy(mapped, data, (size_t)size);

    vkUnmapMemory(mDevice, mem);
    return true;
}

//------------------------------------------------------------------------------
// ctors
//------------------------------------------------------------------------------
VKVertexArrayBackend::VKVertexArrayBackend(unsigned int numVerts,
                                           const float* verts,
                                           const float* norms,
                                           const float* uvs,
                                           const unsigned int* boneids,
                                           const float* weights,
                                           unsigned int numIndices,
                                           const unsigned int* indices)
{
    mDevice = GetVKDevice();

    // ここは “今すぐスプライト” が目的なら後回しでもOK（skinnedはとりあえず未対応で落とさない）
    (void)numVerts; (void)verts; (void)norms; (void)uvs; (void)boneids; (void)weights;
    (void)numIndices; (void)indices;

    std::cerr << "[VKVertexArrayBackend] Skinned ctor not implemented yet.\n";
}

VKVertexArrayBackend::VKVertexArrayBackend(unsigned int numVerts,
                                           const float* verts,
                                           const float* norms,
                                           const float* uvs,
                                           unsigned int numIndices,
                                           const unsigned int* indices)
{
    mDevice = GetVKDevice();

    if (!mDevice || !verts || !norms || !uvs || !indices)
    {
        std::cerr << "[VKVertexArrayBackend] Static mesh invalid args\n";
        return;
    }

    // ------------------------------------------------------
    // 1) interleave (pos3 + normal3 + uv2)
    // ------------------------------------------------------
    std::vector<float> interleaved;
    interleaved.reserve(numVerts * 8);

    for (unsigned int i = 0; i < numVerts; ++i)
    {
        // pos
        interleaved.push_back(verts[i*3 + 0]);
        interleaved.push_back(verts[i*3 + 1]);
        interleaved.push_back(verts[i*3 + 2]);

        // normal
        interleaved.push_back(norms[i*3 + 0]);
        interleaved.push_back(norms[i*3 + 1]);
        interleaved.push_back(norms[i*3 + 2]);

        // uv
        interleaved.push_back(uvs[i*2 + 0]);
        interleaved.push_back(uvs[i*2 + 1]);
    }

    const VkDeviceSize vbSize = sizeof(float) * interleaved.size();
    const VkDeviceSize ibSize = sizeof(uint32_t) * numIndices;

    // ------------------------------------------------------
    // 2) Create VB
    // ------------------------------------------------------
    if (!CreateBufferHostVisible(vbSize,
                                 VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                 mVB, mVBMem))
    {
        std::cerr << "[VKVertexArrayBackend] Create VB failed (static)\n";
        return;
    }

    // ------------------------------------------------------
    // 3) Create IB
    // ------------------------------------------------------
    if (!CreateBufferHostVisible(ibSize,
                                 VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                                 mIB, mIBMem))
    {
        std::cerr << "[VKVertexArrayBackend] Create IB failed (static)\n";
        return;
    }

    // ------------------------------------------------------
    // 4) Upload
    // ------------------------------------------------------
    if (!UploadToBuffer(mVBMem, interleaved.data(), vbSize) ||
        !UploadToBuffer(mIBMem, indices, ibSize))
    {
        std::cerr << "[VKVertexArrayBackend] Upload failed (static)\n";
        Unload();
        return;
    }

    mIndexType = VK_INDEX_TYPE_UINT32;
}

VKVertexArrayBackend::VKVertexArrayBackend(const float* verts,
                                           unsigned int numVerts,
                                           const unsigned int* indices,
                                           unsigned int numIndices)
{
    mDevice = GetVKDevice();

    if (!mDevice || !verts || !indices || numVerts == 0 || numIndices == 0)
    {
        std::cerr << "[VKVertexArrayBackend] Sprite ctor invalid args\n";
        return;
    }

    // Sprite VAO in GL: interleaved 8 floats/vertex (pos3+norm3+uv2)
    const VkDeviceSize vbSize = (VkDeviceSize)(sizeof(float) * 8 * numVerts);
    const VkDeviceSize ibSize = (VkDeviceSize)(sizeof(uint32_t) * numIndices);

    if (!CreateBufferHostVisible(vbSize,
                                 VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                 mVB, mVBMem))
    {
        std::cerr << "[VKVertexArrayBackend] Create VB failed\n";
        return;
    }

    if (!CreateBufferHostVisible(ibSize,
                                 VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                                 mIB, mIBMem))
    {
        std::cerr << "[VKVertexArrayBackend] Create IB failed\n";
        return;
    }

    if (!UploadToBuffer(mVBMem, verts, vbSize) ||
        !UploadToBuffer(mIBMem, indices, ibSize))
    {
        std::cerr << "[VKVertexArrayBackend] Upload failed\n";
        Unload();
        return;
    }

    mIndexType = VK_INDEX_TYPE_UINT32;
}

VKVertexArrayBackend::VKVertexArrayBackend(const float* verts,
                                           unsigned int numVerts,
                                           const unsigned int* indices,
                                           unsigned int numIndices,
                                           bool /*isVec2Only*/)
{
    mDevice = GetVKDevice();

    if (!mDevice || !verts || numVerts == 0)
    {
        std::cerr << "[VKVertexArrayBackend] Vec2 ctor invalid args\n";
        return;
    }

    // vec2 only: 2 floats/vertex
    const VkDeviceSize vbSize = (VkDeviceSize)(sizeof(float) * 2 * numVerts);

    if (!CreateBufferHostVisible(vbSize,
                                 VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                 mVB, mVBMem))
    {
        std::cerr << "[VKVertexArrayBackend] Create VB failed (vec2)\n";
        return;
    }

    if (!UploadToBuffer(mVBMem, verts, vbSize))
    {
        std::cerr << "[VKVertexArrayBackend] Upload VB failed (vec2)\n";
        Unload();
        return;
    }

    // indices optional
    if (indices && numIndices > 0)
    {
        const VkDeviceSize ibSize = (VkDeviceSize)(sizeof(uint32_t) * numIndices);
        if (!CreateBufferHostVisible(ibSize,
                                     VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                                     mIB, mIBMem))
        {
            std::cerr << "[VKVertexArrayBackend] Create IB failed (vec2)\n";
            return;
        }
        if (!UploadToBuffer(mIBMem, indices, ibSize))
        {
            std::cerr << "[VKVertexArrayBackend] Upload IB failed (vec2)\n";
            Unload();
            return;
        }
        mIndexType = VK_INDEX_TYPE_UINT32;
    }
}

VKVertexArrayBackend::~VKVertexArrayBackend()
{
    Unload();
}

void VKVertexArrayBackend::Unload()
{
    if (mDevice) return;

    if (mIB != VK_NULL_HANDLE)
    {
        //vkDestroyBuffer(mDevice, mIB, nullptr);
        mIB = VK_NULL_HANDLE;
    }
    if (mIBMem != VK_NULL_HANDLE)
    {
        vkFreeMemory(mDevice, mIBMem, nullptr);
        mIBMem = VK_NULL_HANDLE;
    }

    if (mVB != VK_NULL_HANDLE)
    {
        //vkDestroyBuffer(mDevice, mVB, nullptr);
        mVB = VK_NULL_HANDLE;
    }
    if (mVBMem != VK_NULL_HANDLE)
    {
        vkFreeMemory(mDevice, mVBMem, nullptr);
        mVBMem = VK_NULL_HANDLE;
    }
}

} // namespace toy
