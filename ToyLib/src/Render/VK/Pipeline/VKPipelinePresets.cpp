// Render/VK/Pipeline/VKPipelinePresets.cpp
#include "Render/VK/Pipeline/VKPipelinePresets.h"

namespace toy
{

namespace VKPipelinePresets
{

static void AddSet0_SceneUBO(VKPipelineDesc& d)
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

static void AddSet1_BaseMap(VKPipelineDesc& d)
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

static void AddSet2_SkinnedUBO(VKPipelineDesc& d)
{
    VKDescriptorSetLayoutDesc set2{};
    set2.set = 2;
    set2.bindings.push_back({
        .binding = 0,
        .type    = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .count   = 1,
        .stages  = VK_SHADER_STAGE_VERTEX_BIT
    });
    d.setLayouts.push_back(set2);
}

static void AddPC_ObjectMaterial(VKPipelineDesc& d)
{
    VKPushConstantDesc pc{};
    pc.stages = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pc.offset = 0;
    pc.size   = 112;
    d.pushConstants.push_back(pc);
}

VKPipelineDesc MakeSprite(const std::string& base)
{
    VKPipelineDesc d{};
    d.vsPath     = base + "Sprite.vert.spv";
    d.fsPath     = base + "Sprite.frag.spv";

    // ★ここ修正（重要）
    d.layout     = VKPipelineDesc::VertexLayout::Mesh_Pos3Nrm3Uv2;

    d.depthTest  = false;
    d.depthWrite = false;
    d.alphaBlend = true;

    d.cullMode   = VK_CULL_MODE_NONE;
    d.frontFace  = VK_FRONT_FACE_CLOCKWISE;

    AddSet0_SceneUBO(d);
    AddSet1_BaseMap(d);

    VKPushConstantDesc pc{};
    pc.stages = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pc.offset = 0;
    pc.size   = 80;
    d.pushConstants.push_back(pc);

    return d;
}

VKPipelineDesc MakeMesh(const std::string& base)
{
    VKPipelineDesc d{};
    d.vsPath     = base + "Mesh.vert.spv";
    d.fsPath     = base + "MeshPhong.frag.spv";
    d.layout     = VKPipelineDesc::VertexLayout::Mesh_Pos3Nrm3Uv2;

    d.depthTest  = true;
    d.depthWrite = true;
    d.alphaBlend = false;

    d.cullMode   = VK_CULL_MODE_BACK_BIT;

    // ★viewport反転に合わせて CCW
    d.frontFace  = VK_FRONT_FACE_COUNTER_CLOCKWISE;

    AddSet0_SceneUBO(d);
    AddSet1_BaseMap(d);
    AddPC_ObjectMaterial(d);

    return d;
}

VKPipelineDesc MakeSkinnedMesh(const std::string& base)
{
    VKPipelineDesc d{};
    d.vsPath     = base + "SkinnedMesh.vert.spv";
    d.fsPath     = base + "MeshPhong.frag.spv";
    d.layout     = VKPipelineDesc::VertexLayout::Skinned_Pos3Nrm3Uv2_Bone4U32_Weight4;

    d.depthTest  = true;
    d.depthWrite = true;
    d.alphaBlend = false;

    d.cullMode   = VK_CULL_MODE_BACK_BIT;

    // ★viewport反転に合わせて CCW
    d.frontFace  = VK_FRONT_FACE_COUNTER_CLOCKWISE;

    AddSet0_SceneUBO(d);
    AddSet1_BaseMap(d);
    AddSet2_SkinnedUBO(d);
    AddPC_ObjectMaterial(d);

    return d;
}

} // namespace VKPipelinePresets

} // namespace toy
