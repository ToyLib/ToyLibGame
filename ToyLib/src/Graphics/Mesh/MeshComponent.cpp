#include "Graphics/Mesh/MeshComponent.h"
#include "Engine/Render/Shader.h"
#include "Engine/Render/LightingManager.h"
#include "Asset/Geometry/Mesh.h"
#include "Engine/Core/Actor.h"
#include "Engine/Core/Application.h"
#include "Engine/Render/Renderer.h"
#include "Asset/Material/Texture.h"
#include "Asset/Geometry/VertexArray.h"
#include "Asset/Material/Material.h"

#include "glad/glad.h"
#include <vector>

namespace toy {

//------------------------------------------------------------
// コンストラクタ
//  - Renderer からシェーダやライト情報を取得
//  - デフォルトでは 3Dオブジェクトレイヤー & 影あり
//------------------------------------------------------------
MeshComponent::MeshComponent(Actor* a, int drawOrder, VisualLayer layer, bool isSkeletal)
    : VisualComponent(a, drawOrder, layer)
    , mMesh(nullptr)
    , mTextureIndex(0)
    , mIsSkeletal(isSkeletal)
    , mIsToon(false)
    , mContourFactor(1.0f)
{
    auto renderer = GetOwner()->GetApp()->GetRenderer();
    mShader          = renderer->GetShader("Mesh");
    mShadowShader    = renderer->GetShader("ShadowMesh");
    mLightingManger  = renderer->GetLightingManager();
    mShadowMapTexture = renderer->GetShadowMapTexture();

    mIsVisible    = true;
    mLayer        = VisualLayer::Object3D;  // Mesh は基本3Dオブジェクト扱い
    mEnableShadow = true;
}

//------------------------------------------------------------
// デストラクタ
//------------------------------------------------------------
MeshComponent::~MeshComponent()
{
}

//------------------------------------------------------------
// Draw()
//  - 通常描画
//  - シャドウマップ + ライティング + マテリアルを反映
//  - オプションでトゥーン輪郭を追加描画
//------------------------------------------------------------
void MeshComponent::Draw()
{
    if (!mMesh) return;

    // 加算ブレンドが指定されている場合はブレンドモード変更
    if (mIsBlendAdd)
    {
        glBlendFunc(GL_ONE, GL_ONE);
    }

    // シャドウマップテクスチャ有効化（テクスチャユニット1）
    mShadowMapTexture->SetActive(1);

    auto renderer = GetOwner()->GetApp()->GetRenderer();
    Matrix4 view  = renderer->GetViewMatrix();
    Matrix4 proj  = renderer->GetProjectionMatrix();
    Matrix4 light = renderer->GetLightSpaceMatrix();

    // メインのメッシュシェーダを使用
    mShader->SetActive();

    // ライティング情報をシェーダに反映
    mLightingManger->ApplyToShader(mShader, view);

    // 行列類
    mShader->SetMatrixUniform("uViewProj", view * proj);
    mShader->SetMatrixUniform("uLightSpaceMatrix", light);

    // シャドウマップサンプラ設定
    mShader->SetTextureUniform("uShadowMap", 1);
    mShader->SetFloatUniform("uShadowBias", 0.005f);

    // トゥーンレンダリングON/OFF
    mShader->SetBooleanUniform("uUseToon", mIsToon);

    // ワールド変換を送る
    mShader->SetMatrixUniform("uWorldTransform", GetOwner()->GetWorldTransform());

    //--------------------------------------------------------
    // メッシュ本体の描画
    //  - Mesh は複数 VertexArray（サブメッシュ）を持つ前提
    //  - 各サブメッシュに対応した Material をバインドして描画
    //--------------------------------------------------------
    auto vaList = mMesh->GetVertexArray();
    for (auto& v : vaList)
    {
        auto mat = mMesh->GetMaterial(v->GetTextureID());
        if (mat)
        {
            // Diffuse / Specular / Texture 等をまとめてバインド
            mat->BindToShader(mShader, 0);
        }

        v->SetActive();
        glDrawElements(GL_TRIANGLES, v->GetNumIndices(), GL_UNSIGNED_INT, nullptr);
    }

    //--------------------------------------------------------
    // トゥーン輪郭描画（アウトライン）
    //  - 表面を少しスケールアップして黒で描画
    //  - CW / CCW を反転して裏面を描くことで輪郭として見せる
    //--------------------------------------------------------
    if (mIsToon)
    {
        // 反時計回り(CCW)→時計回り(CW)に変更し裏面描画にする
        glFrontFace(GL_CW);

        // わずかにスケールアップしたワールド行列
        Matrix4 scaleOutline = Matrix4::CreateScale(mContourFactor);
        mShader->SetMatrixUniform("uWorldTransform", scaleOutline * GetOwner()->GetWorldTransform());

        for (auto& v : vaList)
        {
            auto mat = mMesh->GetMaterial(v->GetTextureID());
            if (mat)
            {
                // 色を強制的に黒に上書きするモード
                mat->SetOverrideColor(true, Vector3(0.f, 0.f, 0.f));
                mat->BindToShader(mShader, 0);
            }

            v->SetActive();
            glDrawElements(GL_TRIANGLES, v->GetNumIndices(), GL_UNSIGNED_INT, nullptr);

            // 上書きカラーを元に戻す
            if (mat)
            {
                mat->SetOverrideColor(false, Vector3(0.f, 0.f, 0.f));
            }
        }

        // フロントフェイスを元に戻す
        glFrontFace(GL_CCW);
    }

    // 加算ブレンドを戻す
    if (mIsBlendAdd)
    {
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }
}

//------------------------------------------------------------
// GetVertexArray()
//  - 指定インデックスのサブメッシュ VAO を取得
//  - デバッグ用やカスタム描画に利用
//------------------------------------------------------------
std::shared_ptr<VertexArray> MeshComponent::GetVertexArray(int id) const
{
    return mMesh->GetVertexArray()[id];
}

//------------------------------------------------------------
// DrawShadow()
//  - シャドウマップ用の深度描画
//  - ライティングは不要で、LightSpaceMatrix と WorldTransform のみ
//------------------------------------------------------------
void MeshComponent::DrawShadow()
{
    if (!mMesh) return;

    auto renderer = GetOwner()->GetApp()->GetRenderer();
    Matrix4 light = renderer->GetLightSpaceMatrix();

    // シャドウ専用シェーダを有効化
    mShadowShader->SetActive();

    // ワールド行列＆ライト空間行列を送る
    mShadowShader->SetMatrixUniform("uWorldTransform", GetOwner()->GetWorldTransform());
    mShadowShader->SetMatrixUniform("uLightSpaceMatrix", light);

    // VAO を全サブメッシュ分描画
    auto vaList = mMesh->GetVertexArray();
    for (auto& v : vaList)
    {
        v->SetActive();
        glDrawElements(GL_TRIANGLES, v->GetNumIndices(), GL_UNSIGNED_INT, nullptr);
    }
}

} // namespace toy
