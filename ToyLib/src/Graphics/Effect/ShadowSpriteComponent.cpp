#include "Graphics/Effect/ShadowSpriteComponent.h"
#include "Engine/Core/Actor.h"
#include "Engine/Render/Renderer.h"
#include "Engine/Render/Shader.h"
#include "Asset/Material/Texture.h"
#include "Asset/Geometry/VertexArray.h"
#include "Engine/Core/Application.h"
#include "Engine/Render/Renderer.h"
#include "Engine/Render/LightingManager.h"
#include <memory>

namespace toy {

ShadowSpriteComponent::ShadowSpriteComponent(Actor* owner, int drawOrder)
: VisualComponent(owner, drawOrder)
, mTexture(nullptr)
, mScaleWidth(1.0f)
, mScaleHeight(1.0f)
{
    // 3D 空間上のエフェクトとして描画（地面に張り付くタイプ）
    mLayer = VisualLayer::Effect3D;

    // 通常のスプライト用シェーダを使用（簡易影テクスチャを貼る）
    mShader = GetOwner()->GetApp()->GetRenderer()->GetShader("Sprite");
    
    // 影用のテクスチャを自前生成（黒のアルファ付き円）
    //   size      : 256x256
    //   center    : (0.5, 0.3) 少し手前寄り
    //   color     : 黒
    //   blendPow  : 0.8（エッジの落ち方）
    mTexture = std::make_shared<Texture>();
    mTexture->CreateAlphaCircle(256, 0.5f, 0.3f, Vector3(0.0f, 0.0f, 0.0f), 0.8f);
}

ShadowSpriteComponent::~ShadowSpriteComponent()
{
    // 特に明示的な破棄は不要（smart pointer／エンジン側管理に任せる）
}

void ShadowSpriteComponent::SetTexture(std::shared_ptr<Texture> tex)
{
    // デフォルトの丸影テクスチャを差し替えたい場合に使用
    mTexture = tex;
}

void ShadowSpriteComponent::Draw()
{
    // 非表示またはテクスチャ未設定なら何もしない
    if (!mIsVisible || mTexture == nullptr) return;
    
    
    // 影は通常のアルファブレンド
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    // ----------------------------------------
    // 影スプライトのスケールを決定
    // ----------------------------------------
    float width  = static_cast<float>(mTexture->GetWidth())  * mScaleWidth;
    float height = static_cast<float>(mTexture->GetHeight()) * mScaleHeight;

    // mOffsetScale で全体の大きさを調整
    // ※高さ側は *3 して、やや楕円気味（足元影の潰れ感を演出）
    Matrix4 scale = Matrix4::CreateScale(
        width  * mOffsetScale,
        height * mOffsetScale * 3.0f,
        1.0f
    );
    
    // ----------------------------------------
    // 光源方向に合わせて影の向きを変える
    //   ・XZ 平面に射影したライトベクトルから回転角を求める
    //   ・影を「光と反対側に伸びる楕円」として表現
    // ----------------------------------------
    Vector3 lightDir = GetOwner()->GetApp()
        ->GetRenderer()
        ->GetLightingManager()
        ->GetLightDirection();
    
    // XZ 平面での向きだけ使う
    lightDir.y = 0.0f;
    if (lightDir.LengthSq() < 0.0001f)
        lightDir = Vector3(0.0f, 0.0f, 1.0f);
    lightDir.Normalize();
    
    float angle = atan2f(lightDir.x, lightDir.z);
    Matrix4 rotY = Matrix4::CreateRotationY(angle);
    
    // X 軸回転 90度で「地面に寝かせる」
    Matrix4 rotX = Matrix4::CreateRotationX(Math::ToRadians(90.0f));
    
    // Actor の位置 + オフセット に配置
    Matrix4 trans = Matrix4::CreateTranslation(
        GetOwner()->GetPosition() + mOffsetPosition
    );
    
    // 最終ワールド行列
    Matrix4 world = scale * rotX * rotY * trans;
    
    // ----------------------------------------
    // 描画セットアップ
    // ----------------------------------------
    mShader->SetActive();
    
    auto renderer = GetOwner()->GetApp()->GetRenderer();
    Matrix4 view = renderer->GetViewMatrix();
    Matrix4 proj = renderer->GetProjectionMatrix();
    
    mShader->SetMatrixUniform("uViewProj", view * proj);
    mShader->SetMatrixUniform("uWorldTransform", world);
    
    // 影用テクスチャをバインド
    mTexture->SetActive(0); // ShadowSprite 用テクスチャユニット
    mShader->SetTextureUniform("uTexture", 0);
    
    // フルスクリーンクアッド or 汎用スプライト用の VAO を使用
    mVertexArray->SetActive();
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);
    renderer->AddDrawCall();
    renderer->AddDrawObject();
}

} // namespace toy
