#include "Graphics/Sprite/FootSpriteComponent.h"

#include "Engine/Core/Actor.h"
#include "Engine/Core/Application.h"
#include "Engine/Render/Renderer.h"
#include "Engine/Render/Shader.h"
#include "Asset/Material/Texture.h"
#include "Asset/Geometry/VertexArray.h"

#include "glad/glad.h"
#include <algorithm>

namespace toy {

FootSpriteComponent::FootSpriteComponent(Actor* owner, int drawOrder, VisualLayer layer)
    : VisualComponent(owner, drawOrder)
    , mWidth(1.0f)
    , mDepth(1.0f)
    , mOffsetPosition(Vector3::Zero)
    , mOffsetScale(1.0f)
    , mYaw(0.0f)
    , mTint(Vector3(1.0f, 1.0f, 1.0f))
    , mAlpha(1.0f)
    
{
    mLayer = layer;
    mIsVisible = true;
    mShader = owner->GetApp()->GetRenderer()->GetShader("Unlit");
}

void FootSpriteComponent::SetTexture(std::shared_ptr<Texture> tex)
{
    mTexture = tex;
}

Matrix4 FootSpriteComponent::BuildWorldMatrix() const
{
    // ワールド単位サイズ
    Matrix4 scale = Matrix4::CreateScale(
        mWidth  * mOffsetScale,
        mDepth  * mOffsetScale,
        1.0f
    );

    // 地面に寝かせる
    Matrix4 rotX = Matrix4::CreateRotationX(Math::ToRadians(90.0f));

    // Yaw
    Matrix4 rotY = Matrix4::CreateRotationY(mYaw);

    // 位置
    Matrix4 trans = Matrix4::CreateTranslation(
        GetOwner()->GetPosition() + mOffsetPosition
    );

    // ToyLib流（君の現状に合わせる）：scale * rotX * rotY * trans
    return scale * rotX * rotY * trans;
}

void FootSpriteComponent::Draw()
{
    if (!mIsVisible) return;

    auto* app = GetOwner()->GetApp();
    auto renderer = app->GetRenderer();
    if (!renderer)
    {
        return;
    }

    // Shader
    if (!mShader)
    {
        return;
    }
    
    // VAO（Rendererが持つSpriteVertsを流用）
    auto vao = renderer->GetSpriteVerts();
    if (!vao) return;

    // Blend
    glEnable(GL_BLEND);
    if (mIsBlendAdd)
    {
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    }
    else
    {
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }

    PreDraw();

    mShader->SetActive();

    // 共通：ViewProj / World
    Matrix4 view = renderer->GetViewMatrix();
    Matrix4 proj = renderer->GetProjectionMatrix();
    mShader->SetMatrixUniform("uViewProj", view * proj);
    mShader->SetMatrixUniform("uWorldTransform", BuildWorldMatrix());

    // 互換のため（使わなくてもOK）
    // Unlit.vertが宣言しているなら、最低限ゼロでも渡しておくと安心
    // ※無ければSetMatrixUniform側が無視する実装なら不要
    // sh->SetMatrixUniform("uLightSpaceMatrix", Matrix4::Identity);

    // Texture
    const bool useTex = (mTexture != nullptr);
    if (useTex)
    {
        mTexture->SetActive(0);
        mShader->SetTextureUniform("uTexture", 0);
    }

    // Unlit用（Meshシェーダなら存在しない可能性があるので、SetXXXが安全に無視できる前提）
    mShader->SetIntUniform("uUseTexture", useTex ? 1 : 0);
    mShader->SetVectorUniform("uTint", mTint);
    mShader->SetFloatUniform("uAlpha", mAlpha);

    vao->SetActive();
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);

    renderer->AddDrawCall();
    renderer->AddDrawObject();

    PostDraw();
}

} // namespace toy
