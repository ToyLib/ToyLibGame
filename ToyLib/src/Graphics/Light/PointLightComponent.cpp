#include "Graphics/Light/PointLightComponent.h"

#include "Engine/Core/Actor.h"
#include "Engine/Core/Application.h"
#include "Render/IRenderer.h"
#include "Render/LightingManager.h"

namespace toy {

//======================================================================
// PointLightComponent
//----------------------------------------------------------------------
// Actor に「点光源」を付与するコンポーネント。
//
// ・生成時に LightingManager に自動登録し、破棄時に自動解除する
// ・ライトの位置は Owner(Actor) のワールド座標（平行移動成分）を参照する
// ・点滅/フェードなどの演出は Update() に追加していく想定
//
//======================================================================

PointLightComponent::PointLightComponent(Actor* owner, int updateOrder)
    : Component(owner, updateOrder)
    , mLighting(nullptr)
    , mColor(Vector3(1.0f, 1.0f, 1.0f))  // デフォルトは白
    , mIntensity(3.0f)                  // 明るさスケール（シェーダ側で乗算される想定）
    , mConstant(1.0f)                   // 減衰：定数項
    , mLinear(0.09f)                    // 減衰：一次項
    , mQuadratic(0.032f)                // 減衰：二次項
    , mRadius(28.0f)                    // 影響半径（最適化/見た目用の閾値）
    , mIsEnabled(true)                  // 有効/無効フラグ
{
    //------------------------------------------------------------------
    // LightingManager に自動登録
    //
    // ここで「自分は点光源である」ことを LightingManager に通知する。
    // LightingManager はレンダリング時に、登録済みライトを列挙して
    // シェーダの Uniform へ反映する想定。
    //------------------------------------------------------------------
    if (auto* app = owner->GetApp())
    {
        if (auto* renderer = app->GetRenderer())
        {
            // Renderer が LightingManager を所有している前提（shared_ptr）
            auto lm = renderer->GetLightingManager();
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
    //------------------------------------------------------------------
    // LightingManager から自動解除
    //
    // Destroy 時に登録を外しておかないと、LightingManager が
    // ダングリングポインタを抱える危険があるため必ず解除する。
    //------------------------------------------------------------------
    if (mLighting)
    {
        mLighting->UnregisterPointLight(this);
    }
}

void PointLightComponent::Update(float /*deltaTime*/)
{
    //------------------------------------------------------------------
    // 演出拡張用（今は未使用）
    //
    // 例：
    // ・炎のゆらぎ：intensity をノイズで変化
    // ・点滅：sin 波で ON/OFF or intensity を変化
    // ・寿命：一定時間で Disable → Unregister など
    //------------------------------------------------------------------
}

Vector3 PointLightComponent::GetPosition() const
{
    //------------------------------------------------------------------
    // ライト位置は Actor のワールド座標に追従させる。
    // WorldTransform の平行移動成分を返す実装。
    //
    // ※ WorldTransform が無い/未実装の場合は GetPosition() で暫定可。
    //------------------------------------------------------------------
    return GetOwner()->GetWorldTransform().GetTranslation();
    // return GetOwner()->GetPosition(); // fallback
}

} // namespace toy
