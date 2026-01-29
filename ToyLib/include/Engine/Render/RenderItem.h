// Engine/Render/RenderItem.h
#pragma once

#include "Utils/MathUtil.h"
#include "Engine/Render/RenderEnums.h"
#include "Engine/Render/RenderHandles.h"
#include "Engine/Render/VisualLayer.h"

namespace toy {

// ★まずは Sprite が出せる最小版
struct RenderItem
{
    // ソートキー
    RenderPass  pass  = RenderPass::Main;
    VisualLayer layer = VisualLayer::Object3D;
    int         drawOrder = 0;

    // 形状
    PrimitiveTopology topology = PrimitiveTopology::Triangles;
    GeometryHandle    geometry {};       // SpriteならRendererの共通Quad VAO
    int               indexCount = 0;    // Spriteなら6

    // 状態
    BlendMode blend = BlendMode::Alpha;
    bool      depthTest  = false;        // Spriteはfalse
    bool      depthWrite = false;        // Spriteはfalse

    // シェーダ（暫定：Rendererがpassで選ぶなら不要。今は入れてもOK）
    ShaderHandle shader {};

    // 行列
    Matrix4 world   { Matrix4::Identity };
    Matrix4 viewProj{ Matrix4::Identity };

    // テクスチャ（Spriteは1枚）
    TextureHandle texture {};
    int textureUnit = 0;

    // “Sprite用 uniform”
    Vector3 color { 1.0f, 1.0f, 1.0f };
    float   alpha { 1.0f };
};

} // namespace toy
