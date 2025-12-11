#include "Graphics/Mesh/SkeletalMeshComponent.h"
#include "Engine/Render/Shader.h"
#include "Engine/Render/LightingManager.h"
#include "Asset/Geometry/Mesh.h"
#include "Engine/Core/Actor.h"
#include "Engine/Core/Application.h"
#include "Engine/Render/Renderer.h"
#include "Asset/Material/Texture.h"
#include "Asset/Geometry/VertexArray.h"
#include "Asset/Material/Material.h"
#include "Engine/Runtime/AnimationPlayer.h"

namespace toy {

//----------------------------------------------------------------------
// コンストラクタ
//  - MeshComponent 側の isSkeletal = true を使う前提
//  - スキニング用シェーダ／シャドウ用シェーダに差し替え
//----------------------------------------------------------------------
SkeletalMeshComponent::SkeletalMeshComponent(Actor* a, int drawOrder, VisualLayer layer)
: MeshComponent(a, drawOrder, layer,  true)
, mAnimTime(0.0f)
, mAnimPlayer(nullptr)
{
    auto renderer = GetOwner()->GetApp()->GetRenderer();
    mShader       = renderer->GetShader("Skinned");
    mShadowShader = renderer->GetShader("ShadowSkinned");
}

//----------------------------------------------------------------------
// 再生するアニメーション ID を設定
//  - mode は今のところ無視しているがインターフェースとして保持
//----------------------------------------------------------------------
void SkeletalMeshComponent::SetAnimID(const unsigned int animID, const bool mode)
{
    if (mAnimPlayer)
    {
        mAnimPlayer->Play(animID, true);
    }
}

//----------------------------------------------------------------------
// 通常描画
//  - ボーン行列(uMatrixPalette)をシェーダへ渡してスキニング描画
//  - MeshComponent::Draw とほぼ同じ構成＋スキニング用処理
//----------------------------------------------------------------------
void SkeletalMeshComponent::Draw()
{
    if (!mMesh) return;

    // 加算ブレンド指定時
    if (mIsBlendAdd)
    {
        glBlendFunc(GL_ONE, GL_ONE);
    }
    
    // シャドウマップテクスチャ
    mShadowMapTexture->SetActive(1);

    auto  renderer = GetOwner()->GetApp()->GetRenderer();
    Matrix4 view   = renderer->GetViewMatrix();
    Matrix4 proj   = renderer->GetProjectionMatrix();
    Matrix4 light  = renderer->GetLightSpaceMatrix();
    
    mShader->SetActive();
    mLightingManger->ApplyToShader(mShader, view);
    mShader->SetMatrixUniform("uViewProj", view * proj);
    mShader->SetMatrixUniform("uLightSpaceMatrix", light);
    mShader->SetTextureUniform("uShadowMap", 1);
    mShader->SetFloatUniform("uShadowBias", 0.005f);
    mShader->SetBooleanUniform("uUseToon", mIsToon);
    mShader->SetMatrixUniform("uWorldTransform", GetOwner()->GetWorldTransform());
    
    // ボーン行列パレットを取得してシェーダに送る
    std::vector<Matrix4> transforms =
        mAnimPlayer ? mAnimPlayer->GetFinalMatrices() : std::vector<Matrix4>();
    
    mShader->SetMatrixUniforms("uMatrixPalette",
                               transforms.data(),
                               static_cast<unsigned int>(transforms.size()));
    mShader->SetFloatUniform("uSpecPower", mMesh->GetSpecPower());
    
    // メッシュ本体描画
    auto va = mMesh->GetVertexArray();
    for (auto v : va)
    {
        auto mat = mMesh->GetMaterial(v->GetTextureID());
        if (mat)
        {
            mat->BindToShader(mShader);
        }
        v->SetActive();
        glDrawElements(GL_TRIANGLES, v->GetNumIndices(), GL_UNSIGNED_INT, nullptr);
        renderer->AddDrawCall();
    }
    
    // トゥーン輪郭描画（アウトライン用にスケール拡大＋表裏反転）
    if (mContourFactor > 1.0f)
    {
        glFrontFace(GL_CW);
        Matrix4 m = Matrix4::CreateScale(mContourFactor);
        mShader->SetMatrixUniform("uWorldTransform",
                                  m * GetOwner()->GetWorldTransform());
        for (auto v : va)
        {
            auto mat = mMesh->GetMaterial(v->GetTextureID());
            if (mat)
            {
                mat->SetOverrideColor(true, Vector3(0.0f, 0.0f, 0.0f));
                mat->BindToShader(mShader, 0);
            }
            v->SetActive();
            glDrawElements(GL_TRIANGLES, v->GetNumIndices(), GL_UNSIGNED_INT, nullptr);
            renderer->AddDrawCall();
            mat->SetOverrideColor(false, Vector3(0.0f, 0.0f, 0.0f));
        }
        glFrontFace(GL_CCW);
    }
    
    // 加算ブレンド解除
    if (mIsBlendAdd)
    {
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }
}

//----------------------------------------------------------------------
// シャドウ描画
//  - 通常描画と同様にボーン行列を渡しつつ、深度のみ書き込む想定
//----------------------------------------------------------------------
void SkeletalMeshComponent::DrawShadow()
{
    if (!mMesh) return;
    
    auto   renderer = GetOwner()->GetApp()->GetRenderer();
    Matrix4 light   = renderer->GetLightSpaceMatrix();
    
    mShadowShader->SetActive();
    mShadowShader->SetMatrixUniform("uWorldTransform", GetOwner()->GetWorldTransform());
    
    // アニメーション行列（無ければ空配列）
    static std::vector<Matrix4> gEmptyMatrixList;
    std::vector<Matrix4> transforms =
        mAnimPlayer ? mAnimPlayer->GetFinalMatrices() : gEmptyMatrixList;
    
    mShadowShader->SetMatrixUniforms("uMatrixPalette",
                                     transforms.data(),
                                     static_cast<unsigned int>(transforms.size()));
    mShadowShader->SetMatrixUniform("uLightSpaceMatrix", light);
    
    // メッシュをシャドウマップ用に描画
    auto va = mMesh->GetVertexArray();
    for (auto v : va)
    {
        v->SetActive();
        glDrawElements(GL_TRIANGLES, v->GetNumIndices(), GL_UNSIGNED_INT, nullptr);
        renderer->AddDrawCall();
    }
}

//----------------------------------------------------------------------
// Update
//  - 毎フレーム AnimationPlayer を進めるだけ
//----------------------------------------------------------------------
void SkeletalMeshComponent::Update(float deltaTime)
{
    if (mAnimPlayer)
    {
        mAnimPlayer->Update(deltaTime);
    }
}

//----------------------------------------------------------------------
// SetMesh
//  - MeshComponent 側の SetMesh を呼んだあと
//    その Mesh を使う AnimationPlayer を生成
//----------------------------------------------------------------------
void SkeletalMeshComponent::SetMesh(std::shared_ptr<Mesh> mesh)
{
    MeshComponent::SetMesh(mesh);
    mAnimPlayer = std::make_unique<AnimationPlayer>(mesh);
}

} // namespace toy
