// Render/VK/VKRenderer_WorldDescriptors.cpp

#include "Render/VK/VKRenderer.h"
#include "Render/VK/VKPipeline.h"
#include "Render/VK/VKUtil.h"
#include "Render/VK/VKUBO.h"

#include <iostream>
#include <vector>
#include <cstring>


namespace toy
{

bool VKRenderer::EnsureWorldDescriptors()
{
    const uint32_t imageCount = (uint32_t)mSwapchainImages.size();
    if (imageCount == 0)
    {
        std::cerr << "[VK] swapchain not ready\n";
        return false;
    }

    // 共有 set layout が必要（set=1）
    if (mWorldSetLayout1_Common == VK_NULL_HANDLE)
    {
        std::cerr << "[VK] mWorldSetLayout1_Common is null\n";
        return false;
    }

    // すでに作ってあるならOK（全image分が揃っていること）
    auto IsWorldFramesReady = [&]() -> bool
    {
        if (mWorldDescPool == VK_NULL_HANDLE) return false;
        if (mWorldFrames.size() != imageCount) return false;

        for (uint32_t i = 0; i < imageCount; ++i)
        {
            const WorldFrameResources& f = mWorldFrames[i];

            if (f.descSet1_Common == VK_NULL_HANDLE) return false;

            if (f.worldCommonUBO == VK_NULL_HANDLE || f.worldCommonMem == VK_NULL_HANDLE) return false;
            if (f.dirLightUBO    == VK_NULL_HANDLE || f.dirLightMem    == VK_NULL_HANDLE) return false;
            if (f.pointLightUBO  == VK_NULL_HANDLE || f.pointLightMem  == VK_NULL_HANDLE) return false;
        }
        return true;
    };

    if (IsWorldFramesReady())
    {
        return true;
    }

    // 既存があれば破棄して作り直す（swapchain recreate 対応）
    DestroyWorldDescriptors();

    // pool: UBO *3 * imageCount（0,2,3）
    VkDescriptorPoolSize poolUBO{};
    poolUBO.type            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolUBO.descriptorCount = imageCount * 3;

    VkDescriptorPoolCreateInfo pci{};
    pci.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pci.maxSets       = imageCount;
    pci.poolSizeCount = 1;
    pci.pPoolSizes    = &poolUBO;

    if (vkCreateDescriptorPool(mDevice, &pci, nullptr, &mWorldDescPool) != VK_SUCCESS)
    {
        std::cerr << "[VK] world desc pool create failed\n";
        return false;
    }

    // frames
    mWorldFrames.resize(imageCount);

    // allocate set=1 (common) per image
    std::vector<VkDescriptorSetLayout> layouts(imageCount, mWorldSetLayout1_Common);
    std::vector<VkDescriptorSet> sets(imageCount, VK_NULL_HANDLE);

    VkDescriptorSetAllocateInfo ai{};
    ai.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    ai.descriptorPool     = mWorldDescPool;
    ai.descriptorSetCount = imageCount;
    ai.pSetLayouts        = layouts.data();

    if (vkAllocateDescriptorSets(mDevice, &ai, sets.data()) != VK_SUCCESS)
    {
        std::cerr << "[VK] world desc set alloc failed\n";
        DestroyWorldDescriptors();
        return false;
    }

    // create UBOs per image + write descriptors
    for (uint32_t i = 0; i < imageCount; ++i)
    {
        WorldFrameResources& f = mWorldFrames[i];
        f.descSet1_Common = sets[i];

        if (!CreateHostVisibleUBO(mPhysicalDevice, mDevice, sizeof(UBO_WorldCommon),
                                  f.worldCommonUBO, f.worldCommonMem))
        {
            DestroyWorldDescriptors();
            return false;
        }

        if (!CreateHostVisibleUBO(mPhysicalDevice, mDevice, sizeof(UBO_DirLight),
                                  f.dirLightUBO, f.dirLightMem))
        {
            DestroyWorldDescriptors();
            return false;
        }

        if (!CreateHostVisibleUBO(mPhysicalDevice, mDevice, sizeof(UBO_PointLightBlock),
                                  f.pointLightUBO, f.pointLightMem))
        {
            DestroyWorldDescriptors();
            return false;
        }

        // binding 0,2,3（binding=1 は空席のまま維持）
        vkutil::WriteDesc_UBO(mDevice, f.descSet1_Common, 0, f.worldCommonUBO, sizeof(UBO_WorldCommon));
        vkutil::WriteDesc_UBO(mDevice, f.descSet1_Common, 2, f.dirLightUBO,    sizeof(UBO_DirLight));
        vkutil::WriteDesc_UBO(mDevice, f.descSet1_Common, 3, f.pointLightUBO,  sizeof(UBO_PointLightBlock));
    }

    // 初期値（未初期化参照を潰す）
    for (uint32_t i = 0; i < imageCount; ++i)
    {
        UpdateWorldCommonUBO(i);
        UpdateDirLightUBO(i);
        UpdatePointLightUBO(i);
    }

    return true;
}

void VKRenderer::DestroyWorldDescriptors()
{
    // per-frame UBOs
    auto destroyBuf = [&](VkBuffer& b, VkDeviceMemory& m)
    {
        if (b) vkDestroyBuffer(mDevice, b, nullptr);
        if (m) vkFreeMemory(mDevice, m, nullptr);
        b = VK_NULL_HANDLE;
        m = VK_NULL_HANDLE;
    };

    for (auto& f : mWorldFrames)
    {
        destroyBuf(f.worldCommonUBO,    f.worldCommonMem);
        destroyBuf(f.dirLightUBO,       f.dirLightMem);
        destroyBuf(f.pointLightUBO,     f.pointLightMem);
        f.descSet1_Common = VK_NULL_HANDLE;
    }
    mWorldFrames.clear();

    // pool（descriptor set は pool破棄でまとめて無効化される）
    if (mWorldDescPool)
    {
        vkDestroyDescriptorPool(mDevice, mWorldDescPool, nullptr);
        mWorldDescPool = VK_NULL_HANDLE;
    }
}

bool VKRenderer::CreateHostVisibleUBO(VkPhysicalDevice /*phys*/,
                                     VkDevice /*device*/,
                                     VkDeviceSize size,
                                     VkBuffer& outBuf,
                                     VkDeviceMemory& outMem)
{
    // 既に作られてたら何もしない（呼び出し側で弾いてるなら不要だが安全）
    if (outBuf != VK_NULL_HANDLE && outMem != VK_NULL_HANDLE)
    {
        return true;
    }

    return CreateBuffer(
        size,
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        outBuf,
        outMem);
}
} // namespace toy
