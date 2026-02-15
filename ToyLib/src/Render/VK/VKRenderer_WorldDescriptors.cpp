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
    if (mWorldDescPool != VK_NULL_HANDLE && !mWorldDescSets.empty())
    {
        return true;
    }

    auto it = mPipelines.find("Mesh");
    if (it == mPipelines.end() || !it->second)
    {
        std::cerr << "[VK] Mesh pipeline missing\n";
        return false;
    }

    VKPipeline* meshPipe = it->second.get();
    if (meshPipe->setLayout1 == VK_NULL_HANDLE)
    {
        std::cerr << "[VK] Mesh setLayout1 null\n";
        return false;
    }

    const uint32_t imageCount = (uint32_t)mSwapchainImages.size();
    if (imageCount == 0)
    {
        std::cerr << "[VK] swapchain not ready\n";
        return false;
    }

    // --- UBO buffers (単一) ----------------------------------------
    if (mWorldCommonUBO == VK_NULL_HANDLE)
    {
        if (!CreateHostVisibleUBO(mPhysicalDevice, mDevice,
                                 sizeof(UBO_WorldCommon),
                                 mWorldCommonUBO, mWorldCommonUBOMem))
        {
            return false;
        }
    }

    if (mMaterialParamsUBO == VK_NULL_HANDLE)
    {
        if (!CreateHostVisibleUBO(mPhysicalDevice, mDevice,
                                 sizeof(UBO_MaterialParams),
                                 mMaterialParamsUBO, mMaterialParamsUBOMem))
        {
            return false;
        }
    }

    if (mDirLightUBO == VK_NULL_HANDLE)
    {
        if (!CreateHostVisibleUBO(mPhysicalDevice, mDevice,
                                 sizeof(UBO_DirLight),
                                 mDirLightUBO, mDirLightUBOMem))
        {
            return false;
        }
    }

    if (mPointLightUBO == VK_NULL_HANDLE)
    {
        if (!CreateHostVisibleUBO(mPhysicalDevice, mDevice,
                                 sizeof(UBO_PointLightBlock),
                                 mPointLightUBO, mPointLightUBOMem))
        {
            return false;
        }
    }

    // --- Descriptor pool (UBO×4 * swapchain) ----------------------
    VkDescriptorPoolSize poolUBO{};
    poolUBO.type            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolUBO.descriptorCount = imageCount * 4;

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

    // --- Allocate sets --------------------------------------------
    mWorldDescSets.resize(imageCount);

    std::vector<VkDescriptorSetLayout> layouts(imageCount, meshPipe->setLayout1);

    VkDescriptorSetAllocateInfo ai{};
    ai.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    ai.descriptorPool     = mWorldDescPool;
    ai.descriptorSetCount = imageCount;
    ai.pSetLayouts        = layouts.data();

    if (vkAllocateDescriptorSets(mDevice, &ai, mWorldDescSets.data()) != VK_SUCCESS)
    {
        std::cerr << "[VK] world desc set alloc failed\n";
        return false;
    }

    // --- Update sets (binding 0..3) -------------------------------
    for (uint32_t i = 0; i < imageCount; ++i)
    {
        VkDescriptorSet set = mWorldDescSets[i];

        // binding0: WorldCommon
        vkutil::WriteDesc_UBO(mDevice, set, 0, mWorldCommonUBO, sizeof(UBO_WorldCommon));

        // binding1: MaterialParams
        vkutil::WriteDesc_UBO(mDevice, set, 1, mMaterialParamsUBO, sizeof(UBO_MaterialParams));

        // binding2: DirLight
        vkutil::WriteDesc_UBO(mDevice, set, 2, mDirLightUBO, sizeof(UBO_DirLight));

        // binding3: PointLight
        vkutil::WriteDesc_UBO(mDevice, set, 3, mPointLightUBO, sizeof(UBO_PointLightBlock));
    }

    // 初期値
    UpdateWorldCommonUBO(0);
    UpdateDirLightUBO();
    UpdatePointLightUBO();
    return true;
}

void VKRenderer::DestroyWorldDescriptors()
{
    if (mWorldDescPool)
    {
        vkDestroyDescriptorPool(mDevice, mWorldDescPool, nullptr);
        mWorldDescPool = VK_NULL_HANDLE;
    }
    mWorldDescSets.clear();

    auto destroyBuf = [&](VkBuffer& b, VkDeviceMemory& m)
    {
        if (b) vkDestroyBuffer(mDevice, b, nullptr);
        if (m) vkFreeMemory(mDevice, m, nullptr);
        b = VK_NULL_HANDLE;
        m = VK_NULL_HANDLE;
    };

    destroyBuf(mWorldCommonUBO,    mWorldCommonUBOMem);
    destroyBuf(mMaterialParamsUBO, mMaterialParamsUBOMem);
    destroyBuf(mDirLightUBO,       mDirLightUBOMem);
    destroyBuf(mPointLightUBO,     mPointLightUBOMem);
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
