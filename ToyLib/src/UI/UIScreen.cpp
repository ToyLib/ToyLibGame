#include "UI/UIScreen.h"
#include "UI/UIStack.h"

#include "Engine/Core/Application.h"
#include "Engine/Core/Component.h"
#include "Graphics/VisualComponent.h"
#include "Render/IRenderer.h"

namespace toy {

//==============================================================
// UIScreenTransitionDriver
//  - UIScreen の開閉アニメーションを毎フレーム進める内部コンポーネント
//  - 派生クラスが UpdateActor で基底を呼ばなくても動くようにするため
//==============================================================
class UIScreenTransitionDriver : public Component
{
public:
    explicit UIScreenTransitionDriver(Actor* owner)
        : Component(owner, 0)
    {
    }

    void Update(float dt) override
    {
        static_cast<UIScreen*>(GetOwner())->TickTransition(dt);
    }
};

UIScreen::UIScreen(Application* app)
    : Actor(app)
{
    SetActorID("UIScreen");
    CreateComponent<UIScreenTransitionDriver>();
}

UIScreen::~UIScreen()
{
    // 破棄中なので派生の OnFocusLost は呼べない（呼ばない）
    if (auto* stack = GetApp()->GetUIStack())
    {
        stack->RemoveWithoutNotify(this);
    }
}

void UIScreen::PushToUIStack()
{
    GetApp()->GetUIStack()->Push(this);
}

void UIScreen::RemoveFromUIStack()
{
    GetApp()->GetUIStack()->Remove(this);
}

bool UIScreen::IsOnUIStack() const
{
    return const_cast<UIScreen*>(this)->GetApp()->GetUIStack()->Contains(this);
}

void UIScreen::SetLayout(const UILayout& layout)
{
    mLayout = layout;
    ApplyLayout();
}

void UIScreen::ApplyLayout()
{
    const UIRect root = GetUIRootRect(GetApp()->GetRenderer());
    mRect = mLayout.Resolve(root);

    UpdateScreenPosition();
    OnLayoutChanged(mRect);
}

//=============================================================================
// Transition
//=============================================================================

void UIScreen::BeginShow()
{
    if (mTransition.duration <= 0.0f)
    {
        mTransitionT   = 1.0f;
        mTransitionDir = 0;
    }
    else if (!mShown)
    {
        // 閉じきった状態から：最初から再生
        mTransitionT   = 0.0f;
        mTransitionDir = +1;
    }
    else if (mTransitionDir < 0)
    {
        // 閉じる途中から：その位置から開き直す
        mTransitionDir = +1;
    }
    // 開いている / 開く途中なら、そのまま続ける

    mShown = true;

    // 最初のフレームから開始状態で描かれるよう、すぐ反映する
    ApplyTransitionVisual();
}

void UIScreen::BeginHide()
{
    if (mTransition.duration <= 0.0f)
    {
        mTransitionT   = 0.0f;
        mTransitionDir = 0;
        mShown         = false;
        ApplyTransitionVisual();
        OnHidden();
        return;
    }

    if (!mShown) return;
    mTransitionDir = -1;
}

void UIScreen::TickTransition(float dt)
{
    if (mTransitionDir == 0) return;

    mTransitionT += static_cast<float>(mTransitionDir) * dt / mTransition.duration;

    if (mTransitionT >= 1.0f)
    {
        mTransitionT   = 1.0f;
        mTransitionDir = 0;
        ApplyTransitionVisual();
    }
    else if (mTransitionT <= 0.0f)
    {
        mTransitionT   = 0.0f;
        mTransitionDir = 0;
        mShown         = false;
        ApplyTransitionVisual();
        OnHidden();
    }
    else
    {
        ApplyTransitionVisual();
    }
}

void UIScreen::ApplyTransitionVisual()
{
    // OutBack などは 1 を超えるので、アルファだけ 0..1 に収める
    const float e = ApplyEase(mTransition.ease, mTransitionT);
    const float alpha = Math::Clamp(mTransition.fadeFrom + (1.0f - mTransition.fadeFrom) * e, 0.0f, 1.0f);

    // 派生が後から作った部品（項目テキスト等）にも効くよう、毎回まとめて設定する
    for (auto* vc : GetAllComponents<VisualComponent>())
    {
        vc->SetAlphaMultiplier(alpha);
    }

    UpdateScreenPosition();
}

void UIScreen::UpdateScreenPosition()
{
    const float e = ApplyEase(mTransition.ease, mTransitionT);
    const Vector2 slide = mTransition.slideFrom * (1.0f - e);

    // z（描画の前後）は Actor 側の値を維持する
    SetPosition(Vector3(mRect.pos.x + slide.x, mRect.pos.y + slide.y, GetPosition().z));
}

void UIScreen::SetFocus(bool focus)
{
    if (mHasFocus == focus) return;

    mHasFocus = focus;
    if (focus)
    {
        OnFocusGained();
    }
    else
    {
        OnFocusLost();
    }
}

} // namespace toy
