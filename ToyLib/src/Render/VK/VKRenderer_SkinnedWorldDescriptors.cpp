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

static inline void WriteUBO(VkDevice dev, VkDeviceMemory mem, const void* src, size_t bytes)
{
    void* dst = nullptr;
    if (vkMapMemory(dev, mem, 0, bytes, 0, &dst) != VK_SUCCESS)
    {
        std::cerr << "[VK] vkMapMemory failed\n";
        return;
    }
    std::memcpy(dst, src, bytes);
    vkUnmapMemory(dev, mem);
}

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

    // set=2 layout（BonePalette 専用）が必要
    if (mWorldSetLayout2_Bone == VK_NULL_HANDLE)
    {
        std::cerr << "[VK] EnsureSkinnedDescriptors: mWorldSetLayout2_Bone is null.\n";
        return false;
    }

    // set=1(common) は別系統（EnsureWorldDescriptors）で作る前提だが、
    // WorldFrames が揃ってることだけはチェックしておく（デバッグ用）
    if (mWorldFrames.size() != imageCount)
    {
        std::cerr << "[VK] EnsureSkinnedDescriptors: mWorldFrames not ready (size mismatch).\n";
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
    // (1) DescriptorPool : UBO x1 / image（BonePalette だけ）
    //==========================================================
    {
        VkDescriptorPoolSize ps{};
        ps.type            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        ps.descriptorCount = imageCount * 1;

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
    // (2) allocate sets : set=2 を per-image
    //==========================================================
    std::vector<VkDescriptorSetLayout> layouts(imageCount, mWorldSetLayout2_Bone);
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
    // (3) BonePalette UBO 作成 + set=2(binding=0) 更新
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

        // 初期化（Identity）— “bp を使うと消える”対策
        {
            Matrix4 mats[96];
            for (int j = 0; j < 96; ++j) mats[j] = Matrix4::Identity;
            WriteUBO(mDevice, sf.bonePaletteMem, mats, sizeof(mats));
        }

        // set=2 binding=0 : BonePalette
        VkDescriptorBufferInfo bi{};
        bi.buffer = sf.bonePaletteUBO;
        bi.offset = 0;
        bi.range  = boneBytes;

        VkWriteDescriptorSet w{};
        w.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w.dstSet          = sf.descSet2_Bone;
        w.dstBinding      = 0; // ★set=2 の layout と shader に合わせる
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
