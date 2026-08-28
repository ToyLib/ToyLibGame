//======================================================================
// Render/VK/VKUniformSet.h
//
// VKRenderer には「frame数ぶんの UBO(VkBuffer+VkDeviceMemory) と
// DescriptorSet」を1セットとして扱う箇所が Scene(World/UI) / Sky /
// Overlay / Shadow(cascade) / SceneCapture など何度も出てくる。
// 同じ形の生成/更新/破棄コードがコピペされていたのをここに集約する。
//======================================================================
#pragma once

#include <vulkan/vulkan.h>

#include <cstddef>
#include <vector>

namespace toy
{

class VKUniformSet
{
public:
    // device/phys から frameCount ぶんの host-visible UBO を作り、
    // pool から layout の DescriptorSet を確保して binding=0 (UBO) に書き込む。
    bool Create(VkDevice device,
                VkPhysicalDevice phys,
                VkDescriptorPool pool,
                VkDescriptorSetLayout layout,
                size_t uboSize,
                size_t frameCount);

    // UBO(Buffer/Memory)とDescriptorSetを破棄する。
    // pool がまだ有効なうちに呼ぶこと（DescriptorPool破棄後に呼ぶ場合は
    // pool=VK_NULL_HANDLE を渡す＝vkFreeDescriptorSetsをスキップする）。
    void Destroy(VkDevice device, VkDescriptorPool pool);

    // frameIndex 番目の UBO に data を書き込む（host-visible なので map/memcpy/unmap）。
    void Upload(VkDevice device, uint32_t frameIndex, const void* data, size_t size) const;

    VkDescriptorSet GetSet(uint32_t frameIndex) const;

    bool IsCreated() const
    {
        return !mUBO.empty();
    }

    size_t FrameCount() const
    {
        return mUBO.size();
    }

private:
    std::vector<VkBuffer> mUBO;
    std::vector<VkDeviceMemory> mMem;
    std::vector<VkDescriptorSet> mSet;
};

} // namespace toy
