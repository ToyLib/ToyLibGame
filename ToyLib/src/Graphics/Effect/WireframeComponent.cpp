// Graphics/Effect/WireframeComponent.cpp
#include "Graphics/Effect/WireframeComponent.h"

#include "Engine/Core/Actor.h"
#include "Engine/Core/Application.h"
#include "Engine/Render/Renderer.h"
#include "Engine/Render/Shader.h"
#include "Asset/Geometry/VertexArray.h"

namespace toy {

//------------------------------------------------------------
// ctor
//------------------------------------------------------------
WireframeComponent::WireframeComponent(Actor* owner,
                                       int drawOrder,
                                       VisualLayer layer)
    : VisualComponent(owner, drawOrder, layer)
{
    // 単色描画（デバッグ用）
    mShader = GetOwner()->GetApp()->GetRenderer()->GetShader("Solid");
}

//------------------------------------------------------------
// RenderQueue 変換
//------------------------------------------------------------
void WireframeComponent::GatherRenderItems(RenderQueue& q)
{
    if (!mIsVisible)
    {
        return;
    }
    if (!mVertexArray)
    {
        return;
    }
    
    auto* app = GetOwner()->GetApp();
    if (!app)
    {
        return;
    }
    
    auto* renderer = app->GetRenderer();
    if (!renderer)
    {
        return;
    }
    
    RenderItem it{};
    it.pass      = RenderPass::World;
    it.layer     = GetLayer();
    it.drawOrder = GetDrawOrder();

    // 新パス上の種別：Debug（ワイヤーフレーム扱い）
    it.type = RenderItemType::Debug;
    it.dispatch = GetDispatch(it.type);
    
    // geometry
    it.geometry.ptr  = mVertexArray.get();
    it.topology      = PrimitiveTopology::Lines;
    it.vertexCount   = mVertexArray->GetNumVerts(); // ★DrawArrays 用
    it.indexCount    = 0;                           // ★DrawElements しない

    // shader
    it.shader.ptr = renderer->GetShader("Solid").get();

    // transforms
    it.world   = GetOwner()->GetWorldTransform();
    it.viewProj = renderer->GetViewMatrix() * renderer->GetProjectionMatrix();

    // color/alpha
    it.color = mColor;
    it.alpha = 1.0f;

    // state（旧 Draw() に寄せる）
    it.blend      = BlendMode::Alpha;  // 旧はブレンド有効が多い（不要なら Opaque に）
    it.depthTest  = true;
    it.depthWrite = true;              // ★旧は glDepthMask 触ってないので “書く” 寄り
    it.cull       = CullMode::None;    // 線はカリングしない方が安全
    it.frontFace  = FrontFace::CCW;

    // toon は無関係
    it.toon = false;

    q.Push(it);
}
} // namespace toy
