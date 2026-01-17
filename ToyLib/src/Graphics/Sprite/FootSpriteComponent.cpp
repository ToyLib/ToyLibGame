#include "Graphics/Sprite/FootSpriteComponent.h"

#include "Engine/Core/Actor.h"
#include "Engine/Core/Application.h"
#include "Engine/Render/Renderer.h"
#include "Engine/Render/Shader.h"
#include "Asset/Material/Texture.h"
#include "Asset/Geometry/VertexArray.h"

// ★ Ground pose を取るため（ある場合だけ使う）
#include "Physics/GravityComponent.h"

#include "glad/glad.h"

namespace toy {

FootSpriteComponent::FootSpriteComponent(Actor* owner, int drawOrder, VisualLayer layer)
    : VisualComponent(owner, drawOrder)
{
    mLayer     = layer;
    mIsVisible = true;

    // Unlit（Phong互換uniform名がある前提）
    mShader = owner->GetApp()->GetRenderer()->GetShader("Unlit");
}

void FootSpriteComponent::SetTexture(std::shared_ptr<Texture> tex)
{
    mTexture = std::move(tex);
}

Matrix4 FootSpriteComponent::BuildWorldMatrix() const
{
    //--------------------------------------------------------------------------
    // 1) base position
    //  - GravityComponent がある場合だけ groundY に追従する
    //  - OffsetPosition は最後に加える（利用側で足元補正できる）
    //--------------------------------------------------------------------------
    Vector3 pos = GetOwner()->GetPosition();

    const GravityComponent* grav = GetOwner()->GetComponent<GravityComponent>();
    if (mSnapToGround && grav && grav->HasGroundPose())
    {
        pos.y = grav->GetGroundPose().y;
    }

    // 足元補正 + 浮かせ
    pos += mOffsetPosition;
    pos.y += mGroundLift;

    //--------------------------------------------------------------------------
    // 2) scale
    //  - SpriteVerts は XY 平面の Quad
    //  - X 回転で地面に寝かせると “Yスケールが奥行(Z)相当” になる
    //--------------------------------------------------------------------------
    Matrix4 scale = Matrix4::CreateScale(
        mWidth * mOffsetScale,
        mDepth * mOffsetScale,
        1.0f
    );

    // 地面に寝かせる（XY → XZ）
    Matrix4 rotX = Matrix4::CreateRotationX(Math::ToRadians(90.0f));

    // 地面上の回転（Yaw）
    Matrix4 rotY = Matrix4::CreateRotationY(mYaw);

    //--------------------------------------------------------------------------
    // 3) slope alignment（★今回追加）
    //  - AlignToGround=true のときだけ、GroundPose から傾きを取得して適用
    //  - Gravity が無い／GroundPose 無効なら Identity（従来どおり）
    //--------------------------------------------------------------------------
    Matrix4 rotSlope = Matrix4::Identity;

    if (mAlignToGround && grav && grav->HasGroundPose())
    {
        const auto& gp = grav->GetGroundPose();
        const Quaternion q = mUseSmoothGroundPose ? gp.smooth : gp.raw;

        // ※ ToyLib の Quaternion→Matrix 変換APIに合わせて差し替えてOK
        rotSlope = Matrix4::CreateFromQuaternion(q);
    }

    // 位置
    Matrix4 trans = Matrix4::CreateTranslation(pos);

    //--------------------------------------------------------------------------
    // 4) world (ToyLib 流 row-vector / SRT)
    //
    //  - rotX  : 地面に寝かせる
    //  - rotY  : Yaw（リング・影の向き）
    //  - rotSlope : 斜面追従（必要時のみ）
    //--------------------------------------------------------------------------
    return scale * rotX * rotY * rotSlope * trans;
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

    auto vao = renderer->GetSpriteVerts();
    if (!vao)
    {
        return;
    }

    //--------------------------------------------------------------------------
    // Blend
    //--------------------------------------------------------------------------
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, mIsBlendAdd ? GL_ONE : GL_ONE_MINUS_SRC_ALPHA);

    PreDraw();

    //--------------------------------------------------------------------------
    // Shader setup
    //--------------------------------------------------------------------------
    mShader->SetActive();

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
    //  - TextBillboard 等が同じ Unlit を使っても互換運用できるようにするため
    //--------------------------------------------------------------------------
    mShader->SetIntUniform("uUseTint", 1);
    mShader->SetIntUniform("uUseTexture", useTex ? 1 : 0);
    mShader->SetVectorUniform("uTint", mTint);
    mShader->SetFloatUniform("uAlpha", mAlpha);
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
