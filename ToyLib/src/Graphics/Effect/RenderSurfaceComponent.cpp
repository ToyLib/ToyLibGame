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
    , mScaleX(1.0f)
    , mScaleY(1.0f)
    , mOpacity(1.0f)
    , mTint(Vector3(1.0f, 1.0f, 1.0f))
    , mMode(SurfaceMode::Mirror)
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
    Matrix4 sc = Matrix4::CreateScale(mScaleX, mScaleY, 1.0f);
    GetOwner()->ComputeWorldTransform();
    const Matrix4 world = sc * GetOwner()->GetWorldTransform();
    
    
    
    mShader->SetMatrixUniform("uWorld", world);
    mShader->SetMatrixUniform("uView",  renderer->GetViewMatrix());
    mShader->SetMatrixUniform("uProj",  renderer->GetProjectionMatrix());

    // 演出パラメータ
    mShader->SetBooleanUniform("uFlipX", mFlipX);
    mShader->SetBooleanUniform("uFlipY", mFlipY);
    mShader->SetFloatUniform("uOpacity", mOpacity);
    mShader->SetVectorUniform("uTint",   mTint);
    
    float gameTime = GetOwner()->GetApp()->GetTimeSconds();
    mShader->SetFloatUniform("uTime", gameTime);
    
    switch (mMode)
    {
        case SurfaceMode::Plain:
            mShader->SetIntUniform("uMode", 0);

            mShader->SetFloatUniform("uDistortStrength", 0.0f);
            mShader->SetFloatUniform("uScanlineStrength", 0.0f);

            mShader->SetFloatUniform("uFresnel", 0.0f);
            mShader->SetFloatUniform("uFresnelPow", 1.0f);

            mShader->SetFloatUniform("uWaveSpeed", 0.0f);
            break;
        case SurfaceMode::Monitor:
            mShader->SetIntUniform("uMode", 0);
            
            mShader->SetFloatUniform("uDistortStrength", 0.0f);
            
            mShader->SetFloatUniform("uScanlineStrength", 0.15f); // 0.1〜0.25
            
            mShader->SetFloatUniform("uFresnel", 0.0f);
            mShader->SetFloatUniform("uFresnelPow", 1.0f);
            
            mShader->SetFloatUniform("uWaveSpeed", 0.0f);
            

            break;
        case SurfaceMode::Mirror:
            mShader->SetIntUniform("uMode", 1);

            mShader->SetFloatUniform("uDistortStrength", 0.01f); // 超重要：弱く

            mShader->SetFloatUniform("uScanlineStrength", 0.0f);

            mShader->SetFloatUniform("uFresnel", 0.5f);   // カメラ側で計算できないなら固定でOK
            mShader->SetFloatUniform("uFresnelPow", 5.0f); // 4〜8くらいが自然

            mShader->SetFloatUniform("uWaveSpeed", 0.0f);
            break;
        case SurfaceMode::Water:
            mShader->SetIntUniform("uMode", 2);

            mShader->SetFloatUniform("uDistortStrength", 0.03f); // 0.02〜0.04

            mShader->SetFloatUniform("uScanlineStrength", 0.0f);

            mShader->SetFloatUniform("uFresnel", 0.0f);
            mShader->SetFloatUniform("uFresnelPow", 1.0f);

            mShader->SetFloatUniform("uWaveSpeed", 2.0f); // 0.5〜2.0
            mShader->SetFloatUniform("uSwayStrength", 0.006f);
            mShader->SetFloatUniform("uSparkleStrength", 0.1f);
            break;
    }
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
