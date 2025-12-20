#include "Graphics/Effect/RenderSurfaceComponent.h"

//------------------------------------------------------------------------------
// Engine / Core
//------------------------------------------------------------------------------
#include "Engine/Core/Actor.h"
#include "Engine/Core/Application.h"

//------------------------------------------------------------------------------
// Engine / Render
//------------------------------------------------------------------------------
#include "Engine/Render/Renderer.h"
#include "Engine/Render/Shader.h"

//------------------------------------------------------------------------------
// Asset
//------------------------------------------------------------------------------
#include "Asset/Geometry/VertexArray.h"
#include "Asset/Material/Texture.h"

//------------------------------------------------------------------------------
// GL
//------------------------------------------------------------------------------
#include "glad/glad.h"

namespace toy {

//==============================================================================
// コンストラクタ
//==============================================================================
RenderSurfaceComponent::RenderSurfaceComponent(Actor* owner, int drawOrder)
    : VisualComponent(owner, drawOrder, VisualLayer::Object3D)
    , mFlipX(true)
    , mFlipY(true)
    , mOpacity(1.0f)
    , mTint(Vector3(1.0f, 1.0f, 1.0f))
{
    // 置物の板ポリ：ビルボードなし、通常 3D と同じ扱い
    auto* renderer = GetOwner()->GetApp()->GetRenderer();
    mVertexArray   = renderer->GetSurfaceQuad();
    mShader        = renderer->GetShader("RenderSurface");
}

//==============================================================================
// 描画
//==============================================================================
void RenderSurfaceComponent::Draw()
{
    // 表示OFF、もしくは表示対象テクスチャが無いなら何もしない
    if (!IsVisible() || !mTexture)
    {
        return;
    }

    //--------------------------------------------------------------------------
    // GL 状態（必要最低限）
    //--------------------------------------------------------------------------
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);

    // 裏面も見せたい演出があるなら、ここだけ無効にできる
    // glDisable(GL_CULL_FACE);

    auto* renderer = GetOwner()->GetApp()->GetRenderer();

    //--------------------------------------------------------------------------
    // 依存リソース（万一 null の場合に備えて再取得）
    //--------------------------------------------------------------------------
    if (!mVertexArray)
    {
        mVertexArray = renderer->GetSurfaceQuad();
    }
    if (!mShader)
    {
        mShader = renderer->GetShader("RenderSurface");
    }
    if (!mVertexArray || !mShader)
    {
        return;
    }

    //--------------------------------------------------------------------------
    // シェーダ設定
    //--------------------------------------------------------------------------
    mShader->SetActive();

    // 行列：ToyLib流（m * v）なので、シェーダ側も vec4 * mat4 で合わせる
    const Matrix4 world = GetOwner()->GetWorldTransform();
    mShader->SetMatrixUniform("uWorld", world);
    mShader->SetMatrixUniform("uView",  renderer->GetViewMatrix());
    mShader->SetMatrixUniform("uProj",  renderer->GetProjectionMatrix());

    // 演出パラメータ
    mShader->SetBooleanUniform("uFlipX", mFlipX);
    mShader->SetBooleanUniform("uFlipY", mFlipY);
    mShader->SetFloatUniform("uOpacity", mOpacity);
    mShader->SetVectorUniform("uTint",   mTint);

    // テクスチャ
    mTexture->SetActive(0);
    mShader->SetIntUniform("uSurfaceTex", 0);

    //--------------------------------------------------------------------------
    // 描画
    //--------------------------------------------------------------------------
    mVertexArray->SetActive();
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);

    // Debug カウンタ
    renderer->AddDrawCall();
    renderer->AddDrawObject();
}

} // namespace toy
