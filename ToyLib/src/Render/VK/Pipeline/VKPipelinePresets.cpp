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

// Sky 用 set=1 UBO
static void AddSet1_SkyUBO(VKPipelineDesc& d)
{
    VKDescriptorSetLayoutDesc set1{};
    set1.set = 1;
    set1.bindings.push_back({
        .binding = 0,
        .type    = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .count   = 1,
        .stages  = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT
    });
    d.setLayouts.push_back(set1);
}

// Overlay 用 set=1 UBO
static void AddSet1_OverlayUBO(VKPipelineDesc& d)
{
    VKDescriptorSetLayoutDesc set1{};
    set1.set = 1;
    set1.bindings.push_back({
        .binding = 0,
        .type    = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .count   = 1,
        .stages  = VK_SHADER_STAGE_FRAGMENT_BIT
    });
    d.setLayouts.push_back(set1);
}

static void AddSet1_Empty(VKPipelineDesc& d)
{
    VKDescriptorSetLayoutDesc set1{};
    set1.set = 1;
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

    set3.bindings.push_back({
        .binding = 0,
        .type    = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .count   = 1,
        .stages  = VK_SHADER_STAGE_FRAGMENT_BIT
    });

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
    pc.size   = 112;
    d.pushConstants.push_back(pc);
}

static void AddPC_WorldTintAlpha(VKPipelineDesc& d)
{
    VKPushConstantDesc pc{};
    pc.stages = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pc.offset = 0;
    pc.size   = 80;
    d.pushConstants.push_back(pc);
}

static void AddPC_DebugWorldColor(VKPipelineDesc& d)
{
    VKPushConstantDesc pc{};
    pc.stages = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pc.offset = 0;
    pc.size   = 96;
    d.pushConstants.push_back(pc);
}

// Sky は world だけ push constant
static void AddPC_SkyWorld(VKPipelineDesc& d)
{
    VKPushConstantDesc pc{};
    pc.stages = VK_SHADER_STAGE_VERTEX_BIT;
    pc.offset = 0;
    pc.size   = 64;
    d.pushConstants.push_back(pc);
}

static void AddPC_ShadowWorld(VKPipelineDesc& d)
{
    VKPushConstantDesc pc{};
    pc.stages = VK_SHADER_STAGE_VERTEX_BIT;
    pc.offset = 0;
    pc.size   = 64;
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
    d.blendMode  = VKPipelineDesc::BlendMode::Alpha;
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
    d.blendMode  = VKPipelineDesc::BlendMode::Alpha;
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
    d.blendMode  = VKPipelineDesc::BlendMode::Alpha;
    d.cullMode   = VK_CULL_MODE_NONE;
    d.frontFace  = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    d.colorAttachmentCount = 1;

    AddSet0_SceneUBO(d);
    AddPC_DebugWorldColor(d);
    return d;
}

VKPipelineDesc MakeSkyDome(const std::string& base)
{
    VKPipelineDesc d{};
    d.vsPath     = base + "WeatherDome.vert.spv";
    d.fsPath     = base + "WeatherDome.frag.spv";
    d.layout     = VKPipelineDesc::VertexLayout::Mesh_Pos3Nrm3Uv2;
    d.topology   = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    d.depthTest  = true;
    d.depthWrite = false;
    d.blendMode  = VKPipelineDesc::BlendMode::Opaque;
    d.cullMode   = VK_CULL_MODE_FRONT_BIT;
    d.frontFace  = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    d.colorAttachmentCount = 1;

    AddSet0_SceneUBO(d);
    AddSet1_SkyUBO(d);
    AddPC_SkyWorld(d);

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
    d.blendMode  = VKPipelineDesc::BlendMode::Opaque;
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
    d.blendMode  = VKPipelineDesc::BlendMode::Opaque;
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
// Overlay
//--------------------------------------------------------------
VKPipelineDesc MakeWeatherOverlay(const std::string& base)
{
    VKPipelineDesc d{};
    d.vsPath     = base + "WeatherScreen.vert.spv";
    d.fsPath     = base + "WeatherScreen.frag.spv";
    d.layout     = VKPipelineDesc::VertexLayout::Vec2_Pos2;
    d.topology   = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    d.depthTest  = false;
    d.depthWrite = false;
    d.blendMode  = VKPipelineDesc::BlendMode::Alpha;
    d.cullMode   = VK_CULL_MODE_NONE;
    d.frontFace  = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    d.colorAttachmentCount = 1;

    // set0 は不要、set1 に OverlayUBO を置く
    AddSet1_OverlayUBO(d);

    return d;
}

VKPipelineDesc MakeWeatherOverlayAdd(const std::string& base)
{
    VKPipelineDesc d{};
    d.vsPath     = base + "WeatherScreen.vert.spv";
    d.fsPath     = base + "WeatherScreen.frag.spv";
    d.layout     = VKPipelineDesc::VertexLayout::Vec2_Pos2;
    d.topology   = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    d.depthTest  = false;
    d.depthWrite = false;
    d.blendMode  = VKPipelineDesc::BlendMode::Additive;
    d.cullMode   = VK_CULL_MODE_NONE;
    d.frontFace  = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    d.colorAttachmentCount = 1;

    AddSet1_OverlayUBO(d);

    return d;
}

//--------------------------------------------------------------
// Shadow pipelines
//--------------------------------------------------------------
static void SetupShadowCommon_Mesh(VKPipelineDesc& d)
{
    d.topology             = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    d.colorAttachmentCount = 0;
    d.depthTest  = true;
    d.depthWrite = true;
    d.blendMode  = VKPipelineDesc::BlendMode::Opaque;
    d.cullMode   = VK_CULL_MODE_BACK_BIT;
    d.frontFace  = VK_FRONT_FACE_COUNTER_CLOCKWISE;

    d.depthBiasEnable         = true;
    d.depthBiasConstantFactor = 1.25f;
    d.depthBiasSlopeFactor    = 1.75f;
    d.depthBiasClamp          = 0.0f;

    AddSet0_SceneUBO(d);
    AddPC_ShadowWorld(d);
}

static void SetupShadowCommon_Skinned(VKPipelineDesc& d)
{
    d.topology             = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    d.colorAttachmentCount = 0;
    d.depthTest  = true;
    d.depthWrite = true;
    d.blendMode  = VKPipelineDesc::BlendMode::Opaque;
    d.cullMode   = VK_CULL_MODE_BACK_BIT;
    d.frontFace  = VK_FRONT_FACE_COUNTER_CLOCKWISE;

    d.depthBiasEnable         = true;
    d.depthBiasConstantFactor = 1.25f;
    d.depthBiasSlopeFactor    = 1.75f;
    d.depthBiasClamp          = 0.0f;

    AddSet0_SceneUBO(d);
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

static void AddPC_FadeColorAlpha(VKPipelineDesc& d)
{
    VKPushConstantDesc pc{};
    pc.stages = VK_SHADER_STAGE_FRAGMENT_BIT;
    pc.offset = 0;
    pc.size   = 16; // vec4
    d.pushConstants.push_back(pc);
}

VKPipelineDesc MakeFade(const std::string& base)
{
    VKPipelineDesc d{};
    d.vsPath     = base + "Fade.vert.spv";
    d.fsPath     = base + "Fade.frag.spv";
    d.layout     = VKPipelineDesc::VertexLayout::Vec2_Pos2;
    d.topology   = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    d.depthTest  = false;
    d.depthWrite = false;
    d.blendMode  = VKPipelineDesc::BlendMode::Alpha;


    d.cullMode   = VK_CULL_MODE_NONE;
    d.frontFace  = VK_FRONT_FACE_COUNTER_CLOCKWISE;

    d.colorAttachmentCount = 1;

    AddPC_FadeColorAlpha(d);

    return d;
}

static void AddPC_Surface(VKPipelineDesc& d)
{
    VKPushConstantDesc pc{};
    pc.stages = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pc.offset = 0;
    pc.size   = 128; // mat4 + vec4 * 4
    d.pushConstants.push_back(pc);
}

VKPipelineDesc MakeRenderSurface(const std::string& base)
{
    VKPipelineDesc d{};
    d.vsPath     = base + "RenderSurface.vert.spv";
    d.fsPath     = base + "RenderSurface.frag.spv";
    d.layout     = VKPipelineDesc::VertexLayout::Mesh_Pos3Nrm3Uv2;
    d.topology   = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    d.depthTest  = true;
    d.depthWrite = true;
    d.blendMode  = VKPipelineDesc::BlendMode::Alpha;
    d.cullMode   = VK_CULL_MODE_BACK_BIT;
    d.frontFace  = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    d.colorAttachmentCount = 1;

    AddSet0_SceneUBO(d);
    AddSet1_BaseMap(d);
    AddPC_Surface(d);

    return d;
}

static void AddSet0_PostEffectTextures(VKPipelineDesc& d)
{
    VKDescriptorSetLayoutDesc set0{};
    set0.set = 0;

    set0.bindings.push_back({
        .binding = 0,
        .type    = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .count   = 1,
        .stages  = VK_SHADER_STAGE_FRAGMENT_BIT
    });

    set0.bindings.push_back({
        .binding = 1,
        .type    = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .count   = 1,
        .stages  = VK_SHADER_STAGE_FRAGMENT_BIT
    });

    d.setLayouts.push_back(set0);
}
static void AddPC_PostEffect(VKPipelineDesc& d)
{
    VKPushConstantDesc pc{};
    pc.stages = VK_SHADER_STAGE_FRAGMENT_BIT;
    pc.offset = 0;
    pc.size   = 32;
    d.pushConstants.push_back(pc);
}
/*
VKPipelineDesc MakePostEffect(const std::string& base)
{
    VKPipelineDesc d{};
    d.vsPath     = base + "PostEffect.vert.spv";
    d.fsPath     = base + "PostEffect.frag.spv";
    d.layout     = VKPipelineDesc::VertexLayout::Vec2_Pos2;
    d.topology   = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    d.depthTest  = false;
    d.depthWrite = false;
    d.blendMode  = VKPipelineDesc::BlendMode::Opaque;
    d.cullMode   = VK_CULL_MODE_NONE;
    d.frontFace  = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    d.colorAttachmentCount = 1;

    AddSet0_PostEffectTextures(d);
    AddPC_PostEffect(d);

    return d;
}
*/
VKPipelineDesc MakePostEffect(const std::string& base)
{
    VKPipelineDesc d{};
    d.vsPath     = base + "PostEffect.vert.spv";
    d.fsPath     = base + "PostEffect.frag.spv";
    d.layout     = VKPipelineDesc::VertexLayout::Vec2_Pos2;
    d.topology   = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    d.depthTest  = false;
    d.depthWrite = false;
    d.blendMode  = VKPipelineDesc::BlendMode::Opaque;
    d.cullMode   = VK_CULL_MODE_NONE;
    d.frontFace  = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    d.colorAttachmentCount = 1;

    AddSet0_PostEffectTextures(d);
    AddPC_PostEffect(d);

    return d;
}
} // namespace VKPipelinePresets

} // namespace toy
