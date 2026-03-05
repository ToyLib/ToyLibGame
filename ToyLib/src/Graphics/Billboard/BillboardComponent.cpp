#include "Graphics/Billboard/BillboardComponent.h"

#include "Asset/Material/Texture.h"
#include "Engine/Core/Actor.h"
#include "Engine/Core/Application.h"
#include "Render/IRenderer.h"

#include <cmath>

namespace toy {

BillboardComponent::BillboardComponent(Actor* a, int drawOrder, VisualLayer layer)
    : VisualComponent(a, drawOrder, layer)
{
    // Shader / Geometry は Renderer 側で選ぶのでここでは持たない（新パス）
    mPipelineName = "UnlitQuad";
}

//----------------------------------------------------------------------
// GatherRenderItems
//  - Billboard を RenderQueue に積む（新パス）
//----------------------------------------------------------------------
void BillboardComponent::GatherRenderItems(RenderQueue& out)
{
    if (!mIsVisible || !mTexture)
    {
        return;
    }

    auto* owner    = GetOwner();
    auto* renderer = owner ? owner->GetApp()->GetRenderer() : nullptr;
    if (!renderer)
    {
        return;
    }

    //--------------------------------------------------------------------------
    // Camera matrices
    //--------------------------------------------------------------------------

    const Matrix4 view = renderer->GetViewMatrix();
    const Matrix4 proj = renderer->GetProjectionMatrix();

    //--------------------------------------------------------------------------
    // World position / camera position
    //--------------------------------------------------------------------------

    const Matrix4 actorWorld = owner->GetWorldTransform();
    const Vector3 pos        = actorWorld.GetTranslation();
    const Vector3 cameraPos  = renderer->GetInvViewMatrix().GetTranslation();

    //--------------------------------------------------------------------------
    // Horizontal billboard rotation (Y-axis)
    //--------------------------------------------------------------------------

    Vector3 toCamera = pos - cameraPos;
    toCamera.y = 0.0f;

    if (toCamera.LengthSq() < 1.0e-6f)
    {
        toCamera = Vector3::UnitZ;
    }
    else
    {
        toCamera.Normalize();
    }

    const float   angle = std::atan2(toCamera.x, toCamera.z);
    const Matrix4 rotY  = Matrix4::CreateRotationY(angle);

    //--------------------------------------------------------------------------
    // Scale (mScale * ownerScale) * texture size
    //--------------------------------------------------------------------------

    const float scale = mScale * owner->GetScale();

    const Matrix4 scaleMat = Matrix4::CreateScale(
        mTexture->GetWidth()  * scale,
        mTexture->GetHeight() * scale,
        1.0f);

    const Matrix4 translate = Matrix4::CreateTranslation(pos);

    // Keep original multiplication order
    const Matrix4 world = scaleMat * rotY * translate;

    // Keep original side effect (owner rotation update)
    {
        Quaternion q(Vector3::UnitY, angle);
        owner->SetRotation(q);
    }

    //--------------------------------------------------------------------------
    // Payload (Billboard)
    //  - Mesh shader前提なので payload は「必要になったら増やす」でOK
    //--------------------------------------------------------------------------

    UnlitQuadPayload bp{};
    // bp.color / bp.alpha を使う設計にするならここで詰める
    // bp.color = ...
    // bp.alpha = ...

    const uint32_t payloadIndex = out.PushUnlitQuad(bp);

    //--------------------------------------------------------------------------
    // RenderItem
    //--------------------------------------------------------------------------

    RenderItem it{};
    it.type      = RenderItemType::UnlitQuad;
    it.pass      = RenderPass::World;
    it.dispatch  = GetDispatch(it.type);
    it.layer     = mLayer;
    it.drawOrder = mDrawOrder; // あれば。無いならこの行は削除

    it.pipeline = renderer->GetPipelineHandle(mPipelineName); // Phong前提
    it.viewProj = view * proj;
    it.world    = world;

    // Common quad
    it.geometry   = renderer->GetSurfaceQuadHandle();
    it.indexCount = 6;

    // Texture (no Material)
    it.texture     = renderer->ToHandle(mTexture);
    it.textureUnit = 0;

    // Payload link
    it.payloadIndex = payloadIndex;

    // Render state
    it.depthTest  = true;
    it.depthWrite = false;
    it.blend      = mIsBlendAdd ? BlendMode::Additive : BlendMode::Alpha;

    // Visibility priority for a single quad
    it.cull      = CullMode::Back;
    it.frontFace = FrontFace::CCW;

    out.Push(it);
}
} // namespace toy
