//======================================================================
// Render/VK/VKUniformSet.cpp
//======================================================================
#include "Render/VK/VKUniformSet.h"
#include "Render/VK/VKUtil.h"

#include <cstring>

namespace toy
{

bool VKUniformSet::Create(VkDevice device,
                          VkPhysicalDevice phys,
                          VkDescriptorPool pool,
                          VkDescriptorSetLayout layout,
                          size_t uboSize,
                          size_t frameCount)
{
    if (!device || !phys || !pool || layout == VK_NULL_HANDLE || uboSize == 0 || frameCount == 0)
    {
        return false;
    }

    mUBO.assign(frameCount, VK_NULL_HANDLE);
    mMem.assign(frameCount, VK_NULL_HANDLE);
    mSet.assign(frameCount, VK_NULL_HANDLE);

    for (size_t i = 0; i < frameCount; ++i)
    {
        if (!vkutil::CreateBuffer_HostVisible(phys, device, static_cast<VkDeviceSize>(uboSize),
                                              VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, mUBO[i], mMem[i]))
        {
            Destroy(device, pool);
            return false;
        }

        VkDescriptorSetAllocateInfo ai{};
        ai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        ai.descriptorPool = pool;
        ai.descriptorSetCount = 1;
        ai.pSetLayouts = &layout;

        if (vkAllocateDescriptorSets(device, &ai, &mSet[i]) != VK_SUCCESS)
        {
            Destroy(device, pool);
            return false;
        }

        vkutil::WriteDesc_UBO(device, mSet[i], /*binding=*/0, mUBO[i], static_cast<VkDeviceSize>(uboSize));
    }

    return true;
}

void VKUniformSet::Destroy(VkDevice device, VkDescriptorPool pool)
{
    if (device)
    {
        if (pool != VK_NULL_HANDLE)
        {
            for (auto& set : mSet)
            {
                if (set != VK_NULL_HANDLE)
                {
                    vkFreeDescriptorSets(device, pool, 1, &set);
                }
            }
        }

        for (size_t i = 0; i < mUBO.size(); ++i)
        {
            if (mUBO[i] != VK_NULL_HANDLE)
            {
                vkDestroyBuffer(device, mUBO[i], nullptr);
            }
            if (mMem[i] != VK_NULL_HANDLE)
            {
                vkFreeMemory(device, mMem[i], nullptr);
            }
        }
    }

    mUBO.clear();
    mMem.clear();
    mSet.clear();
}

void VKUniformSet::Upload(VkDevice device, uint32_t frameIndex, const void* data, size_t size) const
{
    if (!device || frameIndex >= mMem.size() || mMem[frameIndex] == VK_NULL_HANDLE || !data || size == 0)
    {
        return;
    }

    void* mapped = nullptr;
    if (vkMapMemory(device, mMem[frameIndex], 0, static_cast<VkDeviceSize>(size), 0, &mapped) != VK_SUCCESS)
    {
        return;
    }

    std::memcpy(mapped, data, size);
    vkUnmapMemory(device, mMem[frameIndex]);
}

VkDescriptorSet VKUniformSet::GetSet(uint32_t frameIndex) const
{
    if (frameIndex >= mSet.size())
    {
        return VK_NULL_HANDLE;
    }
    return mSet[frameIndex];
}

} // namespace toy
