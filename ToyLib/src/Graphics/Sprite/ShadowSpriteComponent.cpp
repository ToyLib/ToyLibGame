#include "Graphics/Sprite/ShadowSpriteComponent.h"

#include "Engine/Core/Actor.h"
#include "Engine/Core/Application.h"
#include "Engine/Render/Renderer.h"
#include "Engine/Render/LightingManager.h"
#include "Asset/Material/Texture.h"

namespace toy {

ShadowSpriteComponent::ShadowSpriteComponent(Actor* owner, int drawOrder)
    : FootSpriteComponent(owner, drawOrder, VisualLayer::Effect3D)
{
    // 基本はUnlitでOK（影はライティングしない）
    SetShaderName("Sprite");

    // 影用のテクスチャを自前生成（現状再現）
    auto tex = std::make_shared<Texture>();
    tex->CreateAlphaCircle(256, 0.5f, 0.3f, Vector3(0.0f, 0.0f, 0.0f), 0.8f);
    SetTexture(tex);

    // いままで GetWidth/Height を使ってたけど、
    // 新設計ではワールドサイズで指定するので適当に初期値を入れておく
    // （元の見た目に合わせるならキャラの体格に合わせて調整）
    SetSize(100.0f, 100.0f);

    // 地面への埋まり防止（必要なら）
    // SetOffsetPosition(Vector3(0.0f, 0.02f, 0.0f));

    SetBlendMode(FootBlendMode::Alpha);
    SetTint(Vector3(1.0f, 1.0f, 1.0f));
    SetAlpha(1.0f);
}

Matrix4 ShadowSpriteComponent::BuildWorldMatrix() const
{
    // ライト方向でYaw決定（現状再現）
    float yaw = mYaw;
    if (mAutoRotateByLight)
    {
        auto* app = GetOwner()->GetApp();
        auto renderer = app->GetRenderer();
        auto lm = renderer->GetLightingManager();

        Vector3 lightDir = lm->GetLightDirection();
        lightDir.y = 0.0f;

        if (lightDir.LengthSq() < 0.0001f)
        {
            lightDir = Vector3(0.0f, 0.0f, 1.0f);
        }
        lightDir.Normalize();

        yaw = atan2f(lightDir.x, lightDir.z);
    }

    // 影は奥行方向に潰して“伸び”を作る（現状の *3.0f 相当）
    Matrix4 scale = Matrix4::CreateScale(
        mWidth  * mOffsetScale,
        mDepth  * mOffsetScale * mStretch,
        1.0f
    );

    Matrix4 rotX  = Matrix4::CreateRotationX(Math::ToRadians(90.0f));
    Matrix4 rotY  = Matrix4::CreateRotationY(yaw);
    Matrix4 trans = Matrix4::CreateTranslation(GetOwner()->GetPosition() + mOffsetPosition);

    return scale * rotX * rotY * trans;
}

} // namespace toy
