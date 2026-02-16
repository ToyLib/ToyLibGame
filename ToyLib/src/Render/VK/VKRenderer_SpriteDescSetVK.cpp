//======================================================================
// VKRenderer_SpriteDescSetVK.cpp
//  - SpriteQueue(UI bucket) 用 DescriptorSet 管理
//  - Texture* 単位で「swapchain枚数分の VkDescriptorSet」をキャッシュ
//
// 前提:
//  - Sprite pipeline は set=0 binding=0 に CombinedImageSampler を要求
//  - VKRenderer.h 側に以下がある前提：
//      VkDescriptorPool mSpriteDescPool
//      std::unordered_map<const Texture*, std::vector<VkDescriptorSet>> mSpriteDescSetsVK
//      VkImageView mSpriteFallbackImageView
//      VkSampler   mSpriteFallbackSampler
//
// 注意:
//  - Texture -> VkImageView/VkSampler を引けない場合は fallback を使う
//======================================================================

#include "Render/VK/VKRenderer.h"

#include "Asset/Material/Texture.h"     // Texture 完全型
#include "Render/ITextureGPU.h"
#include "Render/VK/VKTextureGPU.h"     // VKTextureGPU の getter を使う

#include <iostream>
#include <vector>

namespace toy
{

//--------------------------------------------------------------
// Sprite用 DescriptorPool を lazily 作る
//--------------------------------------------------------------
bool VKRenderer::EnsureSpriteDescriptorPool()
{
    if (mSpriteDescPool != VK_NULL_HANDLE)
    {
        return true;
    }

    if (!mDevice)
    {
        return false;
    }

    const uint32_t scCount = (uint32_t)mSwapchainImageViews.size();
    if (scCount == 0)
    {
        std::cerr << "[VKRenderer] EnsureSpriteDescriptorPool: swapchain not ready.\n";
        return false;
    }

    // 「1 Texture = swapchain枚数分の set」を想定
    // 最小段階：とりあえず 256 Texture 分
    const uint32_t kMaxTextures   = 256;
    const uint32_t totalSets      = scCount * kMaxTextures;
    const uint32_t totalSamplers  = totalSets; // set あたり1 sampler

    VkDescriptorPoolSize ps{};
    ps.type            = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    ps.descriptorCount = totalSamplers;

    VkDescriptorPoolCreateInfo pci{};
    pci.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pci.flags         = 0; // FREE を使うなら VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT
    pci.maxSets       = totalSets;
    pci.poolSizeCount = 1;
    pci.pPoolSizes    = &ps;

    if (vkCreateDescriptorPool(mDevice, &pci, nullptr, &mSpriteDescPool) != VK_SUCCESS)
    {
        std::cerr << "[VKRenderer] EnsureSpriteDescriptorPool: vkCreateDescriptorPool failed.\n";
        return false;
    }

    return true;
}

//--------------------------------------------------------------
// TextureHandle -> VkImageView/VkSampler bridge
//
// 重要：Texture の public API は増やさない方針なので、Texture.h 側で
//   friend class VKRenderer;
// を追加して、VKRenderer だけが mGPU を覗ける前提。
//--------------------------------------------------------------
VkImageView VKRenderer::GetVkImageViewFromTextureHandle(TextureHandle h) const
{
    const Texture* tex = h.ptr;
    if (!tex)
    {
        return VK_NULL_HANDLE;
    }

    // Texture の GPU 実装を覗く（friend 前提）
    const ITextureGPU* gpu = tex->GetGPU();
    if (!gpu)
    {
        return VK_NULL_HANDLE;
    }

    // Vulkan 実装なら ImageView を返す
    if (auto* vkgpu = dynamic_cast<const VKTextureGPU*>(gpu))
    {
        return vkgpu->GetImageView();
    }

    return VK_NULL_HANDLE;
}

VkSampler VKRenderer::GetVkSamplerFromTextureHandle(TextureHandle h) const
{
    const Texture* tex = h.ptr;
    if (!tex)
    {
        return VK_NULL_HANDLE;
    }

    const ITextureGPU* gpu = tex->GetGPU();
    if (!gpu)
    {
        return VK_NULL_HANDLE;
    }

    if (auto* vkgpu = dynamic_cast<const VKTextureGPU*>(gpu))
    {
        return vkgpu->GetSampler();
    }

    return VK_NULL_HANDLE;
}

//--------------------------------------------------------------
// Sprite用 DescriptorSet を Texture* 単位でキャッシュ
// - swapchain枚数分をまとめて確保して、現在の mImageIndex の set を返す
//--------------------------------------------------------------
VkDescriptorSet VKRenderer::GetOrCreateSpriteDescSet(TextureHandle texH)
{
    if (!mDevice)
    {
        return VK_NULL_HANDLE;
    }

    const uint32_t scCount = (uint32_t)mSwapchainImageViews.size();
    if (scCount == 0)
    {
        std::cerr << "[VKRenderer] GetOrCreateSpriteDescSet: swapchain not ready.\n";
        return VK_NULL_HANDLE;
    }

    // Sprite pipeline が必要（setLayout をここから取る）
    auto itPipe = mPipelines.find("Sprite");
    if (itPipe == mPipelines.end() || !itPipe->second)
    {
        std::cerr << "[VKRenderer] GetOrCreateSpriteDescSet: Sprite pipeline missing.\n";
        return VK_NULL_HANDLE;
    }

    VKPipeline* pipe = itPipe->second.get();
    if (pipe->setLayout0 == VK_NULL_HANDLE)
    {
        std::cerr << "[VKRenderer] GetOrCreateSpriteDescSet: Sprite setLayout missing.\n";
        return VK_NULL_HANDLE;
    }

    if (!EnsureSpriteDescriptorPool())
    {
        return VK_NULL_HANDLE;
    }

    // null は “fallback” 扱い（SpriteComponentが未設定でも落ちない）
    const Texture* texPtr = texH.ptr;

    // キャッシュ hit
    {
        auto it = mSpriteDescSetsVK.find(texPtr);
        if (it != mSpriteDescSetsVK.end() && !it->second.empty())
        {
            const uint32_t idx = (mImageIndex < (uint32_t)it->second.size()) ? mImageIndex : 0;
            return it->second[idx];
        }
    }

    // swapchain枚数分 allocate
    std::vector<VkDescriptorSet> sets(scCount, VK_NULL_HANDLE);
    {
        std::vector<VkDescriptorSetLayout> layouts(scCount, pipe->setLayout0);

        VkDescriptorSetAllocateInfo ai{};
        ai.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        ai.descriptorPool     = mSpriteDescPool;
        ai.descriptorSetCount = scCount;
        ai.pSetLayouts        = layouts.data();

        const VkResult r = vkAllocateDescriptorSets(mDevice, &ai, sets.data());
        if (r != VK_SUCCESS)
        {
            std::cerr << "[VKRenderer] GetOrCreateSpriteDescSet: vkAllocateDescriptorSets failed: " << r << "\n";
            return VK_NULL_HANDLE;
        }
    }

    // VkImageView / VkSampler 解決（無いなら fallback）
    VkImageView view    = VK_NULL_HANDLE;
    VkSampler   sampler = VK_NULL_HANDLE;

    if (texPtr)
    {
        view    = GetVkImageViewFromTextureHandle(texH);
        sampler = GetVkSamplerFromTextureHandle(texH);

        // デバッグ（必要なら）
        // std::cerr << "[VKRenderer] GetVkImageViewFromTextureHandle: tex=" << (void*)texPtr
        //           << " view=" << (void*)view << "\n";
    }


    if (view == VK_NULL_HANDLE)
    {
        view = mSpriteFallbackImageView;
    }
    if (sampler == VK_NULL_HANDLE)
    {
        sampler = mSpriteFallbackSampler;
    }

    if (view == VK_NULL_HANDLE || sampler == VK_NULL_HANDLE)
    {
        std::cerr << "[VKRenderer] GetOrCreateSpriteDescSet: fallback imageView/sampler missing.\n";
        return VK_NULL_HANDLE;
    }

    // update（全部同じ view/sampler）
    for (uint32_t i = 0; i < scCount; ++i)
    {
        VkDescriptorImageInfo ii{};
        ii.sampler     = sampler;
        ii.imageView   = view;
        ii.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkWriteDescriptorSet w{};
        w.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w.dstSet          = sets[i];
        w.dstBinding      = 0;
        w.descriptorCount = 1;
        w.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        w.pImageInfo      = &ii;

        vkUpdateDescriptorSets(mDevice, 1, &w, 0, nullptr);
    }

    // cache
    mSpriteDescSetsVK.emplace(texPtr, sets);

    // current image
    const uint32_t idx = (mImageIndex < (uint32_t)sets.size()) ? mImageIndex : 0;
    return sets[idx];
}

} // namespace toy
