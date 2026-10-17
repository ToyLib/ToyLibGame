#include "UI/UIStack.h"
#include "UI/UIScreen.h"

#include <algorithm>

namespace toy {

UIStack::~UIStack()
{
    mScreens.clear();
    mFocused = nullptr;
}

//=============================================================================
// Stack 操作
//=============================================================================

void UIStack::Push(UIScreen* screen)
{
    if (!screen) return;

    // 既にあれば一度外して最前面へ
    mScreens.erase(std::remove(mScreens.begin(), mScreens.end(), screen), mScreens.end());
    mScreens.push_back(screen);

    RefreshFocus();
}

void UIStack::Remove(UIScreen* screen)
{
    if (!Contains(screen)) return;

    mScreens.erase(std::remove(mScreens.begin(), mScreens.end(), screen), mScreens.end());

    // スタックから外れた画面はフォーカスを失う
    if (mFocused == screen)
    {
        mFocused = nullptr;
        screen->SetFocus(false);
    }

    RefreshFocus();
}

void UIStack::RemoveWithoutNotify(UIScreen* screen)
{
    mScreens.erase(std::remove(mScreens.begin(), mScreens.end(), screen), mScreens.end());

    if (mFocused == screen)
    {
        mFocused = nullptr;
    }

    RefreshFocus();
}

void UIStack::Clear()
{
    mScreens.clear();

    if (mFocused)
    {
        UIScreen* prev = mFocused;
        mFocused = nullptr;
        prev->SetFocus(false);
    }
}

bool UIStack::Contains(const UIScreen* screen) const
{
    return std::find(mScreens.begin(), mScreens.end(), screen) != mScreens.end();
}

UIScreen* UIStack::GetTop() const
{
    for (auto it = mScreens.rbegin(); it != mScreens.rend(); ++it)
    {
        if ((*it)->GetState() != Actor::State::Dead)
        {
            return *it;
        }
    }
    return nullptr;
}

//=============================================================================
// Input
//=============================================================================

bool UIStack::ProcessInput(const InputState& state)
{
    mInputCaptured = false;

    // 配送中に Push / Remove されても安全なようにスナップショットで回す
    // ・このフレームに新しく積まれた画面には、次フレームから入力が届く
    // ・Modal な画面が入力で自分を閉じても、このフレームの入力は占有したまま
    const auto snapshot = mScreens;

    for (auto it = snapshot.rbegin(); it != snapshot.rend(); ++it)
    {
        UIScreen* screen = *it;

        // 他の画面の入力処理で外された / 破棄予約済み
        if (!Contains(screen) || screen->GetState() == Actor::State::Dead)
        {
            continue;
        }

        const bool modal = screen->IsModal();
        screen->ProcessInput(state);

        if (modal)
        {
            mInputCaptured = true;
            break;
        }
    }

    // 非 Modal な画面が入力で Modal な画面を開いた場合も、同じ入力を下に流さない
    if (!mInputCaptured && HasActiveModal())
    {
        mInputCaptured = true;
    }

    return mInputCaptured;
}

//=============================================================================
// Pause
//=============================================================================

bool UIStack::IsGamePaused() const
{
    return std::any_of(mScreens.begin(), mScreens.end(),
                       [](const UIScreen* s)
                       {
                           return s->PausesGame() && s->GetState() != Actor::State::Dead;
                       });
}

//=============================================================================
// Internal
//=============================================================================

bool UIStack::HasActiveModal() const
{
    return std::any_of(mScreens.begin(), mScreens.end(),
                       [](const UIScreen* s)
                       {
                           return s->IsModal() && s->GetState() != Actor::State::Dead;
                       });
}

void UIStack::RefreshFocus()
{
    UIScreen* top = GetTop();
    if (top == mFocused) return;

    UIScreen* prev = mFocused;
    mFocused = top;

    if (prev)
    {
        prev->SetFocus(false);
    }
    if (top)
    {
        top->SetFocus(true);
    }
}

} // namespace toy
