// Graphics/Light/PointLightComponent.cpp
#include "Graphics/Light/PointLightComponent.h"

#include "Engine/Core/Actor.h"
#include "Engine/Core/Application.h"
#include "Engine/Render/Renderer.h"
#include "Engine/Render/LightingManager.h"

namespace toy {

PointLightComponent::PointLightComponent(Actor* owner, int updateOrder)
    : Component(owner, updateOrder)
    , mLighting(nullptr)
    , mColor(Vector3(1.0f, 1.0f, 1.0f))
    , mIntensity(3.0f)
    , mConstant(1.0f)
    , mLinear(0.09f)
    , mQuadratic(0.032f)
    , mRadius(28.0f)
    , mIsEnabled(true)
{
    // LightingManager に自動登録
    if (auto* app = owner->GetApp())
    {
        if (auto* renderer = app->GetRenderer())
        {
            auto lm = renderer->GetLightingManager(); // shared_ptr
            mLighting = lm.get();
            if (mLighting)
            {
                mLighting->RegisterPointLight(this);
            }
        }
    }
}

PointLightComponent::~PointLightComponent()
{
    // LightingManager から自動解除
    if (mLighting)
    {
        mLighting->UnregisterPointLight(this);
    }
}

void PointLightComponent::Update(float /*deltaTime*/)
{
    // 点滅・フェードアウトなどやりたくなったらここに書く
}

Vector3 PointLightComponent::GetPosition() const
{
    // Actor のワールド行列から平行移動成分を取得
    // （Matrix4 に GetTranslation() がある前提）
    return GetOwner()->GetWorldTransform().GetTranslation();
    // もしまだ WorldTransform が無ければ、暫定で:
    // return GetOwner()->GetPosition();
}

} // namespace toy
