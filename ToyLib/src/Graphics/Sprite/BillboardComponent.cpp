#include "Graphics/Sprite/BillboardComponent.h"
#include "Asset/Material/Texture.h"
#include "Engine/Render/Shader.h"
#include "Engine/Render/LightingManager.h"
#include "Asset/Geometry/VertexArray.h"
#include "Engine/Core/Application.h"
#include "Engine/Core/Actor.h"
#include "Engine/Render/Renderer.h"
#include "glad/glad.h"

namespace toy {

BillboardComponent::BillboardComponent(class Actor* a, int drawOrder)
: VisualComponent(a, drawOrder, VisualLayer::Effect3D)
, mScale(1.0f)
{
    // メッシュ用シェーダーを流用（板ポリをメッシュ扱いで描画）
    mShader = GetOwner()->GetApp()->GetRenderer()->GetShader("Mesh");
}

BillboardComponent::~BillboardComponent()
{
}

void BillboardComponent::Draw()
{
    // 非表示 or テクスチャ未設定ならスキップ
    if (!mIsVisible || !mTexture)
    {
        return;
    }

    // 加算ブレンド指定時だけブレンドモードを一時変更
    if (mIsBlendAdd)
    {
        glBlendFunc(GL_ONE, GL_ONE);
    }

    auto* renderer = GetOwner()->GetApp()->GetRenderer();

    // カメラ行列
    Matrix4 view = renderer->GetViewMatrix();
    Matrix4 proj = renderer->GetProjectionMatrix();

    // ============================
    // カメラ方向を向く回転を計算
    // ============================

    // ★ ここをローカル位置 → ワールド位置に変更
    // Vector3 pos = GetOwner()->GetPosition();
    Matrix4 actorWorld = GetOwner()->GetWorldTransform();
    Vector3 pos = actorWorld.GetTranslation();

    // ビュー行列の逆行列からカメラ位置を取得
    Matrix4 invView = renderer->GetInvViewMatrix();
    Vector3 cameraPos = invView.GetTranslation();

    // カメラ → ビルボードへの水平ベクトル
    Vector3 toCamera = pos - cameraPos;
    toCamera.y = 0.0f;      // Yは固定してXZ平面上だけで回転

    // ゼロ長近辺を回避（カメラとほぼ同じXZ位置の場合）
    if (toCamera.LengthSq() < 1.0e-6f)
    {
        toCamera = Vector3::UnitZ;
    }
    else
    {
        toCamera.Normalize();
    }

    // Y軸回転角（atan2(x, z) で方位角取得）
    float angle = atan2f(toCamera.x, toCamera.z);
    Matrix4 rotY = Matrix4::CreateRotationY(angle);

    // ============================
    // スケール・平行移動行列
    // ============================
    float scale = mScale * GetOwner()->GetScale();

    Matrix4 scaleMat = Matrix4::CreateScale(
        mTexture->GetWidth()  * scale,
        mTexture->GetHeight() * scale,
        1.0f
    );

    Matrix4 translate = Matrix4::CreateTranslation(pos);

    // ============================
    // シェーダー設定
    // ============================
    mShader->SetActive();

    // ライティング情報を設定（環境光やディレクショナルライトなど）
    if (mLightingManager)
    {
        mLightingManager->ApplyToShader(mShader, view);
    }

    // ★ 行列の掛け順は「元のまま」維持
    Matrix4 world = scaleMat * rotY * translate;
    mShader->SetMatrixUniform("uWorldTransform", world);

    // ビュー・プロジェクション行列
    mShader->SetMatrixUniform("uViewProj", view * proj);

    // テクスチャ
    mTexture->SetActive(0);
    mShader->SetTextureUniform("uTexture", 0);

    // ============================
    // 描画
    // ============================
    if (mVertexArray)
    {
        mVertexArray->SetActive();
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);
    }

    // 加算ブレンドを使った場合は元に戻しておく
    if (mIsBlendAdd)
    {
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }
}

} // namespace toy
