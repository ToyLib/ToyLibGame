//======================================================================
// Render/VK/Pipeline/VKPipelinePresets.cpp
//======================================================================
#include "Render/VK/Pipeline/VKPipelinePresets.h"

namespace toy
{

namespace VKPipelinePresets
{

//--------------------------------------------------------------
// Common set layouts / push constants
//--------------------------------------------------------------
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
        .type    = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, // ★DYNAMICやめる
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
// VKPipelinePresets.cpp（同ファイル内に追加）
static void AddPC_ShadowWorld(VKPipelineDesc& d)
{
    VKPushConstantDesc pc{};
    pc.stages = VK_SHADER_STAGE_VERTEX_BIT; // ★VSのみ
    pc.offset = 0;
    pc.size   = 64; // mat4
    d.pushConstants.push_back(pc);
}

//--------------------------------------------------------------
// Normal pipelines
//--------------------------------------------------------------
VKPipelineDesc MakeSprite(const std::string& base)
{
    VKPipelineDesc d{};
    d.vsPath     = base + "Sprite.vert.spv";
    d.fsPath     = base + "Sprite.frag.spv";

    // ★いったんこれに戻して Pipeline 作成が通るか確認
    d.layout     = VKPipelineDesc::VertexLayout::Mesh_Pos3Nrm3Uv2;

    d.depthTest  = false;
    d.depthWrite = false;
    d.alphaBlend = true;

    d.cullMode   = VK_CULL_MODE_NONE;
    d.frontFace  = VK_FRONT_FACE_COUNTER_CLOCKWISE;

    d.colorAttachmentCount = 1;

    AddSet0_SceneUBO(d);
    AddSet1_BaseMap(d);

    VKPushConstantDesc pc{};
    pc.stages = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pc.offset = 0;
    pc.size   = 80; // mat4 + vec4
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

    d.colorAttachmentCount = 1;

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

    d.colorAttachmentCount = 1;

    AddSet0_SceneUBO(d);
    AddSet1_BaseMap(d);
    AddSet2_SkinnedUBO(d);
    AddPC_ObjectMaterial(d);

    return d;
}

//--------------------------------------------------------------
// Shadow pipelines (depth-only)
//--------------------------------------------------------------
// SetupShadowCommon を修正（AddPC_ObjectMaterial をやめる）
static void SetupShadowCommon(VKPipelineDesc& d)
{
    d.colorAttachmentCount = 0;

    d.depthTest  = true;
    d.depthWrite = true;
    d.alphaBlend = false;

    d.cullMode   = VK_CULL_MODE_BACK_BIT;
    d.frontFace  = VK_FRONT_FACE_COUNTER_CLOCKWISE;

    d.depthBiasEnable         = true;
    d.depthBiasConstantFactor = 1.25f;
    d.depthBiasSlopeFactor    = 1.75f;
    d.depthBiasClamp          = 0.0f;

    AddSet0_SceneUBO(d);

    // ★重要：set2 を “set=2” として成立させるためのダミー set1
    // Shadow shader は set1 を参照しない前提なので bind しなくてOK。
    AddSet1_BaseMap(d);

    AddPC_ShadowWorld(d);
}

VKPipelineDesc MakeShadowMesh(const std::string& base)
{
    VKPipelineDesc d{};

    // ★この2つのspv名はプロジェクト側の命名に合わせて調整してOK
    //   - Vertex: world * lightVP を出して深度へ
    //   - Frag  : 空でもOKだが、現VKPipelineはfs必須なので最小fragを用意する
    d.vsPath = base + "Shadow_Mesh.vert.spv";
    d.fsPath = base + "Shadow_Mesh.frag.spv";

    d.layout = VKPipelineDesc::VertexLayout::Mesh_Pos3Nrm3Uv2;

    SetupShadowCommon(d);

    // Shadowは BaseMap 不要（set=1 なし）
    return d;
}

VKPipelineDesc MakeShadowSkinnedMesh(const std::string& base)
{
    VKPipelineDesc d{};

    d.vsPath = base + "Shadow_SkinnedMesh.vert.spv";
    d.fsPath = base + "Shadow_SkinnedMesh.frag.spv";

    d.layout = VKPipelineDesc::VertexLayout::Skinned_Pos3Nrm3Uv2_Bone4U32_Weight4;

    SetupShadowCommon(d);

    // Skinned は palette 用 set=2 が必要
    AddSet2_SkinnedUBO(d);

    return d;
}

} // namespace VKPipelinePresets

} // namespace toy
