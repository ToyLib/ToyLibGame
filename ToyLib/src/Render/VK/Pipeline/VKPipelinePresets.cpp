// Render/VK/Pipeline/VKPipelinePresets.cpp
#include "Render/VK/Pipeline/VKPipelinePresets.h"

namespace toy
{

namespace VKPipelinePresets
{

VKPipelineDesc MakeSprite(const std::string& base)
{
    VKPipelineDesc d{};
    d.vsPath     = base + "Sprite.vert.spv";
    d.fsPath     = base + "Sprite.frag.spv";
    d.layout     = VKPipelineDesc::VertexLayout::Sprite_Pos3Nrm3Uv2;

    //==========================================================
    // Sprite の “正” の基本状態（UI 前提）
    //==========================================================
    d.depthTest  = false;
    d.depthWrite = false;
    d.alphaBlend = true;

    // UI なのでカリング事故を避ける（まず確実に出す）
    d.cullMode   = VK_CULL_MODE_NONE;
    d.frontFace  = VK_FRONT_FACE_COUNTER_CLOCKWISE;

    //==========================================================
    // set=0 : Scene UBO（viewProj）
    //==========================================================
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

    //==========================================================
    // set=1 : baseMap sampler2D
    //==========================================================
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

    //==========================================================
    // PushConstants : mat4(64) + vec4(16) = 80
    //==========================================================
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
    // いまは Sprite をベースにする（差分は後で）
    VKPipelineDesc d = MakeSprite(base);

    // Mesh は通常 depth ON / alpha OFF / cull ON を想定するが、
    // ここは “Sprite を最低限出す” のが目的なので当面は触らない。
    // 必要になったら Mesh 専用 preset を作る。

    return d;
}

} // namespace VKPipelinePresets

} // namespace toy
