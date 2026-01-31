#include "Graphics/Sprite/ShadowSpriteComponent.h"

#include "Engine/Core/Actor.h"
#include "Engine/Core/Application.h"
#include "Engine/Render/Renderer.h"
#include "Engine/Render/LightingManager.h"
#include "Engine/Render/RenderQueue.h"
#include "Engine/Render/RenderItem.h"

#include "Asset/Material/Texture.h"
#include "Asset/Geometry/VertexArray.h"

#include <cmath>

namespace toy {

ShadowSpriteComponent::ShadowSpriteComponent(Actor* owner, int drawOrder)
    : GroundConformSpriteComponent(owner, drawOrder, VisualLayer::Effect3D)
{
    // 旧実装互換：Sprite シェーダ（uSpriteColor/uSpriteAlpha）
    mShader = GetOwner()->GetApp()->GetRenderer()->GetShader("Sprite");

    // 影用のテクスチャを自前生成（現状再現）
    auto tex = std::make_shared<Texture>();
    tex->CreateAlphaCircle(256, 0.5f, 0.3f, Vector3(0.0f, 0.0f, 0.0f), 0.8f);
    SetTexture(tex);

    // 新設計は “ワールド単位” サイズで指定する（仮値）
    SetSize(100.0f, 100.0f);

    // 影は通常アルファ
    SetBlendAdd(false);

    // Sprite.frag 用：Tint/Alpha（影は白でOK）
    SetTint(Vector3(1.0f, 1.0f, 1.0f));
    SetAlpha(1.0f);

    // 影なので地面に貼り付け前提
    SetSnapToGround(true);
    SetAlignToGround(false);         // 影は水平が自然
    SetUseSmoothGroundPose(false);   // raw
}

float ShadowSpriteComponent::ComputeLightYawRad() const
{
    float yaw = mYaw;

    if (!mAutoRotateByLight)
        return yaw;

    auto* renderer = GetOwner()->GetApp()->GetRenderer();
    auto lm = renderer ? renderer->GetLightingManager() : nullptr;

    Vector3 lightDir = lm ? lm->GetLightDirection() : Vector3(0.0f, 0.0f, 1.0f);

    // XZ 平面だけ使う
    lightDir.y = 0.0f;

    if (lightDir.LengthSq() < 0.0001f)
    {
        lightDir = Vector3(0.0f, 0.0f, 1.0f);
    }
    lightDir.Normalize();

    yaw = atan2f(lightDir.x, lightDir.z);
    return yaw;
}

Matrix4 ShadowSpriteComponent::BuildWorldMatrix() const
{
    // GroundConform は頂点がワールド座標なので Identity
    return Matrix4::Identity;
}

void ShadowSpriteComponent::PreDraw()
{
    // Shadow は “ライト方向Yaw + Stretch” を
    // GroundConform のグリッド生成側へ反映したいので、
    // 一時的に mYaw / mDepth を調整して Rebuild させる。

    const float savedYaw   = mYaw;
    const float savedDepth = mDepth;

    // 1) yaw をライト方向に合わせる
    mYaw = ComputeLightYawRad();

    // 2) 奥行を伸ばす（GroundConform の halfD が mDepth を使うのでここで反映）
    mDepth = savedDepth * mStretch;

    // GroundConform の再構築
    RebuildGridIfNeeded();

    // 値は戻す（外から見たパラメータを汚さない）
    mYaw   = savedYaw;
    mDepth = savedDepth;
}

//------------------------------------------------------------------------------
// 新パス：RenderQueue に積む（Sprite扱いで Sprite uniform を使う）
//------------------------------------------------------------------------------
void ShadowSpriteComponent::GatherRenderItems(RenderQueue& queue)
{
    if (!mIsVisible || !mTexture)
        return;

    auto* renderer = GetOwner()->GetApp()->GetRenderer();
    if (!renderer || !mShader)
        return;

    // ここで必要なら再構築（ライトYaw/Stretch反映込み）
    PreDraw();

    if (!mGridVAO)
        return;

    RenderItem it;
    it.pass      = RenderPass::World;
    it.layer     = mLayer;          // Effect3D
    it.drawOrder = mDrawOrder;

    // ★Sprite扱い（Spriteシェーダ互換：uSpriteColor/uSpriteAlpha）
    it.type      = RenderItemType::Sprite;
    it.dispatch  = GetDispatch(it.type);

    it.topology    = PrimitiveTopology::Triangles;
    it.geometry.ptr = mGridVAO.get();
    it.indexCount  = static_cast<int>(mGridVAO->GetNumIndices());

    it.shader = renderer->GetShaderHandle("Sprite");

    // transforms：頂点がワールドなので Identity
    const Matrix4 view = renderer->GetViewMatrix();
    const Matrix4 proj = renderer->GetProjectionMatrix();
    it.viewProj = view * proj;
    it.world    = Matrix4::Identity;

    // state：影
    it.blend      = BlendMode::Alpha;   // 通常アルファ
    it.depthTest  = true;
    it.depthWrite = false;              // 影は depth write しない（重なりで破綻しにくい）
    it.cull       = CullMode::None;
    it.frontFace  = FrontFace::CCW;

    // sprite uniforms
    it.color = mTint;
    it.alpha = mAlpha;

    it.texture     = renderer->ToHandle(mTexture);
    it.textureUnit = 0;

    queue.Push(it);
}

} // namespace toy
