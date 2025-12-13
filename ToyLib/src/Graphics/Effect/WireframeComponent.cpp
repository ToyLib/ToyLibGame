#include "Graphics/Effect/WireframeComponent.h"
#include "Engine/Core/Actor.h"
#include "Engine/Core/Application.h"
#include "Engine/Render/Renderer.h"
#include "Engine/Render/Shader.h"
#include "Engine/Render/LightingManager.h"
#include "Asset/Geometry/VertexArray.h"

namespace toy {

//------------------------------------------------------------
// コンストラクタ
//   ・ワイヤーフレーム描画用シェーダ（Solid）を取得
//   ・線色は白で初期化
//------------------------------------------------------------
WireframeComponent::WireframeComponent(Actor* owner, int drawOrder, VisualLayer layer)
    : VisualComponent(owner, drawOrder, layer)
    , mColor(Vector3(1.0f, 1.0f, 1.0f))
{
    // 単色描画シェーダ（Solid）を使用
    mShader = GetOwner()->GetApp()->GetRenderer()->GetShader("Solid");
}

//------------------------------------------------------------
// Draw()
//   ・登録された VertexArray を線描画（ワイヤーフレーム）する
//   ・LightingManager から最低限のライト設定を反映
//   ・GL_LINE_STRIP による線の描画
//------------------------------------------------------------


void WireframeComponent::Draw()
{
    if (!mIsVisible) return;
    
    auto renderer = GetOwner()->GetApp()->GetRenderer();
    Matrix4 view = renderer->GetViewMatrix();
    Matrix4 proj = renderer->GetProjectionMatrix();
    
    // シェーダーアクティブ
    mShader->SetActive();
    
    // ライティングの基本ユニフォームをセット（Solid シェーダー側で使用）
    mLightingManager->ApplyToShader(mShader, view);
    
    // VP 行列
    mShader->SetMatrixUniform("uViewProj", view * proj);
    
    // 線色
    mShader->SetVectorUniform("uSolColor", mColor);
    
    // ワールド変換
    mShader->SetMatrixUniform("uWorldTransform", GetOwner()->GetWorldTransform());
    
    // メッシュ描画
    if (mVertexArray)
    {
        mVertexArray->SetActive();

        glDrawArrays(GL_LINES, 0, mVertexArray->GetNumVerts());
        renderer->AddDrawCall();
    }
    renderer->AddDrawObject();
}

} // namespace toy
