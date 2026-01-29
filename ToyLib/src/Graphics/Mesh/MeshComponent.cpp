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
    , mIsSkeletal(isSkeletal)
{
    auto renderer = GetOwner()->GetApp()->GetRenderer();
    mShader          = renderer->GetShader("Mesh");
    mShadowShader    = renderer->GetShader("ShadowMesh");
    mLightingManger  = renderer->GetLightingManager();

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
    // DrawPass変更のため早期リターン
    return;
    if (!mMesh)
    {
        return;
    }

    // 加算ブレンドが指定されている場合はブレンドモード変更
    if (mIsBlendAdd)
    {
        glBlendFunc(GL_ONE, GL_ONE);
    }

    auto renderer = GetOwner()->GetApp()->GetRenderer();

    Matrix4 view = renderer->GetViewMatrix();
    Matrix4 proj = renderer->GetProjectionMatrix();

    // メインのメッシュシェーダを使用
    mShader->SetActive();

    // ライティング情報をシェーダに反映
    mLightingManger->ApplyToShader(mShader, view);

    // 行列類
    mShader->SetMatrixUniform("uViewProj", view * proj);

    // ----------------------------
    // CSM: ShadowMap 2枚 + LightVP 2本 + split/blend
    // ----------------------------
    // ※ Texture unit は空いている番号なら何でもOK。ここでは 6/7 を使用。
    {
        auto sm0 = renderer->GetShadowMapTexture(0);
        auto sm1 = renderer->GetShadowMapTexture(1);

        if (sm0) sm0->SetActive(6);
        if (sm1) sm1->SetActive(7);

        // Phong.frag 側の uniform 名に合わせる
        mShader->SetTextureUniform("uShadowMap0", 6);
        mShader->SetTextureUniform("uShadowMap1", 7);

        mShader->SetMatrixUniform("uLightViewProj0", renderer->GetLightSpaceMatrix(0));
        mShader->SetMatrixUniform("uLightViewProj1", renderer->GetLightSpaceMatrix(1));

        // まず動かす用の固定値（あとで Renderer 側の設定値に置き換え推奨）
        mShader->SetFloatUniform("uCascadeSplit0", renderer->GetCascadeSplit0());
        mShader->SetFloatUniform("uCascadeBlend", renderer->GetCascadeBlend());

        // Bias
        mShader->SetFloatUniform("uShadowBias", 0.005f);
    }

    // トゥーンレンダリングON/OFF
    mShader->SetBooleanUniform("uUseToon", mIsToon);

    // ワールド変換を送る
    Matrix4 worldMatrix =
        Matrix4::CreateFromQuaternion(mLocalRot) *
        Matrix4::CreateTranslation(mLocalPos) *
        Matrix4::CreateScale(mLocalScale) *
        GetOwner()->GetRenderWorldTransform();

    mShader->SetMatrixUniform("uWorldTransform", worldMatrix);

    //--------------------------------------------------------
    // メッシュ本体の描画
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
        renderer->AddDrawCall();
    }

    //--------------------------------------------------------
    // トゥーン輪郭描画（アウトライン）
    //--------------------------------------------------------
    if (mContourFactor > 1.0f)
    {
        glFrontFace(GL_CW);

        // わずかにスケールアップしたワールド行列
        Matrix4 scaleOutline = Matrix4::CreateScale(mContourFactor);
        mShader->SetMatrixUniform(
            "uWorldTransform",
            scaleOutline * worldMatrix
        );

        for (auto& v : vaList)
        {
            auto mat = mMesh->GetMaterial(v->GetTextureID());
            if (mat)
            {
                // 色を強制的に黒に上書きするモード
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

    // 加算ブレンドを戻す
    if (mIsBlendAdd)
    {
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }

    renderer->AddDrawObject();
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
void MeshComponent::DrawShadow(int cascadeIndex)
{
    if (!mMesh) return;

    auto renderer = GetOwner()->GetApp()->GetRenderer();

    // ★カスケード指定で取得
    Matrix4 light = renderer->GetLightSpaceMatrix(cascadeIndex);

    mShadowShader->SetActive();
    
    Matrix4 worldMatrix =
        Matrix4::CreateFromQuaternion(mLocalRot) *
        Matrix4::CreateTranslation(mLocalPos) *
        Matrix4::CreateScale(mLocalScale) *
        GetOwner()->GetRenderWorldTransform();
    
    mShadowShader->SetMatrixUniform("uWorldTransform", worldMatrix);
    mShadowShader->SetMatrixUniform("uLightSpaceMatrix", light);

    auto vaList = mMesh->GetVertexArray();
    for (auto& v : vaList)
    {
        v->SetActive();
        glDrawElements(GL_TRIANGLES, v->GetNumIndices(), GL_UNSIGNED_INT, nullptr);
        renderer->AddDrawCall();
    }
    renderer->AddDrawObject();
}

void MeshComponent::GatherRenderItems(RenderQueueLike& out)
{
    if (!mIsVisible || !mMesh) return;

    auto* renderer = GetOwner()->GetApp()->GetRenderer();

    // viewProj（ToyLib流：view * proj）
    Matrix4 view = renderer->GetViewMatrix();
    Matrix4 proj = renderer->GetProjectionMatrix();
    Matrix4 viewProj = view * proj;

    // world transform（既存Draw()の合成そのまま）
    Matrix4 worldMatrix =
        Matrix4::CreateFromQuaternion(mLocalRot) *
        Matrix4::CreateTranslation(mLocalPos) *
        Matrix4::CreateScale(mLocalScale) *
        GetOwner()->GetRenderWorldTransform();

    // サブメッシュ単位で item を積む
    auto vaList = mMesh->GetVertexArray();
    for (auto& v : vaList)
    {
        if (!v) continue;

        RenderItem it;
        it.type      = RenderItemType::Mesh;
        it.pass      = RenderPass::Main;
        it.layer     = GetLayer();
        it.drawOrder = GetDrawOrder();

        it.topology   = PrimitiveTopology::Triangles;
        it.geometry   = GeometryHandle{ v.get() };
        it.indexCount = v->GetNumIndices();

        // state（通常メッシュ）
        it.depthTest  = true;
        it.depthWrite = true;

        // MeshComponentの加算ブレンドフラグを尊重（旧Draw()と同じ）
        it.blend = mIsBlendAdd ? BlendMode::Additive : BlendMode::Opaque;

        it.cull      = CullMode::Back;
        it.frontFace = FrontFace::CCW;

        // shader
        it.shader = renderer->GetShaderHandle("Mesh");

        // transforms
        it.world   = worldMatrix;
        it.viewProj= viewProj;
        
        it.toon         = mIsToon;

        // material
        auto mat = mMesh->GetMaterial(v->GetTextureID());
        it.material = renderer->ToHandle(mat);

        out.Push(it);
    }
}

} // namespace toy
