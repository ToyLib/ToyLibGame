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

#include <iostream>
#include <vector>
#include <array>

namespace toy
{

bool VKRenderer::EnsureSkinnedDescriptors()
{
    if (!mDevice) return false;

    const uint32_t imageCount = static_cast<uint32_t>(mSwapchainImages.size());
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

    // WorldCommon を共有するので、mWorldFrames が揃っている必要がある
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

            // binding=0 の参照元（共有）
            if (wf.worldCommonUBO == VK_NULL_HANDLE || wf.worldCommonMem == VK_NULL_HANDLE) return false;

            // binding=1（Skinned専用）
            if (sf.bonePaletteUBO == VK_NULL_HANDLE || sf.bonePaletteMem == VK_NULL_HANDLE) return false;
        }
        return true;
    };

    if (ready()) return true;

    // 既存を破棄して作り直す（recreate対応の最低限）
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
        pci.maxSets       = imageCount; // set=1 を imageCount個
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
    // (2) DescriptorSet allocate（set=1 を per-image）
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
    // (3) BonePalette UBO 作成 + set 更新
    //==========================================================
    mSkinnedFrames.resize(imageCount);

    for (uint32_t i = 0; i < imageCount; ++i)
    {
        auto& sf = mSkinnedFrames[i];
        sf.descSet1_Skinned = sets[i];

        // BonePalette（サイズはあなたの実装に合わせて）
        // 例：96本 mat4 → 96 * 64 = 6144 bytes
        const size_t kBonePaletteBytes = sizeof(Matrix4) * 96;

        if (!CreateHostVisibleUBO(mPhysicalDevice, mDevice,
                                  kBonePaletteBytes,
                                  sf.bonePaletteUBO,
                                  sf.bonePaletteMem))
        {
            std::cerr << "[VK] EnsureSkinnedDescriptors: CreateHostVisibleUBO(bonePalette) failed.\n";
            DestroySkinnedDescriptors();
            return false;
        }

        // binding=0 : WorldCommon（共有。mWorldFrames[i] を参照）
        VkDescriptorBufferInfo bi0{};
        bi0.buffer = mWorldFrames[i].worldCommonUBO;
        bi0.offset = 0;
        bi0.range  = VK_WHOLE_SIZE;

        VkWriteDescriptorSet w0{};
        w0.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w0.dstSet          = sf.descSet1_Skinned;
        w0.dstBinding      = 0;
        w0.descriptorCount = 1;
        w0.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        w0.pBufferInfo     = &bi0;

        // binding=1 : BonePalette（Skinned専用）
        VkDescriptorBufferInfo bi1{};
        bi1.buffer = sf.bonePaletteUBO;
        bi1.offset = 0;
        bi1.range  = kBonePaletteBytes;

        VkWriteDescriptorSet w1{};
        w1.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w1.dstSet          = sf.descSet1_Skinned;
        w1.dstBinding      = 1;
        w1.descriptorCount = 1;
        w1.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        w1.pBufferInfo     = &bi1;

        std::array<VkWriteDescriptorSet, 2> writes = { w0, w1 };
        vkUpdateDescriptorSets(mDevice,
                               static_cast<uint32_t>(writes.size()),
                               writes.data(),
                               0, nullptr);
    }

    return ready();
}

//--------------------------------------------------------------
// 破棄（recreate対応の最低限）
//--------------------------------------------------------------
void VKRenderer::DestroySkinnedDescriptors()
{
    if (!mDevice) return;

    for (auto& sf : mSkinnedFrames)
    {
        if (sf.bonePaletteUBO)
        {
            vkDestroyBuffer(mDevice, sf.bonePaletteUBO, nullptr);
            sf.bonePaletteUBO = VK_NULL_HANDLE;
        }
        if (sf.bonePaletteMem)
        {
            vkFreeMemory(mDevice, sf.bonePaletteMem, nullptr);
            sf.bonePaletteMem = VK_NULL_HANDLE;
        }
        sf.descSet1_Skinned = VK_NULL_HANDLE;
    }
    mSkinnedFrames.clear();

    if (mSkinnedDescPool)
    {
        vkDestroyDescriptorPool(mDevice, mSkinnedDescPool, nullptr);
        mSkinnedDescPool = VK_NULL_HANDLE;
    }
}

} // namespace toy
