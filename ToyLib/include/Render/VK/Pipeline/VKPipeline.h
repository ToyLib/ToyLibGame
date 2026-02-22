// Render/VK/VKPipeline.h
#pragma once

#include <vulkan/vulkan.h>
#include <string>
#include <vector>
#include <cstdint>

namespace toy
{

struct VKDescriptorBindingDesc
{
    uint32_t            binding = 0;
    VkDescriptorType    type    = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uint32_t            count   = 1;
    VkShaderStageFlags  stages  = 0;
};

struct VKDescriptorSetLayoutDesc
{
    uint32_t set = 0; // set index
    std::vector<VKDescriptorBindingDesc> bindings {};
};

struct VKPushConstantDesc
{
    VkShaderStageFlags stages = 0;
    uint32_t offset = 0;
    uint32_t size   = 0;
};

struct VKPipelineDesc
{
    // shader paths (spv)
    std::string vsPath;
    std::string fsPath;

    // fixed states
    bool depthTest  = true;
    bool depthWrite = true;

    bool alphaBlend = false;

    VkCullModeFlags cullMode = VK_CULL_MODE_BACK_BIT;

    // ★重要：ここが “見えない原因” の本丸になりやすい
    //   GLと同じ頂点並びでも、投影のY反転や座標系で表裏が逆転する。
    VkFrontFace frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

    enum class VertexLayout
    {
        Sprite_Pos3Nrm3Uv2, // 8 floats interleaved (32 bytes)
        Mesh_Pos3Nrm3Uv2,   // same as sprite for now
        Skinned_Pos3Nrm3Uv2_Bone4U32_Weight4, // 64 bytes
        Vec2_Pos2           // 2 floats interleaved (8 bytes)
    };

    VertexLayout layout = VertexLayout::Sprite_Pos3Nrm3Uv2;
    
    // ★追加：pipeline layout を Desc で駆動するための set layout 情報
    std::vector<VKDescriptorSetLayoutDesc> setLayouts {};
    
    // ★追加：PipelineLayout の push constant ranges
    std::vector<VKPushConstantDesc> pushConstants {};
};

class VKPipeline
{
public:
    VKPipeline() = default;
    ~VKPipeline();

    VKPipeline(const VKPipeline&) = delete;
    VKPipeline& operator=(const VKPipeline&) = delete;

    bool Create(VkDevice device,
                VkRenderPass renderPass,
                VkExtent2D extent,
                const VKPipelineDesc& desc);

    void Destroy();

    bool IsValid() const { return mPipeline != VK_NULL_HANDLE; }

    VkPipeline       GetPipeline()       const { return mPipeline; }
    VkPipelineLayout GetPipelineLayout() const { return mLayout;   }

    void Bind(VkCommandBuffer cmd) const;

private:
    VkDevice         mDevice   { VK_NULL_HANDLE };
    VkPipeline       mPipeline { VK_NULL_HANDLE };
    VkPipelineLayout mLayout   { VK_NULL_HANDLE };


    std::vector<VkDescriptorSetLayout> mSetLayouts {};
private:
    static void BuildVertexInput(VKPipelineDesc::VertexLayout layout,
                                 VkVertexInputBindingDescription& outBinding,
                                 std::vector<VkVertexInputAttributeDescription>& outAttrs);

    static VkShaderModule LoadShaderModule(VkDevice device, const std::string& spvPath);
    
    bool CreateDescriptorSetLayouts(const VKPipelineDesc& desc);

};

} // namespace toy
