#include "UI/UIScreen.h"
#include "UI/UIStack.h"

#include "Engine/Core/Application.h"
#include "Render/IRenderer.h"

namespace toy {

UIScreen::UIScreen(Application* app)
    : Actor(app)
{
    SetActorID("UIScreen");
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

    // z（描画の前後）は Actor 側の値を維持する
    SetPosition(Vector3(mRect.pos.x, mRect.pos.y, GetPosition().z));

    OnLayoutChanged(mRect);
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
