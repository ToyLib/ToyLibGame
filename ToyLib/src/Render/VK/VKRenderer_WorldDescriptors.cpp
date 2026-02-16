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
    if (imageCount == 0) return false;

    if (mWorldDescPool != VK_NULL_HANDLE && mWorldFrames.size() == imageCount)
    {
        // もう作ってある
        return true;
    }

    // 既存破棄（再生成の安全）
    DestroyWorldDescriptors();

    // pipeline setLayout1 は共有レイアウトを使う方針
    if (mWorldSetLayout1_Common == VK_NULL_HANDLE)
    {
        std::cerr << "[VK] mWorldSetLayout1_Common null\n";
        return false;
    }

    // pool (UBO *4 * imageCount)
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

    mWorldFrames.resize(imageCount);

    // allocate sets
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
        return false;
    }

    for (uint32_t i = 0; i < imageCount; ++i)
    {
        WorldFrameResources& fr = mWorldFrames[i];
        fr.descSet1_Common = sets[i];

        // per-image UBO create
        if (!CreateHostVisibleUBO(mPhysicalDevice, mDevice, sizeof(UBO_WorldCommon),
                                  fr.worldCommonUBO, fr.worldCommonMem)) return false;

        if (!CreateHostVisibleUBO(mPhysicalDevice, mDevice, sizeof(UBO_MaterialParams),
                                  fr.materialParamsUBO, fr.materialParamsMem)) return false;

        if (!CreateHostVisibleUBO(mPhysicalDevice, mDevice, sizeof(UBO_DirLight),
                                  fr.dirLightUBO, fr.dirLightMem)) return false;

        if (!CreateHostVisibleUBO(mPhysicalDevice, mDevice, sizeof(UBO_PointLightBlock),
                                  fr.pointLightUBO, fr.pointLightMem)) return false;

        // update descriptors binding 0..3 to fr.*UBO
        vkutil::WriteDesc_UBO(mDevice, fr.descSet1_Common, 0, fr.worldCommonUBO,    sizeof(UBO_WorldCommon));
        vkutil::WriteDesc_UBO(mDevice, fr.descSet1_Common, 1, fr.materialParamsUBO, sizeof(UBO_MaterialParams));
        vkutil::WriteDesc_UBO(mDevice, fr.descSet1_Common, 2, fr.dirLightUBO,       sizeof(UBO_DirLight));
        vkutil::WriteDesc_UBO(mDevice, fr.descSet1_Common, 3, fr.pointLightUBO,     sizeof(UBO_PointLightBlock));

        // 初期値
        UpdateWorldCommonUBO(i);
        UpdateDirLightUBO(i);
        UpdatePointLightUBO(i);
        // material は item 無いので 0 初期でもOK
    }

    return true;
}
void VKRenderer::DestroyWorldDescriptors()
{
    if (mDevice == VK_NULL_HANDLE)
    {
        mWorldDescPool = VK_NULL_HANDLE;
        mWorldFrames.clear();
        return;
    }

    if (mWorldDescPool != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorPool(mDevice, mWorldDescPool, nullptr);
        mWorldDescPool = VK_NULL_HANDLE;
    }

    auto destroyBuf = [&](VkBuffer& b, VkDeviceMemory& m)
    {
        if (b) vkDestroyBuffer(mDevice, b, nullptr);
        if (m) vkFreeMemory(mDevice, m, nullptr);
        b = VK_NULL_HANDLE;
        m = VK_NULL_HANDLE;
    };

    for (auto& fr : mWorldFrames)
    {
        destroyBuf(fr.worldCommonUBO,    fr.worldCommonMem);
        destroyBuf(fr.materialParamsUBO, fr.materialParamsMem);
        destroyBuf(fr.dirLightUBO,       fr.dirLightMem);
        destroyBuf(fr.pointLightUBO,     fr.pointLightMem);
        fr.descSet1_Common = VK_NULL_HANDLE; // pool destroyで無効化される
    }
    mWorldFrames.clear();
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
