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
    if (mDevice == VK_NULL_HANDLE || mPhysicalDevice == VK_NULL_HANDLE)
    {
        std::cerr << "[VK] EnsureSkinnedDescriptors: device not ready.\n";
        return false;
    }

    const uint32_t imageCount = (uint32_t)mSwapchainImages.size();
    if (imageCount == 0)
    {
        std::cerr << "[VK] EnsureSkinnedDescriptors: swapchain not ready.\n";
        return false;
    }

    if (mWorldSetLayout2_Bone == VK_NULL_HANDLE)
    {
        std::cerr << "[VK] EnsureSkinnedDescriptors: mWorldSetLayout2_Bone is null.\n";
        return false;
    }

    // （デバッグ）set=1 は別管理でも、swapchain image と frame 配列だけは整合しているべき
    if (mWorldFrames.size() != imageCount)
    {
        std::cerr << "[VK] EnsureSkinnedDescriptors: mWorldFrames size mismatch. worldFrames="
                  << mWorldFrames.size() << " imageCount=" << imageCount << "\n";
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
            if (sf.bonePaletteUBO == VK_NULL_HANDLE) return false;
            if (sf.bonePaletteMem == VK_NULL_HANDLE) return false;
        }
        return true;
    };

    if (ready())
    {
        return true;
    }

    DestroySkinnedDescriptors();

    //==========================================================
    // (1) DescriptorPool : UBO x1 / image (BonePalette only)
    //==========================================================
    {
        VkDescriptorPoolSize ps{};
        ps.type            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        ps.descriptorCount = imageCount; // 1 UBO per set

        VkDescriptorPoolCreateInfo pci{};
        pci.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pci.flags         = 0;
        pci.maxSets       = imageCount;
        pci.poolSizeCount = 1;
        pci.pPoolSizes    = &ps;

        VkResult r = vkCreateDescriptorPool(mDevice, &pci, nullptr, &mSkinnedDescPool);
        if (r != VK_SUCCESS || mSkinnedDescPool == VK_NULL_HANDLE)
        {
            std::cerr << "[VK] EnsureSkinnedDescriptors: vkCreateDescriptorPool failed. r=" << r << "\n";
            DestroySkinnedDescriptors();
            return false;
        }
    }

    //==========================================================
    // (2) Allocate descriptor sets (set=2 per swapchain image)
    //==========================================================
    std::vector<VkDescriptorSetLayout> layouts(imageCount, mWorldSetLayout2_Bone);
    std::vector<VkDescriptorSet> sets(imageCount, VK_NULL_HANDLE);

    {
        VkDescriptorSetAllocateInfo ai{};
        ai.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        ai.descriptorPool     = mSkinnedDescPool;
        ai.descriptorSetCount = imageCount;
        ai.pSetLayouts        = layouts.data();

        VkResult r = vkAllocateDescriptorSets(mDevice, &ai, sets.data());
        if (r != VK_SUCCESS)
        {
            std::cerr << "[VK] EnsureSkinnedDescriptors: vkAllocateDescriptorSets failed. r=" << r << "\n";
            DestroySkinnedDescriptors();
            return false;
        }
    }

    //==========================================================
    // (3) Create BonePalette UBO + Update set=2(binding=0)
    //==========================================================
    mSkinnedFrames.clear();
    mSkinnedFrames.resize(imageCount);

    const VkDeviceSize boneBytes = (VkDeviceSize)(sizeof(Matrix4) * 96);

    // 念のため：maxUniformBufferRange チェック（6144は普通OKだが安全）
    {
        VkPhysicalDeviceProperties props{};
        vkGetPhysicalDeviceProperties(mPhysicalDevice, &props);
        if (boneBytes > props.limits.maxUniformBufferRange)
        {
            std::cerr << "[VK] EnsureSkinnedDescriptors: boneBytes exceeds maxUniformBufferRange. boneBytes="
                      << boneBytes << " max=" << props.limits.maxUniformBufferRange << "\n";
            DestroySkinnedDescriptors();
            return false;
        }
    }

    for (uint32_t i = 0; i < imageCount; ++i)
    {
        auto& sf = mSkinnedFrames[i];
        sf.descSet2_Bone = sets[i];

        // --- create UBO
        if (!CreateHostVisibleUBO(mPhysicalDevice, mDevice, boneBytes, sf.bonePaletteUBO, sf.bonePaletteMem))
        {
            std::cerr << "[VK] EnsureSkinnedDescriptors: CreateHostVisibleUBO(BonePalette) failed. i=" << i << "\n";
            DestroySkinnedDescriptors();
            return false;
        }

        // --- hard safety checks (MoltenVK assert 対策)
        if (sf.descSet2_Bone == VK_NULL_HANDLE)
        {
            std::cerr << "[VK] EnsureSkinnedDescriptors: descSet2_Bone is null after alloc. i=" << i << "\n";
            DestroySkinnedDescriptors();
            return false;
        }
        if (sf.bonePaletteUBO == VK_NULL_HANDLE || sf.bonePaletteMem == VK_NULL_HANDLE)
        {
            std::cerr << "[VK] EnsureSkinnedDescriptors: bone buffer/mem null. i=" << i
                      << " buf=" << (void*)sf.bonePaletteUBO
                      << " mem=" << (void*)sf.bonePaletteMem << "\n";
            DestroySkinnedDescriptors();
            return false;
        }

        // --- init identity (so "bp を使うと消える" を避ける)
        {
            Matrix4 mats[96];
            for (int j = 0; j < 96; ++j) mats[j] = Matrix4::Identity;
            WriteUBO(mDevice, sf.bonePaletteMem, mats, sizeof(mats));
        }

        // --- descriptor write
        VkDescriptorBufferInfo bi{};
        bi.buffer = sf.bonePaletteUBO;
        bi.offset = 0;
        bi.range  = boneBytes; // ★VK_WHOLE_SIZE は避ける

        VkWriteDescriptorSet w{};
        w.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w.pNext           = nullptr;
        w.dstSet          = sf.descSet2_Bone;
        w.dstBinding      = 0; // set=2 binding=0
        w.dstArrayElement = 0;
        w.descriptorCount = 1;
        w.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        w.pImageInfo      = nullptr;
        w.pBufferInfo     = &bi;
        w.pTexelBufferView = nullptr;

        // --- debug (落ちるならここ直前まで出るはず)
        std::cerr << "[DBG] SkinnedDesc i=" << i
                  << " set2=" << (void*)sf.descSet2_Bone
                  << " buf="  << (void*)sf.bonePaletteUBO
                  << " mem="  << (void*)sf.bonePaletteMem
                  << " layout2=" << (void*)mWorldSetLayout2_Bone
                  << " bytes=" << (uint64_t)boneBytes
                  << "\n";

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

//======================================================================
// UpdateBonePaletteUBO
//  - set=2 binding=0 : BonePalette (mat4[96])
//  - imageIndex は “いま記録しているコマンドの swapchain image” を渡す
//======================================================================
void VKRenderer::UpdateBonePaletteUBO(uint32_t imageIndex, const Matrix4* palette, uint32_t paletteCount)
{
    if (!palette || paletteCount == 0) return;

    if (mSkinnedFrames.empty()) return;
    if (imageIndex >= (uint32_t)mSkinnedFrames.size()) return;

    auto& sf = mSkinnedFrames[imageIndex];
    if (sf.bonePaletteMem == VK_NULL_HANDLE) return;

    const uint32_t maxBones = 96;
    const uint32_t n = (paletteCount > maxBones) ? maxBones : paletteCount;

    // GPU に渡す配列（不足分は Identity）
    Matrix4 mats[maxBones];
    for (uint32_t i = 0; i < maxBones; ++i)
    {
        mats[i] = (i < n) ? palette[i] : Matrix4::Identity;
    }

    WriteUBO(mDevice, sf.bonePaletteMem, mats, sizeof(mats));
}

} // namespace toy
