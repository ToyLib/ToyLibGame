// Asset/Geometry/VK/VKVertexArrayBackend.cpp
#include "Asset/Geometry/VK/VKVertexArrayBackend.h"
#include "Render/RenderBackendState.h"

#include <iostream>
#include <cstring>
#include <vector>
#include <cstdint>

namespace toy {

//------------------------------------------------------------------------------
// Helpers
//------------------------------------------------------------------------------
static uint32_t FindMemoryType(VkPhysicalDeviceMemoryProperties memProps,
                              uint32_t typeBits,
                              VkMemoryPropertyFlags props)
{
    for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i)
    {
        const bool typeOk = (typeBits & (1u << i)) != 0;
        const bool propOk = (memProps.memoryTypes[i].propertyFlags & props) == props;
        if (typeOk && propOk)
        {
            return i;
        }
    }
    return UINT32_MAX;
}

static VkPhysicalDevice GetVKPhysicalDevice()
{
    return (VkPhysicalDevice)RenderBackendState::Get().GetVKPhysicalDevice();
}

static VkDevice GetVKDevice()
{
    return (VkDevice)RenderBackendState::Get().GetVKDevice();
}

//------------------------------------------------------------------------------
// Buffer helpers
//------------------------------------------------------------------------------
bool VKVertexArrayBackend::CreateBufferHostVisible(VkDeviceSize size,
                                                   VkBufferUsageFlags usage,
                                                   VkBuffer& outBuf,
                                                   VkDeviceMemory& outMem)
{
    outBuf = VK_NULL_HANDLE;
    outMem = VK_NULL_HANDLE;

    if (!mDevice || size == 0)
    {
        return false;
    }

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

    const uint32_t typeIndex =
        FindMemoryType(memProps,
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
    if (!mDevice || mem == VK_NULL_HANDLE || !data || size == 0)
    {
        return false;
    }

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

    if (!mDevice || numVerts == 0 || numIndices == 0 ||
        !verts || !norms || !uvs || !boneids || !weights || !indices)
    {
        std::cerr << "[VKVertexArrayBackend] Skinned mesh invalid args\n";
        return;
    }

    // Interleaved struct (float + uint)
    struct VtxSkinned
    {
        float    pos[3];
        float    nrm[3];
        float    uv[2];
        uint32_t bone[4];
        float    w[4];
    };

    std::vector<VtxSkinned> interleaved;
    interleaved.resize(numVerts);

    for (unsigned int i = 0; i < numVerts; ++i)
    {
        VtxSkinned v{};

        // pos
        v.pos[0] = verts[i * 3 + 0];
        v.pos[1] = verts[i * 3 + 1];
        v.pos[2] = verts[i * 3 + 2];

        // normal
        v.nrm[0] = norms[i * 3 + 0];
        v.nrm[1] = norms[i * 3 + 1];
        v.nrm[2] = norms[i * 3 + 2];

        // uv
        v.uv[0] = uvs[i * 2 + 0];
        v.uv[1] = uvs[i * 2 + 1];

        // bone ids (uvec4)
        v.bone[0] = (uint32_t)boneids[i * 4 + 0];
        v.bone[1] = (uint32_t)boneids[i * 4 + 1];
        v.bone[2] = (uint32_t)boneids[i * 4 + 2];
        v.bone[3] = (uint32_t)boneids[i * 4 + 3];

        // weights (vec4)
        v.w[0] = weights[i * 4 + 0];
        v.w[1] = weights[i * 4 + 1];
        v.w[2] = weights[i * 4 + 2];
        v.w[3] = weights[i * 4 + 3];

        interleaved[i] = v;
    }

    const VkDeviceSize vbSize = (VkDeviceSize)(sizeof(VtxSkinned) * interleaved.size());
    const VkDeviceSize ibSize = (VkDeviceSize)(sizeof(uint32_t) * (VkDeviceSize)numIndices);

    // VB
    if (!CreateBufferHostVisible(vbSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, mVB, mVBMem))
    {
        std::cerr << "[VKVertexArrayBackend] Create VB failed (skinned)\n";
        return;
    }

    // IB
    if (!CreateBufferHostVisible(ibSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, mIB, mIBMem))
    {
        std::cerr << "[VKVertexArrayBackend] Create IB failed (skinned)\n";
        Unload();
        return;
    }

    // Upload
    if (!UploadToBuffer(mVBMem, interleaved.data(), vbSize) ||
        !UploadToBuffer(mIBMem, indices, ibSize))
    {
        std::cerr << "[VKVertexArrayBackend] Upload failed (skinned)\n";
        Unload();
        return;
    }

    mIndexType = VK_INDEX_TYPE_UINT32;
}

VKVertexArrayBackend::VKVertexArrayBackend(unsigned int numVerts,
                                           const float* verts,
                                           const float* norms,
                                           const float* uvs,
                                           unsigned int numIndices,
                                           const unsigned int* indices)
{
    mDevice = GetVKDevice();

    if (!mDevice || !verts || !norms || !uvs || !indices || numVerts == 0 || numIndices == 0)
    {
        std::cerr << "[VKVertexArrayBackend] Static mesh invalid args\n";
        return;
    }

    // interleave (pos3 + normal3 + uv2)
    std::vector<float> interleaved;
    interleaved.reserve(numVerts * 8);

    for (unsigned int i = 0; i < numVerts; ++i)
    {
        interleaved.push_back(verts[i * 3 + 0]);
        interleaved.push_back(verts[i * 3 + 1]);
        interleaved.push_back(verts[i * 3 + 2]);

        interleaved.push_back(norms[i * 3 + 0]);
        interleaved.push_back(norms[i * 3 + 1]);
        interleaved.push_back(norms[i * 3 + 2]);

        interleaved.push_back(uvs[i * 2 + 0]);
        interleaved.push_back(uvs[i * 2 + 1]);
    }

    const VkDeviceSize vbSize = (VkDeviceSize)(sizeof(float) * interleaved.size());
    const VkDeviceSize ibSize = (VkDeviceSize)(sizeof(uint32_t) * (VkDeviceSize)numIndices);

    if (!CreateBufferHostVisible(vbSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, mVB, mVBMem))
    {
        std::cerr << "[VKVertexArrayBackend] Create VB failed (static)\n";
        return;
    }

    if (!CreateBufferHostVisible(ibSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, mIB, mIBMem))
    {
        std::cerr << "[VKVertexArrayBackend] Create IB failed (static)\n";
        Unload();
        return;
    }

    if (!UploadToBuffer(mVBMem, interleaved.data(), vbSize) ||
        !UploadToBuffer(mIBMem, indices, ibSize))
    {
        std::cerr << "[VKVertexArrayBackend] Upload failed (static)\n";
        Unload();
        return;
    }

    mIndexType = VK_INDEX_TYPE_UINT32;
}

// Sprite ctor (pos3+norm3+uv2 interleaved already)
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

    const VkDeviceSize vbSize = (VkDeviceSize)(sizeof(float) * 8 * (VkDeviceSize)numVerts);
    const VkDeviceSize ibSize = (VkDeviceSize)(sizeof(uint32_t) * (VkDeviceSize)numIndices);

    if (!CreateBufferHostVisible(vbSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, mVB, mVBMem))
    {
        std::cerr << "[VKVertexArrayBackend] Create VB failed (sprite)\n";
        return;
    }

    if (!CreateBufferHostVisible(ibSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, mIB, mIBMem))
    {
        std::cerr << "[VKVertexArrayBackend] Create IB failed (sprite)\n";
        Unload();
        return;
    }

    if (!UploadToBuffer(mVBMem, verts, vbSize) ||
        !UploadToBuffer(mIBMem, indices, ibSize))
    {
        std::cerr << "[VKVertexArrayBackend] Upload failed (sprite)\n";
        Unload();
        return;
    }

    mIndexType = VK_INDEX_TYPE_UINT32;
}

// Vec2-only ctor
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

    const VkDeviceSize vbSize = (VkDeviceSize)(sizeof(float) * 2 * (VkDeviceSize)numVerts);

    if (!CreateBufferHostVisible(vbSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, mVB, mVBMem))
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

    if (indices && numIndices > 0)
    {
        const VkDeviceSize ibSize = (VkDeviceSize)(sizeof(uint32_t) * (VkDeviceSize)numIndices);

        if (!CreateBufferHostVisible(ibSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, mIB, mIBMem))
        {
            std::cerr << "[VKVertexArrayBackend] Create IB failed (vec2)\n";
            Unload();
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
    // ★致命的バグ修正：mDevice が無いなら何もしない
    if (!mDevice)
    {
        return;
    }

    if (mIB != VK_NULL_HANDLE)
    {
        vkDestroyBuffer(mDevice, mIB, nullptr);
        mIB = VK_NULL_HANDLE;
    }
    if (mIBMem != VK_NULL_HANDLE)
    {
        vkFreeMemory(mDevice, mIBMem, nullptr);
        mIBMem = VK_NULL_HANDLE;
    }

    if (mVB != VK_NULL_HANDLE)
    {
        vkDestroyBuffer(mDevice, mVB, nullptr);
        mVB = VK_NULL_HANDLE;
    }
    if (mVBMem != VK_NULL_HANDLE)
    {
        vkFreeMemory(mDevice, mVBMem, nullptr);
        mVBMem = VK_NULL_HANDLE;
    }

    mIndexType = VK_INDEX_TYPE_UINT32;
}

} // namespace toy
