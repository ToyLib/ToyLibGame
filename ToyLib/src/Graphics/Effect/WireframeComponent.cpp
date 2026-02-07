// Graphics/Effect/WireframeComponent.cpp
#include "Graphics/Effect/WireframeComponent.h"

#include "Engine/Core/Actor.h"
#include "Engine/Core/Application.h"
#include "Render/IRenderer.h"
#include "Render/Shader.h"
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

    auto* owner = GetOwner();
    if (!owner)
    {
        return;
    }

    auto* app = owner->GetApp();
    if (!app)
    {
        return;
    }

    auto* renderer = app->GetRenderer();
    if (!renderer)
    {
        return;
    }

    // ----------------------------------------------------------
    // Debug payload（線の色/α）
    // ----------------------------------------------------------
    DebugPayload dp {};
    dp.color = mColor;
    dp.alpha = 1.0f;

    const uint32_t payloadIndex = q.PushDebugPayload(dp);

    // ----------------------------------------------------------
    // RenderItem
    // ----------------------------------------------------------
    RenderItem it {};
    it.pass      = RenderPass::World;
    it.layer     = GetLayer();
    it.drawOrder = GetDrawOrder();

    it.type     = RenderItemType::Debug;
    it.dispatch = GetDispatch(it.type);

    // geometry（lines + DrawArrays）
    it.geometry.ptr = mVertexArray.get();
    it.topology     = PrimitiveTopology::Lines;
    it.vertexCount  = static_cast<int>(mVertexArray->GetNumVerts());
    it.indexCount   = 0;

    // shader（ハンドルで統一）
    it.shader = renderer->GetShaderHandle("Solid");

    // transforms
    it.world    = owner->GetWorldTransform();
    it.viewProj = renderer->GetViewMatrix() * renderer->GetProjectionMatrix();

    // state（旧 Draw 寄せ）
    it.blend      = BlendMode::Alpha;   // 不要なら Opaque にしてOK
    it.depthTest  = true;
    it.depthWrite = true;
    it.cull       = CullMode::None;
    it.frontFace  = FrontFace::CCW;

    // payload
    it.payloadIndex = payloadIndex;

    q.Push(it);
}
} // namespace toy
