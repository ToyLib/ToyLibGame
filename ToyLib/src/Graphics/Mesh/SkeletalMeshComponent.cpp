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
    
    if (!mMesh)
    {
        return;
    }

    // 加算ブレンド指定時
    if (mIsBlendAdd)
    {
        glBlendFunc(GL_ONE, GL_ONE);
    }

    auto renderer = GetOwner()->GetApp()->GetRenderer();
    Matrix4 view  = renderer->GetViewMatrix();
    Matrix4 proj  = renderer->GetProjectionMatrix();

    mShader->SetActive();

    // ライティング情報を反映
    mLightingManger->ApplyToShader(mShader, view);

    // 行列
    mShader->SetMatrixUniform("uViewProj", view * proj);

    // ----------------------------
    // CSM: ShadowMap 2枚 + LightVP 2本 + split/blend
    // ----------------------------
    {
        auto sm0 = renderer->GetShadowMapTexture(0);
        auto sm1 = renderer->GetShadowMapTexture(1);

        // Texture unit は空いている番号なら何でもOK（ここでは 6/7）
        if (sm0) sm0->SetActive(6);
        if (sm1) sm1->SetActive(7);

        // Phong.frag 側の uniform 名に合わせる
        mShader->SetTextureUniform("uShadowMap0", 6);
        mShader->SetTextureUniform("uShadowMap1", 7);

        mShader->SetMatrixUniform("uLightViewProj0", renderer->GetLightSpaceMatrix(0));
        mShader->SetMatrixUniform("uLightViewProj1", renderer->GetLightSpaceMatrix(1));

        // まず動かす用の固定値（後で Renderer の設定値へ）
        mShader->SetFloatUniform("uCascadeSplit0", renderer->GetCascadeSplit0());
        mShader->SetFloatUniform("uCascadeBlend", renderer->GetCascadeBlend());

        // Bias
        mShader->SetFloatUniform("uShadowBias", 0.005f);
    }

    // Toon ON/OFF
    mShader->SetBooleanUniform("uUseToon", mIsToon);

    
    Matrix4 worldMatrix =
        Matrix4::CreateFromQuaternion(mLocalRot) *
        Matrix4::CreateTranslation(mLocalPos) *
        Matrix4::CreateScale(mLocalScale) *
        GetOwner()->GetRenderWorldTransform();
    // World
    mShader->SetMatrixUniform("uWorldTransform", worldMatrix);

    // ----------------------------
    // ボーン行列パレット
    // ----------------------------
    static const std::vector<Matrix4> kEmpty;
    const std::vector<Matrix4> transforms =
        mAnimPlayer ? mAnimPlayer->GetFinalMatrices() : kEmpty;

    if (!transforms.empty())
    {
        mShader->SetMatrixUniforms(
            "uMatrixPalette",
            transforms.data(),
            static_cast<unsigned int>(transforms.size())
        );
    }

    mShader->SetFloatUniform("uSpecPower", mMesh->GetSpecPower());

    // ----------------------------
    // メッシュ本体描画
    // ----------------------------
    auto& va = mMesh->GetVertexArray();
    for (auto& v : va)
    {
        auto mat = mMesh->GetMaterial(v->GetTextureID());
        if (mat)
        {
            // こっちは元コード通り（必要なら BindToShader(mShader,0) へ統一でもOK）
            mat->BindToShader(mShader);
        }

        v->SetActive();
        glDrawElements(GL_TRIANGLES, v->GetNumIndices(), GL_UNSIGNED_INT, nullptr);
        renderer->AddDrawCall();
    }

    // ----------------------------
    // トゥーン輪郭描画（アウトライン）
    // ----------------------------
    if (mContourFactor > 1.0f)
    {
        glFrontFace(GL_CW);

        Matrix4 scaleOutline = Matrix4::CreateScale(mContourFactor);
        mShader->SetMatrixUniform(
            "uWorldTransform",
            scaleOutline * worldMatrix
        );

        for (auto& v : va)
        {
            auto mat = mMesh->GetMaterial(v->GetTextureID());
            if (mat)
            {
                mat->SetOverrideColor(true, mContourColor);
                mat->BindToShader(mShader, 0);
            }

            v->SetActive();
            glDrawElements(GL_TRIANGLES, v->GetNumIndices(), GL_UNSIGNED_INT, nullptr);
            renderer->AddDrawCall();

            // 上書きカラーを元に戻す
            if (mat)
            {
                mat->SetOverrideColor(false, Vector3(0.0f, 0.0f, 0.0f));
            }
        }

        glFrontFace(GL_CCW);
        renderer->AddDrawObject();
    }

    // 加算ブレンド解除
    if (mIsBlendAdd)
    {
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }

    renderer->AddDrawObject();
}

//----------------------------------------------------------------------
// シャドウ描画
//  - 通常描画と同様にボーン行列を渡しつつ、深度のみ書き込む想定
//----------------------------------------------------------------------
void SkeletalMeshComponent::DrawShadow(int cascadeIndex)
{
    if (!mMesh)
    {
        return;
    }

    auto renderer = GetOwner()->GetApp()->GetRenderer();

    // ★カスケード指定でライト行列取得
    Matrix4 light = renderer->GetLightSpaceMatrix(cascadeIndex);

    mShadowShader->SetActive();
    Matrix4 worldMatrix =
        Matrix4::CreateFromQuaternion(mLocalRot) *
        Matrix4::CreateTranslation(mLocalPos) *
        Matrix4::CreateScale(mLocalScale) *
        GetOwner()->GetRenderWorldTransform();
    mShadowShader->SetMatrixUniform(
        "uWorldTransform",
        worldMatrix
    );

    // アニメーション行列（無ければ空配列）
    static std::vector<Matrix4> gEmptyMatrixList;
    const std::vector<Matrix4> transforms =
            mAnimPlayer ? mAnimPlayer->GetFinalMatrices() : gEmptyMatrixList;
    if (!transforms.empty())
    {
        mShadowShader->SetMatrixUniforms(
            "uMatrixPalette",
            transforms.data(),
            static_cast<unsigned int>(transforms.size())
        );
    }

    mShadowShader->SetMatrixUniform("uLightSpaceMatrix", light);

    // シャドウマップ描画
    auto& va = mMesh->GetVertexArray();
    for (auto& v : va)
    {
        v->SetActive();
        glDrawElements(GL_TRIANGLES, v->GetNumIndices(), GL_UNSIGNED_INT, nullptr);
        renderer->AddDrawCall();
    }
    renderer->AddDrawObject();
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


void SkeletalMeshComponent::GatherRenderItems(RenderQueueLike &out)
{
    return;
}
} // namespace toy
