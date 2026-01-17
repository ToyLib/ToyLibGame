#include "Graphics/Sprite/FootSpriteComponent.h"

#include "Engine/Core/Actor.h"
#include "Engine/Core/Application.h"
#include "Engine/Render/Renderer.h"
#include "Engine/Render/Shader.h"
#include "Asset/Material/Texture.h"
#include "Asset/Geometry/VertexArray.h"

#include "glad/glad.h"

namespace toy {

FootSpriteComponent::FootSpriteComponent(Actor* owner, int drawOrder, VisualLayer layer)
    : VisualComponent(owner, drawOrder)
{
    // 描画レイヤー（Effect3D想定：depth on & depth mask off）
    mLayer = layer;

    // 表示ON
    mIsVisible = true;

    // Unlit（Phong互換uniform名がある前提）
    // TextBillboard 等も Unlit を使うので、FootSprite 側は “拡張モード” を明示して運用する
    mShader = owner->GetApp()->GetRenderer()->GetShader("Unlit");
}

void FootSpriteComponent::SetTexture(std::shared_ptr<Texture> tex)
{
    mTexture = std::move(tex);
}

Matrix4 FootSpriteComponent::BuildWorldMatrix() const
{
    //--------------------------------------------------------------------------
    // Scale
    //  - Renderer::GetSpriteVerts() は「XY 平面の Quad」なので、
    //    X回転で地面に寝かせると “Yスケールが奥行(Z)相当” になる。
    //--------------------------------------------------------------------------
    Matrix4 scale = Matrix4::CreateScale(
        mWidth * mOffsetScale,   // X = 幅
        mDepth * mOffsetScale,   // Y = 奥行（寝かせた後 Z 相当）
        1.0f
    );

    // 地面に寝かせる（XY → XZ）
    Matrix4 rotX = Matrix4::CreateRotationX(Math::ToRadians(90.0f));

    // 地面上の回転（Yaw）
    Matrix4 rotY = Matrix4::CreateRotationY(mYaw);

    // 位置（足元に貼る想定）
    Matrix4 trans = Matrix4::CreateTranslation(
        GetOwner()->GetPosition() + mOffsetPosition
    );

    // ToyLib 流（row-vector / SRT）：scale * rotX * rotY * trans
    return scale * rotX * rotY * trans;
}

void FootSpriteComponent::Draw()
{
    if (!mIsVisible)
    {
        return;
    }

    auto* renderer = GetOwner()->GetApp()->GetRenderer();
    if (!renderer || !mShader)
    {
        return;
    }

    // Quad（Sprite用）を流用
    auto vao = renderer->GetSpriteVerts();
    if (!vao)
    {
        return;
    }

    //--------------------------------------------------------------------------
    // Blend
    //--------------------------------------------------------------------------
    glEnable(GL_BLEND);
    glBlendFunc(
        GL_SRC_ALPHA,
        mIsBlendAdd ? GL_ONE : GL_ONE_MINUS_SRC_ALPHA
    );

    PreDraw();

    //--------------------------------------------------------------------------
    // Shader setup
    //--------------------------------------------------------------------------
    mShader->SetActive();

    // 共通：World / ViewProj（ToyLib：view * proj）
    Matrix4 view = renderer->GetViewMatrix();
    Matrix4 proj = renderer->GetProjectionMatrix();
    mShader->SetMatrixUniform("uViewProj", view * proj);
    mShader->SetMatrixUniform("uWorldTransform", BuildWorldMatrix());

    //--------------------------------------------------------------------------
    // Texture
    //--------------------------------------------------------------------------
    const bool useTex = (mTexture != nullptr);
    if (useTex)
    {
        mTexture->SetActive(0);
        mShader->SetTextureUniform("uTexture", 0);
    }

    //--------------------------------------------------------------------------
    // Unlit uniforms
    //
    // 重要：uUseTint=1 を FootSprite 側で必ず入れる
    //  - 同じ Unlit を使う TextBillboard 等は “互換モード（uUseTint=0）” で動かしたい
    //  - FootSprite は tint/alpha を使う前提なので、ここで明示して事故を防ぐ
    //--------------------------------------------------------------------------
    mShader->SetIntUniform("uUseTint", 1);                // FootSpriteは常に拡張モード
    mShader->SetIntUniform("uUseTexture", useTex ? 1 : 0);
    mShader->SetVectorUniform("uTint", mTint);
    mShader->SetFloatUniform("uAlpha", mAlpha);

    // テクスチャ無し運用（色だけ）に備えて常に渡しておく（Unlit側が未使用でもOK）
    mShader->SetVectorUniform("uDiffuseColor", mDiffuseColor);

    //--------------------------------------------------------------------------
    // Draw
    //--------------------------------------------------------------------------
    vao->SetActive();
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);

    renderer->AddDrawCall();
    renderer->AddDrawObject();

    PostDraw();
}

} // namespace toy
