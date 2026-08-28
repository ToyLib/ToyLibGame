//======================================================================
// Render/VK/VKBaseMapDescriptorCache.cpp
//======================================================================
#include "Render/VK/VKBaseMapDescriptorCache.h"

#include "Asset/Material/ITextureGPU.h"
#include "Asset/Material/Texture.h"
#include "Render/VK/Pipeline/VKPipeline.h"
#include "Render/VK/Pipeline/VKPipelineLibrary.h"
#include "Render/VK/VKTextureGPU.h"
#include "Render/VK/VKUtil.h"

#include <cstring>
#include <iostream>

namespace toy
{

namespace
{

VkDescriptorSetLayout GetPipelineSetLayout(VKPipelineLibrary& lib, const char* pipelineName, uint32_t setIndex)
{
    auto* p = lib.Get(pipelineName);
    if (!p)
    {
        return VK_NULL_HANDLE;
    }
    return p->GetSetLayout(setIndex);
}

std::string NormalizePipelineName(const char* name)
{
    return name ? std::string(name) : std::string();
}

bool IsShadowPipelineName(const char* pipelineName)
{
    if (!pipelineName)
    {
        return false;
    }
    return (std::strncmp(pipelineName, "Shadow", 6) == 0);
}

} // namespace

void VKBaseMapDescriptorCache::Init(VkDevice device, VkPhysicalDevice phys)
{
    mDevice = device;
    mPhysicalDevice = phys;
}

//==============================================================
// Pools
//==============================================================
VkDescriptorPool VKBaseMapDescriptorCache::CreatePool(uint32_t maxSets, uint32_t samplerCount)
{
    if (!mDevice)
    {
        return VK_NULL_HANDLE;
    }

    VkDescriptorPoolSize sizes[1]{};
    sizes[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    sizes[0].descriptorCount = samplerCount;

    VkDescriptorPoolCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    ci.flags = 0; // ★個別freeしない運用（poolごと破棄）
    ci.maxSets = maxSets;
    ci.poolSizeCount = 1;
    ci.pPoolSizes = sizes;

    VkDescriptorPool pool = VK_NULL_HANDLE;
    VkResult vr = vkCreateDescriptorPool(mDevice, &ci, nullptr, &pool);
    if (vr != VK_SUCCESS)
    {
        std::cerr << "[VKBaseMapDescriptorCache] CreatePool failed vr=" << vr << "\n";
        return VK_NULL_HANDLE;
    }
    return pool;
}

VkDescriptorPool VKBaseMapDescriptorCache::GetActivePool()
{
    if (mPools.empty())
    {
        VkDescriptorPool p = CreatePool(/*maxSets*/ 8192, /*samplerCount*/ 8192);
        if (p)
        {
            mPools.push_back(p);
        }
        mPoolCursor = 0;
    }
    return mPools.empty() ? VK_NULL_HANDLE : mPools[mPoolCursor];
}

VkDescriptorPool VKBaseMapDescriptorCache::GrowPoolAndGet()
{
    const uint32_t n = (uint32_t)mPools.size();
    const uint32_t maxSets = 8192u + 4096u * n;
    const uint32_t samplers = 8192u + 4096u * n;

    VkDescriptorPool p = CreatePool(maxSets, samplers);
    if (!p)
    {
        return VK_NULL_HANDLE;
    }

    mPools.push_back(p);
    mPoolCursor = (uint32_t)mPools.size() - 1;
    return p;
}

//==============================================================
// Cache
//==============================================================
void VKBaseMapDescriptorCache::Clear()
{
    // cacheは "poolごと破棄" するので、個別 vkFree は不要
    mSetCache.clear();

    // fallback DS も baseMap pool 所有なので破棄対象
    mFallbackSetByPipe.clear();

    // baseMap pools destroy
    if (mDevice)
    {
        for (auto& p : mPools)
        {
            if (p != VK_NULL_HANDLE)
            {
                vkDestroyDescriptorPool(mDevice, p, nullptr);
                p = VK_NULL_HANDLE;
            }
        }
    }
    mPools.clear();
    mPoolCursor = 0;
}

void VKBaseMapDescriptorCache::RemoveTexture(const Texture* tex)
{
    if (!tex)
    {
        return;
    }

    for (auto it = mSetCache.begin(); it != mSetCache.end();)
    {
        if (it->first.tex == tex)
        {
            it = mSetCache.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

VkDescriptorSet VKBaseMapDescriptorCache::GetOrCreateBaseMapSet(VKPipelineLibrary& pipelines, const Texture* tex,
                                                                 const char* pipelineName)
{
    if (!mDevice || !pipelineName)
    {
        std::cerr << "[VK] BaseMapSet: invalid state dev/name\n";
        return VK_NULL_HANDLE;
    }

    const std::string pipeName = NormalizePipelineName(pipelineName);

    // Shadow pass は set=1(BaseMap) を使わない設計。
    // ここに来たら呼び出し側の設計ミスなので弾く。
    if (IsShadowPipelineName(pipelineName))
    {
        std::cerr << "[VK] BaseMapSet: Shadow pipeline requested set=1 (BUG) name=" << pipelineName << "\n";
        return VK_NULL_HANDLE;
    }

    //----------------------------------------------------------
    // fallback
    //----------------------------------------------------------
    if (!tex)
    {
        auto it = mFallbackSetByPipe.find(pipeName);
        if (it != mFallbackSetByPipe.end() && it->second.set != VK_NULL_HANDLE)
        {
            return it->second.set;
        }

        if (CreateFallbackBaseMapSet(pipelines, pipelineName))
        {
            it = mFallbackSetByPipe.find(pipeName);
            if (it != mFallbackSetByPipe.end())
            {
                return it->second.set;
            }
        }

        std::cerr << "[VK] BaseMapSet: fallback missing (" << pipelineName << ")\n";
        return VK_NULL_HANDLE;
    }

    //----------------------------------------------------------
    // cache (frame をまたいで再利用 — テクスチャは破棄まで安定)
    //----------------------------------------------------------
    BaseMapKey key{};
    key.tex = tex;
    key.pipelineName = pipeName;

    if (auto it = mSetCache.find(key); it != mSetCache.end())
    {
        return it->second.set;
    }

    //----------------------------------------------------------
    // layout
    //----------------------------------------------------------
    VkDescriptorSetLayout set1 = GetPipelineSetLayout(pipelines, pipelineName, 1);
    if (set1 == VK_NULL_HANDLE)
    {
        std::cerr << "[VK] BaseMapSet: set1 layout NULL (" << pipelineName << ")\n";
        return VK_NULL_HANDLE;
    }

    //----------------------------------------------------------
    // GPU
    //----------------------------------------------------------
    ITextureGPU* gpu = (ITextureGPU*)tex->GetGPU();
    if (!gpu)
    {
        std::cerr << "[VK] BaseMapSet: tex GPU NULL (" << pipelineName << ")\n";
        return VK_NULL_HANDLE;
    }

    auto* vkgpu = dynamic_cast<VKTextureGPU*>(gpu);
    if (!vkgpu)
    {
        std::cerr << "[VK] BaseMapSet: GPU not VKTextureGPU (" << pipelineName << ")\n";
        return VK_NULL_HANDLE;
    }

    const VkSampler sampler = vkgpu->GetSampler();
    const VkImageView view = vkgpu->GetImageView();
    if (sampler == VK_NULL_HANDLE || view == VK_NULL_HANDLE)
    {
        std::cerr << "[VK] BaseMapSet: sampler/view NULL (" << pipelineName << ")\n";
        return VK_NULL_HANDLE;
    }

    //----------------------------------------------------------
    // alloc from active baseMap pool
    //----------------------------------------------------------
    VkDescriptorPool pool = GetActivePool();
    if (pool == VK_NULL_HANDLE)
    {
        std::cerr << "[VK] BaseMapSet: baseMap pool null (" << pipelineName << ")\n";
        return VK_NULL_HANDLE;
    }

    auto allocOnce = [&](VkDescriptorPool p, VkDescriptorSet& outSet) -> VkResult
    {
        VkDescriptorSetAllocateInfo ai{};
        ai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        ai.descriptorPool = p;
        ai.descriptorSetCount = 1;
        ai.pSetLayouts = &set1;

        return vkAllocateDescriptorSets(mDevice, &ai, &outSet);
    };

    VkDescriptorSet ds = VK_NULL_HANDLE;
    VkResult vr = allocOnce(pool, ds);

    // 枯れたら増設してもう一回
    if (vr == VK_ERROR_OUT_OF_POOL_MEMORY || vr == VK_ERROR_FRAGMENTED_POOL)
    {
        pool = GrowPoolAndGet();
        if (pool == VK_NULL_HANDLE)
        {
            std::cerr << "[VK] BaseMapSet: grow pool failed (" << pipelineName << ")\n";
            return VK_NULL_HANDLE;
        }

        ds = VK_NULL_HANDLE;
        vr = allocOnce(pool, ds);
    }

    if (vr != VK_SUCCESS || ds == VK_NULL_HANDLE)
    {
        std::cerr << "[VK] BaseMapSet: alloc failed vr=" << vr << " (" << pipelineName << ")\n";
        return VK_NULL_HANDLE;
    }

    //----------------------------------------------------------
    // write
    //----------------------------------------------------------
    VkDescriptorImageInfo ii{};
    ii.sampler = sampler;
    ii.imageView = view;
    ii.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkWriteDescriptorSet w{};
    w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    w.dstSet = ds;
    w.dstBinding = 0;
    w.descriptorCount = 1;
    w.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    w.pImageInfo = &ii;

    vkUpdateDescriptorSets(mDevice, 1, &w, 0, nullptr);

    //----------------------------------------------------------
    // cache
    //----------------------------------------------------------
    CachedDescriptorSet cds{};
    cds.pool = pool;
    cds.set = ds;
    mSetCache[key] = cds;

    return ds;
}

//==============================================================
// Fallback White Texture (1x1 RGBA8) : Image/View/Sampler
//==============================================================
bool VKBaseMapDescriptorCache::CreateFallbackWhiteTexture(std::function<VkCommandBuffer()> beginOneTime,
                                                           std::function<void(VkCommandBuffer)> endOneTime)
{
    if (!mDevice || !mPhysicalDevice)
    {
        return false;
    }
    if (mFallbackWhiteImg != VK_NULL_HANDLE && mFallbackWhiteView != VK_NULL_HANDLE &&
        mFallbackWhiteSampler != VK_NULL_HANDLE)
    {
        return true;
    }

    DestroyFallbackWhiteTexture();

    const uint32_t w = 1;
    const uint32_t h = 1;
    const uint32_t pixel = 0xFFFFFFFFu;

    VkBuffer staging = VK_NULL_HANDLE;
    VkDeviceMemory stagingMem = VK_NULL_HANDLE;

    if (!toy::vkutil::CreateBuffer_HostVisible(mPhysicalDevice, mDevice, sizeof(uint32_t),
                                               VK_BUFFER_USAGE_TRANSFER_SRC_BIT, staging, stagingMem))
    {
        std::cerr << "[VKBaseMapDescriptorCache] CreateFallbackWhiteTexture: staging buffer create failed\n";
        return false;
    }

    void* mapped = nullptr;
    if (vkMapMemory(mDevice, stagingMem, 0, sizeof(uint32_t), 0, &mapped) != VK_SUCCESS)
    {
        vkDestroyBuffer(mDevice, staging, nullptr);
        vkFreeMemory(mDevice, stagingMem, nullptr);
        return false;
    }
    std::memcpy(mapped, &pixel, sizeof(uint32_t));
    vkUnmapMemory(mDevice, stagingMem);

    if (!toy::vkutil::CreateImage2D(mPhysicalDevice, mDevice, w, h, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_TILING_OPTIMAL,
                                    VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, mFallbackWhiteImg, mFallbackWhiteMem,
                                    VK_IMAGE_LAYOUT_UNDEFINED))
    {
        vkDestroyBuffer(mDevice, staging, nullptr);
        vkFreeMemory(mDevice, stagingMem, nullptr);
        std::cerr << "[VKBaseMapDescriptorCache] CreateFallbackWhiteTexture: CreateImage2D failed\n";
        return false;
    }

    VkCommandBuffer cmd = beginOneTime();
    if (cmd == VK_NULL_HANDLE)
    {
        vkDestroyBuffer(mDevice, staging, nullptr);
        vkFreeMemory(mDevice, stagingMem, nullptr);
        DestroyFallbackWhiteTexture();
        return false;
    }

    toy::vkutil::CmdTransitionImageLayout(cmd, mFallbackWhiteImg, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                          VK_PIPELINE_STAGE_TRANSFER_BIT, 0, VK_ACCESS_TRANSFER_WRITE_BIT);

    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = {0, 0, 0};
    region.imageExtent = {w, h, 1};

    vkCmdCopyBufferToImage(cmd, staging, mFallbackWhiteImg, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    toy::vkutil::CmdTransitionImageLayout(
        cmd, mFallbackWhiteImg, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT);

    endOneTime(cmd);

    vkDestroyBuffer(mDevice, staging, nullptr);
    vkFreeMemory(mDevice, stagingMem, nullptr);

    mFallbackWhiteView =
        toy::vkutil::CreateImageView2D(mDevice, mFallbackWhiteImg, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT);

    if (mFallbackWhiteView == VK_NULL_HANDLE)
    {
        std::cerr << "[VKBaseMapDescriptorCache] CreateFallbackWhiteTexture: CreateImageView2D failed\n";
        DestroyFallbackWhiteTexture();
        return false;
    }

    VkSamplerCreateInfo sci{};
    sci.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sci.magFilter = VK_FILTER_LINEAR;
    sci.minFilter = VK_FILTER_LINEAR;
    sci.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    sci.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sci.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sci.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sci.minLod = 0.0f;
    sci.maxLod = 0.0f;
    sci.maxAnisotropy = 1.0f;

    if (vkCreateSampler(mDevice, &sci, nullptr, &mFallbackWhiteSampler) != VK_SUCCESS)
    {
        std::cerr << "[VKBaseMapDescriptorCache] CreateFallbackWhiteTexture: vkCreateSampler failed\n";
        DestroyFallbackWhiteTexture();
        return false;
    }

    return true;
}

void VKBaseMapDescriptorCache::DestroyFallbackWhiteTexture()
{
    if (!mDevice)
    {
        return;
    }

    if (mFallbackWhiteSampler != VK_NULL_HANDLE)
    {
        vkDestroySampler(mDevice, mFallbackWhiteSampler, nullptr);
        mFallbackWhiteSampler = VK_NULL_HANDLE;
    }
    if (mFallbackWhiteView != VK_NULL_HANDLE)
    {
        vkDestroyImageView(mDevice, mFallbackWhiteView, nullptr);
        mFallbackWhiteView = VK_NULL_HANDLE;
    }
    if (mFallbackWhiteImg != VK_NULL_HANDLE)
    {
        vkDestroyImage(mDevice, mFallbackWhiteImg, nullptr);
        mFallbackWhiteImg = VK_NULL_HANDLE;
    }
    if (mFallbackWhiteMem != VK_NULL_HANDLE)
    {
        vkFreeMemory(mDevice, mFallbackWhiteMem, nullptr);
        mFallbackWhiteMem = VK_NULL_HANDLE;
    }
}

//==============================================================
// Fallback BaseMap DS (set=1)
//  - BaseMapPoolから allocate（枯れ対策）
//==============================================================
bool VKBaseMapDescriptorCache::CreateFallbackBaseMapSet(VKPipelineLibrary& pipelines, const char* pipelineName)
{
    if (!mDevice || !pipelineName)
    {
        return false;
    }
    if (mFallbackWhiteView == VK_NULL_HANDLE || mFallbackWhiteSampler == VK_NULL_HANDLE)
    {
        return false;
    }

    const std::string pipeName = NormalizePipelineName(pipelineName);

    {
        auto it = mFallbackSetByPipe.find(pipeName);
        if (it != mFallbackSetByPipe.end() && it->second.set != VK_NULL_HANDLE)
        {
            return true;
        }
    }

    VkDescriptorSetLayout set1 = GetPipelineSetLayout(pipelines, pipelineName, 1);
    if (set1 == VK_NULL_HANDLE)
    {
        std::cerr << "[VK] FallbackBaseMapSet: set1 layout null (" << pipelineName << ")\n";
        return false;
    }

    VkDescriptorPool pool = GetActivePool();
    if (pool == VK_NULL_HANDLE)
    {
        return false;
    }

    auto allocOnce = [&](VkDescriptorPool p, VkDescriptorSet& outSet) -> VkResult
    {
        VkDescriptorSetAllocateInfo ai{};
        ai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        ai.descriptorPool = p;
        ai.descriptorSetCount = 1;
        ai.pSetLayouts = &set1;
        return vkAllocateDescriptorSets(mDevice, &ai, &outSet);
    };

    VkDescriptorSet ds = VK_NULL_HANDLE;
    VkResult vr = allocOnce(pool, ds);

    if (vr == VK_ERROR_OUT_OF_POOL_MEMORY || vr == VK_ERROR_FRAGMENTED_POOL)
    {
        pool = GrowPoolAndGet();
        if (pool == VK_NULL_HANDLE)
        {
            return false;
        }
        ds = VK_NULL_HANDLE;
        vr = allocOnce(pool, ds);
    }

    if (vr != VK_SUCCESS || ds == VK_NULL_HANDLE)
    {
        std::cerr << "[VK] FallbackBaseMapSet: alloc failed vr=" << vr << " (" << pipelineName << ")\n";
        return false;
    }

    VkDescriptorImageInfo ii{};
    ii.sampler = mFallbackWhiteSampler;
    ii.imageView = mFallbackWhiteView;
    ii.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkWriteDescriptorSet w{};
    w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    w.dstSet = ds;
    w.dstBinding = 0;
    w.descriptorCount = 1;
    w.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    w.pImageInfo = &ii;

    vkUpdateDescriptorSets(mDevice, 1, &w, 0, nullptr);

    CachedDescriptorSet cds{};
    cds.pool = pool;
    cds.set = ds;

    mFallbackSetByPipe[pipeName] = cds;

    return true;
}

} // namespace toy
