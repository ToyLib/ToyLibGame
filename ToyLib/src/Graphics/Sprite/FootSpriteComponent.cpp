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

static Matrix4 BuildAlignToGround(const Vector3& groundNormal, float yawRad)
{
    Vector3 up = groundNormal;
    if (up.LengthSq() <= Math::NearZeroEpsilon)
    {
        up = Vector3::UnitY;
    }
    up.Normalize();

    // yaw から “ワールド水平の前” を作る（+Z基準）
    Vector3 fwd(Math::Sin(yawRad), 0.0f, Math::Cos(yawRad));

    // 前ベクトルを地面平面に射影して直交化
    fwd = fwd - up * Vector3::Dot(fwd, up);
    if (fwd.LengthSq() <= Math::NearZeroEpsilon)
    {
        // up とほぼ平行になったら別軸で作る
        fwd = Vector3::Cross(Vector3::UnitX, up);
    }
    fwd.Normalize();

    Vector3 right = Vector3::Cross(up, fwd);
    if (right.LengthSq() <= Math::NearZeroEpsilon)
    {
        right = Vector3::UnitX;
    }
    right.Normalize();

    // row-vector: 行に軸を詰める
    Matrix4 m = Matrix4::Identity;
    m.SetXAxis(right);
    m.SetYAxis(up);
    m.SetZAxis(fwd);
    return m;
}


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
    Vector3 pos = GetOwner()->GetPosition();

    const GravityComponent* grav = GetOwner()->GetComponent<GravityComponent>();

    //========================================
    // (1) 基準XZを “足OBB下面中心” に寄せる
    //========================================
    if (grav && grav->HasFootBottom())
    {
        const Vector3 b = grav->GetFootBottomPos();
        pos.x = b.x;
        pos.z = b.z;
    }

    //========================================
    // (2) Yは groundY にスナップ
    //========================================
    if (mSnapToGround && grav && grav->HasGroundPose())
    {
        pos.y = grav->GetGroundPose().y;
    }

    // offset + lift
    pos += mOffsetPosition;
    pos.y += mGroundLift;

    // scale（XY quad）
    Matrix4 scale = Matrix4::CreateScale(
        mWidth * mOffsetScale,
        mDepth * mOffsetScale,
        1.0f);

    // XY quad を地面に寝かせる（XY → XZ）
    Matrix4 rotLay = Matrix4::CreateRotationX(Math::ToRadians(90.0f));

    // slope alignment（yaw込みで作るので rotY は不要）
    Matrix4 rot = Matrix4::Identity;

    if (mAlignToGround && grav && grav->HasGroundPose())
    {
        rot = BuildAlignToGround(grav->GetGroundPose().normal, mYaw);
    }
    else
    {
        // 従来どおり水平yawだけ
        rot = Matrix4::CreateRotationY(mYaw);
    }

    Matrix4 trans = Matrix4::CreateTranslation(pos);

    // row-vector: S * R * T
    //  - rotLay: 板を寝かせる
    //  - rot   : yaw or slope+ yaw
    return scale * rotLay * rot * trans;
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
