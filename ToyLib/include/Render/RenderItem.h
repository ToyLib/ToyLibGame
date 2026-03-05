// Render/RenderItem.h
#pragma once

#include "Render/RenderEnums.h"
#include "Render/RenderHandles.h"
#include "Render/VisualLayer.h"
#include "Utils/MathUtil.h"
#include <cstddef>
#include <cstdint>

namespace toy {

enum class RenderItemType
{
    Sprite,
    Mesh,
    SkinnedMesh,
    UnlitQuad,
    Particle,
    SkyDome,
    Overlay,
    Debug,
    Surface
};

struct RenderItem
{
    //========================
    // Sort key / pass
    //========================
    RenderPass  pass      { RenderPass::World };
    VisualLayer layer     { VisualLayer::Object3D };
    int         drawOrder { 0 };

    using DispatchFn = bool(*)(
        class IRenderer&  r,
        const RenderItem& it,
        RenderPass        pass,
        int               cascadeIndex);

    DispatchFn     dispatch { nullptr };
    RenderItemType type     { RenderItemType::Sprite };

    static constexpr uint32_t kInvalidPayload = 0xFFFFFFFFu;
    uint32_t payloadIndex { kInvalidPayload }; // type で解釈する

    //========================
    // Geometry
    //========================
    PrimitiveTopology topology    { PrimitiveTopology::Triangles };
    GeometryHandle    geometry    {};
    int               indexCount  { 0 };
    int               vertexCount { 0 };

    //========================
    // Render state
    //========================
    BlendMode  blend      { BlendMode::Opaque };
    bool       depthTest  { true };
    bool       depthWrite { true };
    CullMode   cull       { CullMode::Back };
    FrontFace  frontFace  { FrontFace::CCW };

    //========================
    // Shader / material / texture
    //========================
    PipelineHandle pipeline    {};
    MaterialHandle material    {};
    TextureHandle  texture     {};      // Sprite/Billboard/Particle/Surface などで使う
    int            textureUnit { 0 };

    //========================
    // Transforms (共通)
    //========================
    Matrix4 world    { Matrix4::Identity };
    Matrix4 viewProj { Matrix4::Identity };

    //========================
    // SkinnedMesh-only (描画形態)
    //========================
    const Matrix4* matrixPalette { nullptr };
    size_t         paletteCount  { 0 };

    //========================
    // GPUParticle-only (描画形態)
    //========================
    uint32_t gpuVAO        { 0 };
    int      instanceCount { 0 };
};

RenderItem::DispatchFn GetDispatch(RenderItemType type);

} // namespace toy
