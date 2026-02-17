//======================================================================
// VKRenderer_SkinnedDescriptors.cpp
//  - SkinnedMesh 用 set=1 (SkinnedWorld) を per-swapchain-image で用意する
//
// set=1 layout（例）:
//   binding=0 : WorldCommon (UBO)   ★これは mWorldFrames[i].worldCommonUBO を参照して共有
//   binding=1 : BonePalette (UBO)  ★Skinned専用（mSkinnedFrames[i].bonePaletteUBO）
//
// 前提（VKRenderer.h）:
//   VkDescriptorPool        mSkinnedDescPool { VK_NULL_HANDLE };
//   VkDescriptorSetLayout   mWorldSetLayout1_Skinned { VK_NULL_HANDLE }; // set=1
//
//   struct WorldFrameResources { VkBuffer worldCommonUBO; VkDeviceMemory worldCommonMem; ... };
//   std::vector<WorldFrameResources> mWorldFrames;
//
//   struct SkinnedFrameResources
//   {
//       VkDescriptorSet descSet1_Skinned { VK_NULL_HANDLE };
//       VkBuffer       bonePaletteUBO    { VK_NULL_HANDLE };
//       VkDeviceMemory bonePaletteMem    { VK_NULL_HANDLE };
//   };
//   std::vector<SkinnedFrameResources> mSkinnedFrames;
//
//   bool CreateHostVisibleUBO(VkPhysicalDevice, VkDevice, size_t, VkBuffer&, VkDeviceMemory&);
//   void DestroySkinnedDescriptors();  // 先に用意しておく（下に実装例あり）
//
//======================================================================

#include "Render/VK/VKRenderer.h"
#include "Render/VK/VKUtil.h"
#include "Render/VK/VKUBO.h"

#include <iostream>
#include <vector>
#include <array>

namespace toy
{

//======================================================================
// EnsureSkinnedDescriptors
//  - set=1 binding=0 : WorldCommon (共有: mWorldFrames[i].worldCommonUBO)
//  - set=1 binding=1 : BonePalette (Skinned専用)
//======================================================================
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

    // WorldCommon が揃っている必要
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

            if (sf.descSet1_Skinned == VK_NULL_HANDLE) return false;

            if (wf.worldCommonUBO == VK_NULL_HANDLE || wf.worldCommonMem == VK_NULL_HANDLE) return false;
            if (sf.bonePaletteUBO == VK_NULL_HANDLE || sf.bonePaletteMem == VK_NULL_HANDLE) return false;
        }
        return true;
    };

    if (ready()) return true;

    DestroySkinnedDescriptors();

    //==========================================================
    // (1) DescriptorPool（UBO x2 / image）
    //==========================================================
    {
        VkDescriptorPoolSize ps{};
        ps.type            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        ps.descriptorCount = imageCount * 2; // WorldCommon + BonePalette

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

    for (uint32_t i = 0; i < imageCount; ++i)
    {
        auto& sf = mSkinnedFrames[i];
        sf.descSet1_Skinned = sets[i];

        if (!CreateHostVisibleUBO(mPhysicalDevice, mDevice, boneBytes, sf.bonePaletteUBO, sf.bonePaletteMem))
        {
            std::cerr << "[VK] EnsureSkinnedDescriptors: CreateHostVisibleUBO(BonePalette) failed.\n";
            DestroySkinnedDescriptors();
            return false;
        }

        // binding=0 : WorldCommon (共有)
        VkDescriptorBufferInfo bi0{};
        bi0.buffer = mWorldFrames[i].worldCommonUBO;
        bi0.offset = 0;
        bi0.range  = worldBytes; // ★固定

        VkWriteDescriptorSet w0{};
        w0.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w0.dstSet          = sf.descSet1_Skinned;
        w0.dstBinding      = 0;
        w0.dstArrayElement = 0;
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
        w1.dstSet          = sf.descSet1_Skinned;
        w1.dstBinding      = 1; // ★shaderに合わせる
        w1.dstArrayElement = 0;
        w1.descriptorCount = 1;
        w1.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        w1.pBufferInfo     = &bi1;

        VkWriteDescriptorSet writes[2] = { w0, w1 };
        vkUpdateDescriptorSets(mDevice, 2, writes, 0, nullptr);
    }

    return ready();
}

//======================================================================
// DestroySkinnedDescriptors
//======================================================================
void VKRenderer::DestroySkinnedDescriptors()
{
    if (!mDevice) return;

    for (auto& f : mSkinnedFrames)
    {
        if (f.bonePaletteUBO) vkDestroyBuffer(mDevice, f.bonePaletteUBO, nullptr);
        if (f.bonePaletteMem) vkFreeMemory(mDevice, f.bonePaletteMem, nullptr);

        f.bonePaletteUBO   = VK_NULL_HANDLE;
        f.bonePaletteMem   = VK_NULL_HANDLE;
        f.descSet1_Skinned = VK_NULL_HANDLE;
    }
    mSkinnedFrames.clear();

    if (mSkinnedDescPool)
    {
        vkDestroyDescriptorPool(mDevice, mSkinnedDescPool, nullptr);
        mSkinnedDescPool = VK_NULL_HANDLE;
    }
}

} // namespace toy
