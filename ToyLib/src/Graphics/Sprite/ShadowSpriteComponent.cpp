#include "Graphics/Sprite/ShadowSpriteComponent.h"

#include "Engine/Core/Actor.h"
#include "Engine/Core/Application.h"
#include "Engine/Render/Renderer.h"
#include "Engine/Render/LightingManager.h"
#include "Engine/Render/Shader.h"
#include "Asset/Geometry/VertexArray.h"
#include "Asset/Material/Texture.h"

#include "glad/glad.h"

namespace toy {

ShadowSpriteComponent::ShadowSpriteComponent(Actor* owner, int drawOrder)
    : FootSpriteComponent(owner, drawOrder, VisualLayer::Effect3D)
{
    // 旧実装互換：Sprite シェーダ（完全Unlit＆Tint/Alpha対応）
    mShader = GetOwner()->GetApp()->GetRenderer()->GetShader("Sprite");

    // 影用のテクスチャを自前生成（現状再現）
    auto tex = std::make_shared<Texture>();
    tex->CreateAlphaCircle(256, 0.5f, 0.3f, Vector3(0.0f, 0.0f, 0.0f), 0.8f);
    SetTexture(tex);

    // 新設計は “ワールド単位” サイズで指定する
    // ※ここはキャラサイズに合わせて後で詰める前提の仮値
    SetSize(100.0f, 100.0f);

    // 影は通常アルファ
    SetBlendAdd(false);

    // Sprite.frag の uSpriteColor / uSpriteAlpha に対応させる（影は基本白でOK）
    SetTint(Vector3(1.0f, 1.0f, 1.0f));
    SetAlpha(1.0f);
}

Matrix4 ShadowSpriteComponent::BuildWorldMatrix() const
{
    //--------------------------------------------------------------------------
    // 1) Yaw（ライト方向に合わせる：現状再現）
    //--------------------------------------------------------------------------
    float yaw = mYaw;

    if (mAutoRotateByLight)
    {
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
    }

    //--------------------------------------------------------------------------
    // 2) Scale（奥行方向を伸ばして“影の伸び”を作る）
    //
    // SpriteVerts は XY 平面の板なので、X回転で地面に寝かせた後は
    // Yスケールが “奥行(Z)” 相当になる。
    //--------------------------------------------------------------------------
    Matrix4 scale = Matrix4::CreateScale(
        mWidth * mOffsetScale,                  // 幅（X）
        mDepth * mOffsetScale * mStretch,       // 奥行（寝かせた後Z相当）
        1.0f
    );

    // 地面に寝かせる
    Matrix4 rotX = Matrix4::CreateRotationX(Math::ToRadians(90.0f));

    // ライト方向へ回転
    Matrix4 rotY = Matrix4::CreateRotationY(yaw);

    // 位置
    Matrix4 trans = Matrix4::CreateTranslation(
        GetOwner()->GetPosition() + mOffsetPosition
    );

    return scale * rotX * rotY * trans;
}

void ShadowSpriteComponent::Draw()
{
    if (!mIsVisible || !mTexture || !mShader)
    {
        return;
    }

    auto* renderer = GetOwner()->GetApp()->GetRenderer();
    if (!renderer)
    {
        return;
    }

    auto vao = renderer->GetSpriteVerts();
    if (!vao)
    {
        return;
    }

    //--------------------------------------------------------------------------
    // Blend（影は通常アルファ）
    //--------------------------------------------------------------------------
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    //--------------------------------------------------------------------------
    // Shader setup（Sprite）
    //--------------------------------------------------------------------------
    mShader->SetActive();

    Matrix4 view = renderer->GetViewMatrix();
    Matrix4 proj = renderer->GetProjectionMatrix();

    mShader->SetMatrixUniform("uViewProj", view * proj);
    mShader->SetMatrixUniform("uWorldTransform", BuildWorldMatrix());

    // Sprite.frag 用：Tint/Alpha（FootSpriteのUnlit uniformではなくこちらを使う）
    // 影はテクスチャが黒なので、基本は白TintでOK（色を変えたいならここで）
    mShader->SetVectorUniform("uSpriteColor", mTint);
    mShader->SetFloatUniform("uSpriteAlpha", mAlpha);

    // Texture
    mTexture->SetActive(0);
    mShader->SetTextureUniform("uTexture", 0);

    //--------------------------------------------------------------------------
    // Draw
    //--------------------------------------------------------------------------
    vao->SetActive();
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);

    renderer->AddDrawCall();
    renderer->AddDrawObject();
}

} // namespace toy
