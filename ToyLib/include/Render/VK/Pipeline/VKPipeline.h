#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <string>

namespace toy
{

struct VKPipelineDesc
{
    // 基本
    VkRenderPass renderPass = VK_NULL_HANDLE;
    uint32_t     subpass    = 0;

    // シェーダ（SPIR-V）
    std::string vertSpvPath;
    std::string fragSpvPath;

    // 固定機能（当面これだけでOK）
    VkPrimitiveTopology topology  = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    VkCullModeFlags     cullMode  = VK_CULL_MODE_BACK_BIT;
    VkFrontFace         frontFace = VK_FRONT_FACE_CLOCKWISE; // ←今回の学び
    VkBool32            depthTest  = VK_TRUE;
    VkBool32            depthWrite = VK_TRUE;

    // ブレンド（Spriteは alpha blend を使いたいので後でON）
    VkBool32 enableAlphaBlend = VK_FALSE;

    // vertex input（まずはSpriteだけ / Meshは後で拡張）
    std::vector<VkVertexInputBindingDescription>   bindings;
    std::vector<VkVertexInputAttributeDescription> attributes;

    // descriptor set layouts（まず空でOK→後で追加）
    std::vector<VkDescriptorSetLayout> setLayouts;

    // push constants（まず無しでOK→後で追加）
    std::vector<VkPushConstantRange> pushConstants;
};

class VKPipeline
{
public:
    VKPipeline() = default;
    ~VKPipeline() { Destroy(); }

    bool Create(VkDevice device, const VKPipelineDesc& desc);
    void Destroy();

    VkPipeline       GetPipeline() const { return mPipeline; }
    VkPipelineLayout GetLayout()   const { return mLayout; }

private:
    VkDevice         mDevice   = VK_NULL_HANDLE;
    VkPipeline       mPipeline = VK_NULL_HANDLE;
    VkPipelineLayout mLayout   = VK_NULL_HANDLE;

    // internal
    bool CreateLayout(const VKPipelineDesc& desc);
};

} // namespace toy
