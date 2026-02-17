//======================================================================
// VKRenderer_SkinnedDescriptors.cpp（修正版：set=2 boneだけ）
//======================================================================

#include "Render/VK/VKRenderer.h"
#include "Render/VK/VKUtil.h"
#include "Render/VK/VKUBO.h"

#include <iostream>
#include <vector>

namespace toy
{

bool VKRenderer::EnsureSkinnedDescriptors()
{
    if (!mDevice || !mPhysicalDevice)
    {
        return false;
    }

    const uint32_t imageCount = (uint32_t)mSwapchainImages.size();
    if (imageCount == 0)
    {
        std::cerr << "[VK] EnsureSkinnedDescriptors: swapchain not ready.\n";
        return false;
    }

    // set=2 layout
    if (mWorldSetLayout2_BonePalette == VK_NULL_HANDLE)
    {
        std::cerr << "[VK] EnsureSkinnedDescriptors: mWorldSetLayout2_BonePalette is null.\n";
        return false;
    }

    // WorldCommon set=1 が先に揃ってる必要（descSet1_Common を使うため）
    if (mWorldFrames.size() != imageCount)
    {
        std::cerr << "[VK] EnsureSkinnedDescriptors: mWorldFrames not ready.\n";
        return false;
    }

    auto ready = [&]() -> bool
    {
        if (mSkinnedDescPool == VK_NULL_HANDLE) return false;
        if (mSkinnedFrames.size() != imageCount) return false;

        for (uint32_t i = 0; i < imageCount; ++i)
        {
            const auto& sf = mSkinnedFrames[i];
            if (sf.descSet2_Bone == VK_NULL_HANDLE) return false;
            if (sf.bonePaletteUBO == VK_NULL_HANDLE || sf.bonePaletteMem == VK_NULL_HANDLE) return false;
        }
        return true;
    };

    if (ready()) return true;

    DestroySkinnedDescriptors();

    //==========================================================
    // pool（UBO x1 / image）
    //==========================================================
    {
        VkDescriptorPoolSize ps{};
        ps.type            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        ps.descriptorCount = imageCount; // bone only

        VkDescriptorPoolCreateInfo pci{};
        pci.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pci.maxSets       = imageCount;
        pci.poolSizeCount = 1;
        pci.pPoolSizes    = &ps;

        if (vkCreateDescriptorPool(mDevice, &pci, nullptr, &mSkinnedDescPool) != VK_SUCCESS)
        {
            std::cerr << "[VK] EnsureSkinnedDescriptors: vkCreateDescriptorPool failed.\n";
            DestroySkinnedDescriptors();
            return false;
        }
    }

    //==========================================================
    // allocate set=2
    //==========================================================
    std::vector<VkDescriptorSetLayout> layouts(imageCount, mWorldSetLayout2_BonePalette);
    std::vector<VkDescriptorSet> sets(imageCount, VK_NULL_HANDLE);

    {
        VkDescriptorSetAllocateInfo ai{};
        ai.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        ai.descriptorPool     = mSkinnedDescPool;
        ai.descriptorSetCount = imageCount;
        ai.pSetLayouts        = layouts.data();

        if (vkAllocateDescriptorSets(mDevice, &ai, sets.data()) != VK_SUCCESS)
        {
            std::cerr << "[VK] EnsureSkinnedDescriptors: vkAllocateDescriptorSets failed.\n";
            DestroySkinnedDescriptors();
            return false;
        }
    }

    //==========================================================
    // bone UBO + update（set=2 binding=0）
    //==========================================================
    mSkinnedFrames.clear();
    mSkinnedFrames.resize(imageCount);

    const VkDeviceSize boneBytes = (VkDeviceSize)(sizeof(Matrix4) * 96);

    for (uint32_t i = 0; i < imageCount; ++i)
    {
        auto& sf = mSkinnedFrames[i];
        sf.descSet2_Bone = sets[i];

        if (!CreateHostVisibleUBO(mPhysicalDevice, mDevice, boneBytes, sf.bonePaletteUBO, sf.bonePaletteMem))
        {
            std::cerr << "[VK] EnsureSkinnedDescriptors: CreateHostVisibleUBO(BonePalette) failed.\n";
            DestroySkinnedDescriptors();
            return false;
        }

        VkDescriptorBufferInfo bi{};
        bi.buffer = sf.bonePaletteUBO;
        bi.offset = 0;
        bi.range  = boneBytes;

        VkWriteDescriptorSet w{};
        w.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w.dstSet          = sf.descSet2_Bone;
        w.dstBinding      = 0; // set=2 binding=0
        w.dstArrayElement = 0;
        w.descriptorCount = 1;
        w.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        w.pBufferInfo     = &bi;

        vkUpdateDescriptorSets(mDevice, 1, &w, 0, nullptr);
    }

    return ready();
}

void VKRenderer::DestroySkinnedDescriptors()
{
    if (!mDevice) return;

    for (auto& f : mSkinnedFrames)
    {
        if (f.bonePaletteUBO) vkDestroyBuffer(mDevice, f.bonePaletteUBO, nullptr);
        if (f.bonePaletteMem) vkFreeMemory(mDevice, f.bonePaletteMem, nullptr);

        f.bonePaletteUBO  = VK_NULL_HANDLE;
        f.bonePaletteMem  = VK_NULL_HANDLE;
        f.descSet2_Bone   = VK_NULL_HANDLE;
    }
    mSkinnedFrames.clear();

    if (mSkinnedDescPool)
    {
        vkDestroyDescriptorPool(mDevice, mSkinnedDescPool, nullptr);
        mSkinnedDescPool = VK_NULL_HANDLE;
    }
}

} // namespace toy
