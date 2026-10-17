#pragma once

#include <vector>

namespace toy {

class UIScreen;

//==============================================================
// UIStack
//------------------------------------------------------------------------------
// UIScreen を積み重ねて管理し、入力の配送とフォーカスを制御する。
// Application が 1 つ保持する（GetUIStack()）。
//
// 入力の流れ（Application::ProcessInput）
//   1. UIStack::ProcessInput : 最前面から順に UIScreen へ配送
//                              Modal な画面に当たったらそこで止める
//   2. 通常 Actor            : UI が入力を占有していなければ配送
//                              （Actor::SetAlwaysReceiveInput(true) は常に配送）
//   3. Application::ProcessInput(state) フック（ゲーム側）
//                              占有中かどうかは IsInputCaptured() で判断する
//
// ・UIScreen の寿命は Application（Actor 管理）が持つ。ここは参照のみ
//==============================================================
class UIStack
{
public:
    UIStack() = default;
    ~UIStack();

    UIStack(const UIStack&) = delete;
    UIStack& operator=(const UIStack&) = delete;

    //--------------------------------------------------------------------------
    // Stack 操作
    //--------------------------------------------------------------------------
    // 既に積まれていれば最前面へ移動する
    void Push(UIScreen* screen);
    void Remove(UIScreen* screen);
    void Clear();

    bool Contains(const UIScreen* screen) const;
    bool IsEmpty() const { return mScreens.empty(); }

    // 最前面（Dead は除く）
    UIScreen* GetTop() const;

    //--------------------------------------------------------------------------
    // Input
    //--------------------------------------------------------------------------
    // UIScreen へ入力を配送する。戻り値：このフレームの入力を UI が占有したか
    bool ProcessInput(const struct InputState& state);

    // 直近の ProcessInput で UI が入力を占有したか
    bool IsInputCaptured() const { return mInputCaptured; }

private:
    friend class UIScreen;

    // 破棄中の UIScreen から呼ぶ（仮想関数を呼ばない）
    void RemoveWithoutNotify(UIScreen* screen);

    bool HasActiveModal() const;
    void RefreshFocus();

    std::vector<UIScreen*> mScreens;       // 末尾が最前面
    UIScreen* mFocused     { nullptr };
    bool mInputCaptured    { false };
};

} // namespace toy
