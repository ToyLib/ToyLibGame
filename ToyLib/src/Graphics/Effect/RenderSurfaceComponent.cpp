#include "Graphics/Effect/RenderSurfaceComponent.h"

#include "Engine/Core/Actor.h"
#include "Engine/Core/Application.h"
#include "Engine/Render/Renderer.h"
#include "Engine/Render/Shader.h"
#include "Asset/Material/Texture.h"
#include "Asset/Geometry/VertexArray.h"

#include "glad/glad.h"

namespace toy {

RenderSurfaceComponent::RenderSurfaceComponent(Actor* owner, int drawOrder)
    : VisualComponent(owner, drawOrder, VisualLayer::Object3D)
    , mFlipX(true)
    , mFlipY(true)
    , mOpacity(1.0f)
    , mTint(Vector3(1.0f, 1.0f, 1.0f))
{
    // 置物の板ポリ：ビルボードなし、通常 3D と同じ扱い
    
    mVertexArray = GetOwner()->GetApp()->GetRenderer()->GetSurfaceQuad();
    mShader = GetOwner()->GetApp()->GetRenderer()->GetShader("RenderSurface");
}

void RenderSurfaceComponent::Draw()
{
    if (!IsVisible() || !mTexture )
    {
        return;
    }

    // ---- 状態（必要最低限）----
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    
    // 表面だけでいいなら cull 有効のままでOK
    // 裏面も見せたい演出があるなら、ここだけ無効にできる
    // glDisable(GL_CULL_FACE);
    auto renderer = GetOwner()->GetApp()->GetRenderer();
    
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
    
    mShader->SetActive();
    
    // 行列：ToyLib流（m * v）なので、シェーダ側も vec4 * mat4 で合わせる
    const Matrix4 world = GetOwner()->GetWorldTransform();
    mShader->SetMatrixUniform("uWorld", world);
    mShader->SetMatrixUniform("uView",  renderer->GetViewMatrix());
    mShader->SetMatrixUniform("uProj",  renderer->GetProjectionMatrix());
    
    // パラメータ
    mShader->SetBooleanUniform("uFlipX", mFlipX);
    mShader->SetBooleanUniform("uFlipY", mFlipY);
    mShader->SetFloatUniform("uOpacity", mOpacity);
    mShader->SetVectorUniform("uTint", mTint);
    
    // テクスチャ
    mTexture->SetActive(0);
    mShader->SetIntUniform("uSurfaceTex", 0);
    

    // 描画
    mVertexArray->SetActive();
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);
    
    renderer->AddDrawCall();
    renderer->AddDrawObject();
}

} // namespace toy
