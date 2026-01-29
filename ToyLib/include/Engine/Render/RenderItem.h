// Engine/Render/RenderItem.h
#pragma once

#include "Utils/MathUtil.h"
#include "Engine/Render/RenderEnums.h"
#include "Engine/Render/RenderHandles.h"
#include "Engine/Render/VisualLayer.h"

namespace toy {

enum class RenderItemType {
    Sprite,
    Mesh,
    SkinnedMesh,
    Debug
};

struct RenderItem
{
    // sort key
    RenderPass  pass      = RenderPass::Main;
    VisualLayer layer     = VisualLayer::Object3D;
    int         drawOrder = 0;

    RenderItemType type   = RenderItemType::Sprite; // ★追加

    // geometry
    PrimitiveTopology topology  = PrimitiveTopology::Triangles;
    GeometryHandle    geometry  {};
    int               indexCount = 0;

    // render state
    BlendMode  blend      = BlendMode::Opaque;
    bool       depthTest  = true;
    bool       depthWrite = true;

    CullMode   cull       = CullMode::Back;          // ★追加
    FrontFace  frontFace  = FrontFace::CCW;          // ★追加

    // shader (暫定：GL Shader)
    ShaderHandle shader {};

    // transforms
    Matrix4 world   { Matrix4::Identity };
    Matrix4 viewProj{ Matrix4::Identity };

    // texture / material
    TextureHandle  texture {};
    int            textureUnit = 0;

    MaterialHandle material {};                      // ★追加（Meshで使う）

    // sprite-only uniforms
    Vector3 color { 1.0f, 1.0f, 1.0f };
    float   alpha { 1.0f };
    
    // ★Skinned 用（nullptrなら非スキン）
    const Matrix4* matrixPalette = nullptr;
    int            paletteCount  = 0;


    
};

} // namespace toy
