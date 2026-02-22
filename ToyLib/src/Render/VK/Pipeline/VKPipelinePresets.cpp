// Render/VK/Pipeline/VKPipelinePresets.cpp
#include "Render/VK/Pipeline/VKPipelinePresets.h"

namespace toy
{

namespace
{
    inline void AddDefaultSet0_SceneUBO(VKPipelineDesc& d)
    {
        VKDescriptorSetLayoutDesc set0{};
        set0.set = 0;
        set0.bindings.push_back({
            .binding = 0,
            .type    = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .count   = 1,
            .stages  = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT
        });
        d.setLayouts.push_back(set0);
    }

    inline void AddDefaultPushConstant(VKPipelineDesc& d)
    {
        VKPushConstantDesc pc{};
        pc.stages = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        pc.offset = 0;
        pc.size   = 128; // 仮：本番は sizeof(YourPCStruct)
        d.pushConstants.push_back(pc);
    }
}

namespace VKPipelinePresets
{

VKPipelineDesc MakeSprite(const std::string& base)
{
    VKPipelineDesc d{};
    d.vsPath     = base + "Sprite.vert.spv";
    d.fsPath     = base + "Sprite.frag.spv";
    d.layout     = VKPipelineDesc::VertexLayout::Sprite_Pos3Nrm3Uv2;

    d.depthTest  = true;
    d.depthWrite = true;
    d.alphaBlend = true;

    d.cullMode   = VK_CULL_MODE_BACK_BIT;
    d.frontFace  = VK_FRONT_FACE_CLOCKWISE;

    // set=0 Scene UBO
    {
        VKDescriptorSetLayoutDesc set0{};
        set0.set = 0;
        set0.bindings.push_back({
            .binding = 0,
            .type    = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .count   = 1,
            .stages  = VK_SHADER_STAGE_VERTEX_BIT
        });
        d.setLayouts.push_back(set0);
    }

    // set=1 baseMap sampler2D
    {
        VKDescriptorSetLayoutDesc set1{};
        set1.set = 1;
        set1.bindings.push_back({
            .binding = 0,
            .type    = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .count   = 1,
            .stages  = VK_SHADER_STAGE_FRAGMENT_BIT
        });
        d.setLayouts.push_back(set1);
    }

    // PC : mat4(64) + vec4(16) = 80
    {
        VKPushConstantDesc pc{};
        pc.stages = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        pc.offset = 0;
        pc.size   = 80;
        d.pushConstants.push_back(pc);
    }

    return d;
}

VKPipelineDesc MakeMesh(const std::string& base)
{
    
    VKPipelineDesc d = MakeSprite(base);
    /*
    // Mesh 用に差分だけ上書き
    d.layout     = VKPipelineDesc::VertexLayout::Mesh_Pos3Nrm3Uv2;
    d.alphaBlend = false;

    // Mesh.frag が無いなら Sprite.frag のままでもOK
    d.fsPath     = base + "Mesh.frag.spv";
*/
    return d;
}

} // namespace VKPipelinePresets

} // namespace toy
