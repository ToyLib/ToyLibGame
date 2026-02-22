//======================================================================
// Render/VK/VKRenderer_Descriptors.cpp
//  - Sprite(Texture) を出すための最低限を集約
//  - DescriptorPool / SceneUBO / SceneSet / SpriteTextureSet cache
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
//  ※将来的には vkutil 側に寄せてもOK（現状はこのCPP内で完結）
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
// local helper: Sprite pipeline set layout getter
//--------------------------------------------------------------
static VkDescriptorSetLayout GetSpritePipelineSetLayout(VKPipelineLibrary& lib,
                                                        uint32_t setIndex)
{
    auto* p = lib.Get("Sprite");
    if (!p)
    {
        return VK_NULL_HANDLE;
    }
    return p->GetSetLayout(setIndex); // ★VKPipeline に GetSetLayout がある前提
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

    constexpr uint32_t kMaxSceneSets   = 8;
    constexpr uint32_t kMaxTextureSets = 512;

    VkDescriptorPoolSize sizes[2]{};
    sizes[0].type            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    sizes[0].descriptorCount = kMaxSceneSets;

    sizes[1].type            = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    sizes[1].descriptorCount = kMaxTextureSets;

    VkDescriptorPoolCreateInfo ci{};
    ci.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    // 個別に set を free できるようにしておく（キャッシュ破棄や失敗ロールバックが楽）
    ci.flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    ci.maxSets       = kMaxSceneSets + kMaxTextureSets;
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

//--------------------------------------------------------------
// small helper: Free descriptor set (if possible)
//--------------------------------------------------------------
static void FreeDescriptorSetIfPossible(VkDevice device, VkDescriptorPool pool, VkDescriptorSet set)
{
    if (!device || !pool || !set) return;
    // pool が FREE_DESCRIPTOR_SET_BIT で作られている前提
    vkFreeDescriptorSets(device, pool, 1, &set);
}

void VKRenderer::DestroyDescriptorPool()
{
    if (!mDevice)
    {
        return;
    }

    // 先に個別解放（順序事故・後始末の見通しを良くする）
    if (mSceneSet != VK_NULL_HANDLE)
    {
        FreeDescriptorSetIfPossible(mDevice, mDescPool, mSceneSet);
        mSceneSet = VK_NULL_HANDLE;
    }

    ClearSpriteTextureSetCache();

    if (mDescPool != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorPool(mDevice, mDescPool, nullptr);
        mDescPool = VK_NULL_HANDLE;
    }
}

//==============================================================
// Scene UBO (最小)
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

void VKRenderer::UpdateSceneUBO()
{
    if (!mDevice || mSceneUBOMem == VK_NULL_HANDLE || mSceneUBOSize == 0)
    {
        return;
    }

    VKSceneUBO_Min ubo{};

    // ここで毎回 view*proj を構築して詰める（mViewProjMatrix 前提を排除）
    const Matrix4 viewProj = mViewMatrix * mProjectionMatrix;

    // Matrix4 が 16float 連続の前提（ToyLibのMathUtil設計に合わせる）
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
// Sprite Texture Set cache
//==============================================================
void VKRenderer::ClearSpriteTextureSetCache()
{
    // キャッシュに積んだ DS を個別解放（pool が FREE_DESCRIPTOR_SET_BIT の前提）
    if (mDevice && mDescPool)
    {
        for (auto& kv : mSpriteTexSetCache)
        {
            VkDescriptorSet ds = kv.second;
            if (ds != VK_NULL_HANDLE)
            {
                FreeDescriptorSetIfPossible(mDevice, mDescPool, ds);
            }
        }
    }
    mSpriteTexSetCache.clear();
}

//==============================================================
// Sprite Texture Set (set=1 binding=0 CombinedImageSampler)
//==============================================================
VkDescriptorSet VKRenderer::GetOrCreateSpriteTextureSet(const Texture* tex)
{
    if (!tex || !mDevice || !mDescPool)
    {
        return VK_NULL_HANDLE;
    }

    auto it = mSpriteTexSetCache.find(tex);
    if (it != mSpriteTexSetCache.end())
    {
        return it->second;
    }

    VkDescriptorSetLayout set1 = GetSpritePipelineSetLayout(mPipelines, 1);
    if (set1 == VK_NULL_HANDLE)
    {
        std::cerr << "[VKRenderer] Sprite set1 layout is null\n";
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
        std::cerr << "[VKRenderer] vkAllocateDescriptorSets(SpriteTexSet) failed: " << vr << "\n";
        return VK_NULL_HANDLE;
    }

    // 以降の失敗では ds を必ず解放する
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
        return fail("[VKRenderer] texture GPU is null");
    }

    auto* vkgpu = dynamic_cast<VKTextureGPU*>(gpu);
    if (!vkgpu)
    {
        return fail("[VKRenderer] texture GPU is not VKTextureGPU");
    }

    if (vkgpu->GetSampler() == VK_NULL_HANDLE || vkgpu->GetImageView() == VK_NULL_HANDLE)
    {
        return fail("[VKRenderer] VKTextureGPU sampler/view is null");
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

    mSpriteTexSetCache[tex] = ds;
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

} // namespace toy
