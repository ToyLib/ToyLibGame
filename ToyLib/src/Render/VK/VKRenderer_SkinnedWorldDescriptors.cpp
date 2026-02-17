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

    if (mWorldSetLayout1_Skinned == VK_NULL_HANDLE)
    {
        std::cerr << "[VK] EnsureSkinnedDescriptors: mWorldSetLayout1_Skinned is null.\n";
        return false;
    }

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
            const auto& wf = mWorldFrames[i];
            const auto& sf = mSkinnedFrames[i];

            if (sf.descSet2_Bone == VK_NULL_HANDLE) return false;

            // World common
            if (wf.worldCommonUBO == VK_NULL_HANDLE || wf.worldCommonMem == VK_NULL_HANDLE) return false;

            // Lights (あなたの WorldFrames 側の名前に合わせてください)
            if (wf.dirLightUBO == VK_NULL_HANDLE || wf.dirLightMem == VK_NULL_HANDLE) return false;
            if (wf.pointLightUBO == VK_NULL_HANDLE || wf.pointLightMem == VK_NULL_HANDLE) return false;

            // Bone palette
            if (sf.bonePaletteUBO == VK_NULL_HANDLE || sf.bonePaletteMem == VK_NULL_HANDLE) return false;
        }
        return true;
    };

    if (ready()) return true;

    DestroySkinnedDescriptors();

    //==========================================================
    // (1) DescriptorPool
    //==========================================================
    {
        VkDescriptorPoolSize ps{};
        ps.type            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        ps.descriptorCount = imageCount * 4; // WorldCommon + BonePalette + Dir + Point

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
    // (2) allocate sets
    //==========================================================
    std::vector<VkDescriptorSetLayout> layouts(imageCount, mWorldSetLayout1_Skinned);
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
    // (3) bone UBO + update
    //==========================================================
    mSkinnedFrames.clear();
    mSkinnedFrames.resize(imageCount);

    const VkDeviceSize worldBytes = sizeof(UBO_WorldCommon);
    const VkDeviceSize boneBytes  = (VkDeviceSize)(sizeof(Matrix4) * 96);
    const VkDeviceSize dirBytes   = sizeof(UBO_DirLight);
    const VkDeviceSize ptBytes    = sizeof(UBO_PointLightBlock);

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

        // ★超重要：初期化（Identity）
        {
            Matrix4 mats[96];
            for (int j = 0; j < 96; ++j)
            {
                mats[j] = Matrix4::Identity;
            }
            WriteUBO(mDevice, sf.bonePaletteMem, mats, sizeof(mats));
        }

        // binding=0 : WorldCommon (共有)
        VkDescriptorBufferInfo bi0{};
        bi0.buffer = mWorldFrames[i].worldCommonUBO;
        bi0.offset = 0;
        bi0.range  = worldBytes;

        VkWriteDescriptorSet w0{};
        w0.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w0.dstSet          = sf.descSet2_Bone;
        w0.dstBinding      = 0;
        w0.descriptorCount = 1;
        w0.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        w0.pBufferInfo     = &bi0;

        // binding=1 : BonePalette
        VkDescriptorBufferInfo bi1{};
        bi1.buffer = sf.bonePaletteUBO;
        bi1.offset = 0;
        bi1.range  = boneBytes;

        VkWriteDescriptorSet w1{};
        w1.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w1.dstSet          = sf.descSet2_Bone;
        w1.dstBinding      = 1;
        w1.descriptorCount = 1;
        w1.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        w1.pBufferInfo     = &bi1;

        // binding=2 : DirLight (共有)
        VkDescriptorBufferInfo bi2{};
        bi2.buffer = mWorldFrames[i].dirLightUBO;
        bi2.offset = 0;
        bi2.range  = dirBytes;

        VkWriteDescriptorSet w2{};
        w2.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w2.dstSet          = sf.descSet2_Bone;
        w2.dstBinding      = 2;
        w2.descriptorCount = 1;
        w2.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        w2.pBufferInfo     = &bi2;

        // binding=3 : PointLight (共有)
        VkDescriptorBufferInfo bi3{};
        bi3.buffer = mWorldFrames[i].pointLightUBO;
        bi3.offset = 0;
        bi3.range  = ptBytes;

        VkWriteDescriptorSet w3{};
        w3.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w3.dstSet          = sf.descSet2_Bone;
        w3.dstBinding      = 3;
        w3.descriptorCount = 1;
        w3.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        w3.pBufferInfo     = &bi3;

        VkWriteDescriptorSet writes[4] = { w0, w1, w2, w3 };
        vkUpdateDescriptorSets(mDevice, 4, writes, 0, nullptr);
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
