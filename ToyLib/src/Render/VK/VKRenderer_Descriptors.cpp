// Render/VK/VKRenderer_Descriptors.cpp

#include "Render/VK/VKRenderer.h"
#include "Render/VK/VKPipeline.h"
#include "Render/VK/VKTextureGPU.h"
#include "Render/ITextureGPU.h"
#include "Asset/Material/Texture.h"

#include <iostream>
#include <vector>

namespace toy
{

static VKTextureGPU* AsVKTex(TextureHandle h)
{
    if (!h.ptr) return nullptr;

    ITextureGPU* gpu = h.ptr->GetGPU();
    if (!gpu) return nullptr;

    // あなたの実装では VKTextureGPU を使っている前提
    return dynamic_cast<VKTextureGPU*>(gpu);
}

VkDescriptorSet VKRenderer::GetOrCreateWorldTexDescSet(TextureHandle texH)
{
    // pool を先に作る
    if (mWorldTexDescPool == VK_NULL_HANDLE)
    {
        // だいたいでOK：必要なら増やす
        VkDescriptorPoolSize ps{};
        ps.type            = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        ps.descriptorCount = 1024;

        VkDescriptorPoolCreateInfo pci{};
        pci.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pci.poolSizeCount = 1;
        pci.pPoolSizes    = &ps;
        pci.maxSets       = 1024;

        if (vkCreateDescriptorPool(mDevice, &pci, nullptr, &mWorldTexDescPool) != VK_SUCCESS)
        {
            std::cerr << "[VK] world tex desc pool create failed\n";
            return VK_NULL_HANDLE;
        }
    }

    // dummy white を必ず用意
    if (!CreateDummyWhiteResources())
    {
        std::cerr << "[VK] dummy white create failed\n";
        return VK_NULL_HANDLE;
    }

    // texture が無い → dummy white の descriptor を返す（1個だけ作って使い回す）
    if (!texH.ptr)
    {
        if (mWorldTexDescSetDummyWhite != VK_NULL_HANDLE)
        {
            return mWorldTexDescSetDummyWhite;
        }

        auto itP = mPipelines.find("Mesh");
        if (itP == mPipelines.end() || !itP->second || itP->second->setLayout0 == VK_NULL_HANDLE)
        {
            std::cerr << "[VK] Mesh pipeline/layout0 missing\n";
            return VK_NULL_HANDLE;
        }

        VkDescriptorSetLayout layout = itP->second->setLayout0;

        VkDescriptorSetAllocateInfo ai{};
        ai.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        ai.descriptorPool     = mWorldTexDescPool;
        ai.descriptorSetCount = 1;
        ai.pSetLayouts        = &layout;

        if (vkAllocateDescriptorSets(mDevice, &ai, &mWorldTexDescSetDummyWhite) != VK_SUCCESS)
        {
            std::cerr << "[VK] dummy white desc set alloc failed\n";
            return VK_NULL_HANDLE;
        }

        VkDescriptorImageInfo ii{};
        ii.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        ii.imageView   = mDummyWhiteImageView;
        ii.sampler     = mDummyWhiteSampler;

        VkWriteDescriptorSet w{};
        w.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w.dstSet          = mWorldTexDescSetDummyWhite;
        w.dstBinding      = 0;
        w.descriptorCount = 1;
        w.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        w.pImageInfo      = &ii;

        vkUpdateDescriptorSets(mDevice, 1, &w, 0, nullptr);
        return mWorldTexDescSetDummyWhite;
    }

    // cache hit?
    auto it = mWorldTexDescSetCache.find(texH.ptr);
    if (it != mWorldTexDescSetCache.end())
    {
        return it->second;
    }

    // layout0
    auto itP = mPipelines.find("Mesh");
    if (itP == mPipelines.end() || !itP->second || itP->second->setLayout0 == VK_NULL_HANDLE)
    {
        std::cerr << "[VK] Mesh pipeline/layout0 missing\n";
        return VK_NULL_HANDLE;
    }

    VkDescriptorSetLayout layout = itP->second->setLayout0;

    // alloc
    VkDescriptorSet set = VK_NULL_HANDLE;
    VkDescriptorSetAllocateInfo ai{};
    ai.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    ai.descriptorPool     = mWorldTexDescPool;
    ai.descriptorSetCount = 1;
    ai.pSetLayouts        = &layout;

    if (vkAllocateDescriptorSets(mDevice, &ai, &set) != VK_SUCCESS)
    {
        std::cerr << "[VK] world tex desc set alloc failed\n";
        return VK_NULL_HANDLE;
    }

    // resolve view/sampler
    VkImageView view = VK_NULL_HANDLE;
    VkSampler   samp = VK_NULL_HANDLE;

    if (VKTextureGPU* vkTex = AsVKTex(texH))
    {
        view = vkTex->GetImageView();
        samp = vkTex->GetSampler();
    }

    // fallback
    if (view == VK_NULL_HANDLE || samp == VK_NULL_HANDLE)
    {
        view = mDummyWhiteImageView;
        samp = mDummyWhiteSampler;
    }

    VkDescriptorImageInfo ii{};
    ii.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    ii.imageView   = view;
    ii.sampler     = samp;

    VkWriteDescriptorSet w{};
    w.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    w.dstSet          = set;
    w.dstBinding      = 0;
    w.descriptorCount = 1;
    w.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    w.pImageInfo      = &ii;

    vkUpdateDescriptorSets(mDevice, 1, &w, 0, nullptr);

    // cache
    mWorldTexDescSetCache[texH.ptr] = set;
    return set;
}

} // namespace toy
