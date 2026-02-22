//======================================================================
// Render/VK/VKRenderer_Descriptors.cpp
//  - DescriptorPool / SceneUBO / SceneSet / BaseMapSet cache
//======================================================================

#include "Render/VK/VKRenderer.h"

#include "Render/VK/Pipeline/VKPipeline.h"
#include "Asset/Material/VKTextureGPU.h"
#include "Asset/Material/ITextureGPU.h"

#include <iostream>
#include <cstring>

namespace toy
{

//--------------------------------------------------------------
// local helper: FindMemoryType
//--------------------------------------------------------------
static uint32_t FindMemoryType(VkPhysicalDevice physicalDevice,
                              uint32_t typeBits,
                              VkMemoryPropertyFlags props)
{
    VkPhysicalDeviceMemoryProperties memProps{};
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProps);

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

//--------------------------------------------------------------
// local helper: Pipeline set layout getter（ここが “ブレない入口”）
//--------------------------------------------------------------
static VkDescriptorSetLayout GetPipelineSetLayout(VKPipelineLibrary& lib,
                                                  const char* pipelineName,
                                                  uint32_t setIndex)
{
    if (!pipelineName)
    {
        return VK_NULL_HANDLE;
    }

    auto* p = lib.Get(pipelineName);
    if (!p)
    {
        return VK_NULL_HANDLE;
    }

    return p->GetSetLayout(setIndex);
}

// 互換用：Sprite 専用 getter（呼び出し側が残ってもOK）
static VkDescriptorSetLayout GetSpritePipelineSetLayout(VKPipelineLibrary& lib,
                                                        uint32_t setIndex)
{
    return GetPipelineSetLayout(lib, "Sprite", setIndex);
}

//--------------------------------------------------------------
// small helper: Free descriptor set (if possible)
//--------------------------------------------------------------
static void FreeDescriptorSetIfPossible(VkDevice device, VkDescriptorPool pool, VkDescriptorSet set)
{
    if (!device || !pool || !set) return;
    vkFreeDescriptorSets(device, pool, 1, &set);
}

//==============================================================
// DescriptorPool
//==============================================================
bool VKRenderer::CreateDescriptorPool()
{
    if (!mDevice)
    {
        return false;
    }
    if (mDescPool)
    {
        return true;
    }

    // 必要数：Scene(set0) + BaseMap(set1) が主。余裕を見て確保。
    constexpr uint32_t kMaxSceneSets   = 16;
    constexpr uint32_t kMaxBaseMapSets = 1024;

    VkDescriptorPoolSize sizes[2]{};
    sizes[0].type            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    sizes[0].descriptorCount = kMaxSceneSets;

    sizes[1].type            = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    sizes[1].descriptorCount = kMaxBaseMapSets;

    VkDescriptorPoolCreateInfo ci{};
    ci.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    ci.flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    ci.maxSets       = kMaxSceneSets + kMaxBaseMapSets;
    ci.poolSizeCount = 2;
    ci.pPoolSizes    = sizes;

    const VkResult vr = vkCreateDescriptorPool(mDevice, &ci, nullptr, &mDescPool);
    if (vr != VK_SUCCESS)
    {
        std::cerr << "[VKRenderer] vkCreateDescriptorPool failed: " << vr << "\n";
        mDescPool = VK_NULL_HANDLE;
        return false;
    }

    return true;
}

void VKRenderer::DestroyDescriptorPool()
{
    if (!mDevice)
    {
        return;
    }

    // 個別に解放（FREE_DESCRIPTOR_SET_BIT 前提）
    if (mSceneSet != VK_NULL_HANDLE)
    {
        FreeDescriptorSetIfPossible(mDevice, mDescPool, mSceneSet);
        mSceneSet = VK_NULL_HANDLE;
    }

    ClearBaseMapSetCache();

    if (mDescPool != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorPool(mDevice, mDescPool, nullptr);
        mDescPool = VK_NULL_HANDLE;
    }
}

//==============================================================
// Scene UBO (minimum)
//==============================================================
struct VKSceneUBO_Min
{
    float viewProj[16];
};

bool VKRenderer::CreateSceneUBO()
{
    if (!mDevice || !mPhysicalDevice)
    {
        return false;
    }
    if (mSceneUBO != VK_NULL_HANDLE)
    {
        return true;
    }

    mSceneUBOSize = sizeof(VKSceneUBO_Min);

    if (!CreateBufferHostVisible((VkDeviceSize)mSceneUBOSize,
                                 VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                 mSceneUBO,
                                 mSceneUBOMem))
    {
        std::cerr << "[VKRenderer] CreateSceneUBO: CreateBufferHostVisible failed\n";
        DestroySceneUBO();
        return false;
    }

    UpdateSceneUBO();
    return true;
}

void VKRenderer::DestroySceneUBO()
{
    if (!mDevice)
    {
        return;
    }

    if (mSceneUBO != VK_NULL_HANDLE)
    {
        vkDestroyBuffer(mDevice, mSceneUBO, nullptr);
        mSceneUBO = VK_NULL_HANDLE;
    }
    if (mSceneUBOMem != VK_NULL_HANDLE)
    {
        vkFreeMemory(mDevice, mSceneUBOMem, nullptr);
        mSceneUBOMem = VK_NULL_HANDLE;
    }

    mSceneUBOSize = 0;
}

bool VKRenderer::UploadToBuffer(VkDeviceMemory mem, const void* data, VkDeviceSize size)
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

void VKRenderer::UpdateSceneUBO()
{
    if (!mDevice || mSceneUBOMem == VK_NULL_HANDLE || mSceneUBOSize == 0)
    {
        return;
    }

    VKSceneUBO_Min ubo{};

    // ★順番は変えない（ユーザー指定）
    const Matrix4 viewProj = mViewMatrix * mProjectionMatrix;
    std::memcpy(ubo.viewProj, &viewProj, sizeof(float) * 16);

    (void)UploadToBuffer(mSceneUBOMem, &ubo, (VkDeviceSize)mSceneUBOSize);
}

//==============================================================
// Scene Descriptor Set (set=0 binding=0 UBO)
//==============================================================
bool VKRenderer::CreateSceneDescriptorSet()
{
    if (!mDevice || !mDescPool)
    {
        return false;
    }
    if (mSceneSet != VK_NULL_HANDLE)
    {
        return true;
    }
    if (mSceneUBO == VK_NULL_HANDLE || mSceneUBOSize == 0)
    {
        std::cerr << "[VKRenderer] CreateSceneDescriptorSet: SceneUBO not created\n";
        return false;
    }

    // まず Sprite pipeline の set0 を基準にする（Scene契約を統一）
    VkDescriptorSetLayout set0 = GetSpritePipelineSetLayout(mPipelines, 0);
    if (set0 == VK_NULL_HANDLE)
    {
        std::cerr << "[VKRenderer] CreateSceneDescriptorSet: Sprite set0 layout is null\n";
        return false;
    }

    VkDescriptorSetAllocateInfo ai{};
    ai.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    ai.descriptorPool     = mDescPool;
    ai.descriptorSetCount = 1;
    ai.pSetLayouts        = &set0;

    VkResult vr = vkAllocateDescriptorSets(mDevice, &ai, &mSceneSet);
    if (vr != VK_SUCCESS || mSceneSet == VK_NULL_HANDLE)
    {
        std::cerr << "[VKRenderer] vkAllocateDescriptorSets(SceneSet) failed: " << vr << "\n";
        mSceneSet = VK_NULL_HANDLE;
        return false;
    }

    VkDescriptorBufferInfo bi{};
    bi.buffer = mSceneUBO;
    bi.offset = 0;
    bi.range  = (VkDeviceSize)mSceneUBOSize;

    VkWriteDescriptorSet w{};
    w.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    w.dstSet          = mSceneSet;
    w.dstBinding      = 0;
    w.descriptorCount = 1;
    w.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    w.pBufferInfo     = &bi;

    vkUpdateDescriptorSets(mDevice, 1, &w, 0, nullptr);
    return true;
}

//==============================================================
// BaseMap set cache (set=1 binding=0 CombinedImageSampler)
//==============================================================
void VKRenderer::ClearBaseMapSetCache()
{
    if (mDevice && mDescPool)
    {
        for (auto& kv : mBaseMapSetCache)
        {
            VkDescriptorSet ds = kv.second;
            if (ds != VK_NULL_HANDLE)
            {
                FreeDescriptorSetIfPossible(mDevice, mDescPool, ds);
            }
        }
    }
    mBaseMapSetCache.clear();
}

VkDescriptorSet VKRenderer::GetOrCreateBaseMapSet(const Texture* tex, const char* pipelineName)
{
    if (!tex || !pipelineName || !mDevice || !mDescPool)
    {
        return VK_NULL_HANDLE;
    }

    BaseMapKey key{};
    key.tex  = tex;
    key.pipe = pipelineName;

    auto it = mBaseMapSetCache.find(key);
    if (it != mBaseMapSetCache.end())
    {
        return it->second;
    }

    // pipeline の set=1 layout を必ず使う（ここが “扇/無表示” の分岐点になりがち）
    VkDescriptorSetLayout set1 = GetPipelineSetLayout(mPipelines, pipelineName, 1);
    if (set1 == VK_NULL_HANDLE)
    {
        std::cerr << "[VKRenderer] GetOrCreateBaseMapSet: set1 layout is null (pipe="
                  << pipelineName << ")\n";
        return VK_NULL_HANDLE;
    }

    VkDescriptorSet ds = VK_NULL_HANDLE;

    VkDescriptorSetAllocateInfo ai{};
    ai.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    ai.descriptorPool     = mDescPool;
    ai.descriptorSetCount = 1;
    ai.pSetLayouts        = &set1;

    VkResult vr = vkAllocateDescriptorSets(mDevice, &ai, &ds);
    if (vr != VK_SUCCESS || ds == VK_NULL_HANDLE)
    {
        std::cerr << "[VKRenderer] vkAllocateDescriptorSets(BaseMapSet) failed: " << vr
                  << " (pipe=" << pipelineName << ")\n";
        return VK_NULL_HANDLE;
    }

    auto fail = [&](const char* msg) -> VkDescriptorSet
    {
        std::cerr << msg << "\n";
        FreeDescriptorSetIfPossible(mDevice, mDescPool, ds);
        ds = VK_NULL_HANDLE;
        return VK_NULL_HANDLE;
    };

    // Texture -> GPU
    ITextureGPU* gpu = (ITextureGPU*)tex->GetGPU();
    if (!gpu)
    {
        return fail("[VKRenderer] GetOrCreateBaseMapSet: texture GPU is null");
    }

    auto* vkgpu = dynamic_cast<VKTextureGPU*>(gpu);
    if (!vkgpu)
    {
        return fail("[VKRenderer] GetOrCreateBaseMapSet: texture GPU is not VKTextureGPU");
    }

    if (vkgpu->GetSampler() == VK_NULL_HANDLE || vkgpu->GetImageView() == VK_NULL_HANDLE)
    {
        return fail("[VKRenderer] GetOrCreateBaseMapSet: sampler/view is null");
    }

    VkDescriptorImageInfo ii{};
    ii.sampler     = vkgpu->GetSampler();
    ii.imageView   = vkgpu->GetImageView();
    ii.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkWriteDescriptorSet w{};
    w.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    w.dstSet          = ds;
    w.dstBinding      = 0;
    w.descriptorCount = 1;
    w.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    w.pImageInfo      = &ii;

    vkUpdateDescriptorSets(mDevice, 1, &w, 0, nullptr);

    mBaseMapSetCache.emplace(std::move(key), ds);
    return ds;
}

//==============================================================
// Host-visible buffer helpers
//==============================================================
bool VKRenderer::CreateBufferHostVisible(VkDeviceSize size,
                                        VkBufferUsageFlags usage,
                                        VkBuffer& outBuf,
                                        VkDeviceMemory& outMem)
{
    outBuf = VK_NULL_HANDLE;
    outMem = VK_NULL_HANDLE;

    if (!mDevice || !mPhysicalDevice || size == 0)
    {
        return false;
    }

    VkBufferCreateInfo bci{};
    bci.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bci.size        = size;
    bci.usage       = usage;
    bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(mDevice, &bci, nullptr, &outBuf) != VK_SUCCESS)
    {
        return false;
    }

    VkMemoryRequirements req{};
    vkGetBufferMemoryRequirements(mDevice, outBuf, &req);

    const uint32_t typeIndex =
        FindMemoryType(mPhysicalDevice,
                       req.memoryTypeBits,
                       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    if (typeIndex == UINT32_MAX)
    {
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
        outMem = VK_NULL_HANDLE;
        return false;
    }

    if (vkBindBufferMemory(mDevice, outBuf, outMem, 0) != VK_SUCCESS)
    {
        vkFreeMemory(mDevice, outMem, nullptr);
        vkDestroyBuffer(mDevice, outBuf, nullptr);
        outBuf = VK_NULL_HANDLE;
        outMem = VK_NULL_HANDLE;
        return false;
    }

    return true;
}

} // namespace toy
