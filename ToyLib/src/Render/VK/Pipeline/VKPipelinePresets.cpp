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

// ★追加：ShadowSkinned の “穴埋め” 用（binding 0件）
static void AddSet1_Empty(VKPipelineDesc& d)
{
    VKDescriptorSetLayoutDesc set1{};
    set1.set = 1;
    // bindings は空のまま（shader側で set=1 を使わない）
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

static void AddSet3_ShadowSample(VKPipelineDesc& d)
{
    VKDescriptorSetLayoutDesc set3{};
    set3.set = 3;

    // binding=0 : shadowMap0
    set3.bindings.push_back({
        .binding = 0,
        .type    = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .count   = 1,
        .stages  = VK_SHADER_STAGE_FRAGMENT_BIT
    });

    // binding=1 : shadowMap1
    set3.bindings.push_back({
        .binding = 1,
        .type    = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .count   = 1,
        .stages  = VK_SHADER_STAGE_FRAGMENT_BIT
    });

    d.setLayouts.push_back(set3);
}

static void AddPC_ObjectMaterial(VKPipelineDesc& d)
{
    VKPushConstantDesc pc{};
    pc.stages = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pc.offset = 0;
    pc.size   = 112; // mat4 + vec4 + vec4 + vec4
    d.pushConstants.push_back(pc);
}

static void AddPC_WorldTintAlpha(VKPipelineDesc& d)
{
    VKPushConstantDesc pc{};
    pc.stages = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pc.offset = 0;
    pc.size   = 80; // mat4(64) + vec4(16)
    d.pushConstants.push_back(pc);
}

static void AddPC_DebugWorldColor(VKPipelineDesc& d)
{
    VKPushConstantDesc pc{};
    pc.stages = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pc.offset = 0;
    pc.size   = 96; // mat4(64) + vec4(16) + vec4(16)
    d.pushConstants.push_back(pc);
}

static void AddPC_ShadowWorld(VKPipelineDesc& d)
{
    VKPushConstantDesc pc{};
    pc.stages = VK_SHADER_STAGE_VERTEX_BIT;
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
    d.layout     = VKPipelineDesc::VertexLayout::Mesh_Pos3Nrm3Uv2;
    d.topology   = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    d.depthTest  = false;
    d.depthWrite = false;
    d.alphaBlend = true;

    d.cullMode   = VK_CULL_MODE_NONE;
    d.frontFace  = VK_FRONT_FACE_COUNTER_CLOCKWISE;

    d.colorAttachmentCount = 1;

    AddSet0_SceneUBO(d);
    AddSet1_BaseMap(d);
    AddPC_WorldTintAlpha(d);

    return d;
}

VKPipelineDesc MakeUnlitQuad(const std::string& base)
{
    VKPipelineDesc d{};
    d.vsPath     = base + "UnlitQuad.vert.spv";
    d.fsPath     = base + "UnlitQuad.frag.spv";
    d.layout     = VKPipelineDesc::VertexLayout::Mesh_Pos3Nrm3Uv2;
    d.topology   = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    d.depthTest  = true;
    d.depthWrite = false;
    d.alphaBlend = true;

    // まずは安全側。FootSprite / Billboard / GroundConform を1本で通す
    d.cullMode   = VK_CULL_MODE_NONE;
    d.frontFace  = VK_FRONT_FACE_COUNTER_CLOCKWISE;

    d.colorAttachmentCount = 1;

    AddSet0_SceneUBO(d);
    AddSet1_BaseMap(d);
    AddPC_WorldTintAlpha(d);

    return d;
}

VKPipelineDesc MakeUnlitWire(const std::string& base)
{
    VKPipelineDesc d{};
    d.vsPath     = base + "UnlitWire.vert.spv";
    d.fsPath     = base + "UnlitWire.frag.spv";
    d.layout     = VKPipelineDesc::VertexLayout::Mesh_Pos3Nrm3Uv2;
    d.topology   = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;

    d.depthTest  = true;
    d.depthWrite = true;
    d.alphaBlend = true;

    d.cullMode   = VK_CULL_MODE_NONE;
    d.frontFace  = VK_FRONT_FACE_COUNTER_CLOCKWISE;

    d.colorAttachmentCount = 1;

    AddSet0_SceneUBO(d);
    AddPC_DebugWorldColor(d);

    return d;
}

VKPipelineDesc MakeMesh(const std::string& base)
{
    VKPipelineDesc d{};
    d.vsPath     = base + "Mesh.vert.spv";
    d.fsPath     = base + "MeshPhong.frag.spv";
    d.layout     = VKPipelineDesc::VertexLayout::Mesh_Pos3Nrm3Uv2;
    d.topology   = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    d.depthTest  = true;
    d.depthWrite = true;
    d.alphaBlend = false;

    d.cullMode   = VK_CULL_MODE_BACK_BIT;
    d.frontFace  = VK_FRONT_FACE_COUNTER_CLOCKWISE;

    d.colorAttachmentCount = 1;

    AddSet0_SceneUBO(d);
    AddSet1_BaseMap(d);
    AddSet3_ShadowSample(d);
    AddPC_ObjectMaterial(d);

    return d;
}

VKPipelineDesc MakeSkinnedMesh(const std::string& base)
{
    VKPipelineDesc d{};
    d.vsPath     = base + "SkinnedMesh.vert.spv";
    d.fsPath     = base + "MeshPhong.frag.spv";
    d.layout     = VKPipelineDesc::VertexLayout::Skinned_Pos3Nrm3Uv2_Bone4U32_Weight4;
    d.topology   = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    d.depthTest  = true;
    d.depthWrite = true;
    d.alphaBlend = false;

    d.cullMode   = VK_CULL_MODE_BACK_BIT;
    d.frontFace  = VK_FRONT_FACE_COUNTER_CLOCKWISE;

    d.colorAttachmentCount = 1;

    AddSet0_SceneUBO(d);
    AddSet1_BaseMap(d);
    AddSet2_SkinnedUBO(d);
    AddSet3_ShadowSample(d);
    AddPC_ObjectMaterial(d);

    return d;
}

//--------------------------------------------------------------
// Shadow pipelines (depth-only)
//--------------------------------------------------------------
static void SetupShadowCommon_Mesh(VKPipelineDesc& d)
{
    d.topology             = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
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
    AddPC_ShadowWorld(d);
}

// ShadowSkinned 用（set=0 + set=2、ただし set=1 を空で“穴埋め”）
static void SetupShadowCommon_Skinned(VKPipelineDesc& d)
{
    d.topology             = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
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

    // ★重要：set=2 を使う pipeline は “set=1 を空で挟む”
    AddSet1_Empty(d);

    AddSet2_SkinnedUBO(d);
    AddPC_ShadowWorld(d);
}

VKPipelineDesc MakeShadowMesh(const std::string& base)
{
    VKPipelineDesc d{};

    d.vsPath = base + "Shadow_Mesh.vert.spv";
    d.fsPath = base + "Shadow_Mesh.frag.spv";
    d.layout = VKPipelineDesc::VertexLayout::Mesh_Pos3Nrm3Uv2;

    SetupShadowCommon_Mesh(d);

    return d;
}

VKPipelineDesc MakeShadowSkinnedMesh(const std::string& base)
{
    VKPipelineDesc d{};

    d.vsPath = base + "Shadow_SkinnedMesh.vert.spv";
    d.fsPath = base + "Shadow_SkinnedMesh.frag.spv";
    d.layout = VKPipelineDesc::VertexLayout::Skinned_Pos3Nrm3Uv2_Bone4U32_Weight4;

    SetupShadowCommon_Skinned(d);

    return d;
}

} // namespace VKPipelinePresets

} // namespace toy
