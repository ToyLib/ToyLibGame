#pragma once

#include "Engine/Core/Actor.h"
#include "UI/UILayout.h"

namespace toy {

//==============================================================
// UIScreen
//------------------------------------------------------------------------------
// UIStack に積まれる UI の基底 Actor。
// （メッセージボックス / メニュー / 確認ダイアログ / HUD など）
//
// ・入力は Application の通常 Actor ループではなく UIStack 経由で届く
//   → スタックに積まれている間だけ ActorInput() が呼ばれる
// ・Modal（既定 true）：自分より下の UI とゲーム側に入力を渡さない
//   → 会話中にプレイヤーが動く / 閉じた A がシーンにも届く、を防ぐ
// ・非 Modal（HUD 等）：入力を受け取りつつ、下にもそのまま流す
//
// 使い方
// ・派生クラスで開くときに PushToUIStack()、閉じるときに RemoveFromUIStack()
// ・破棄時はデストラクタで自動的にスタックから外れる
//
// レイアウト
// ・SetLayout() で画面（UI ルート矩形）に対する配置を指定する
//   → 解決した矩形の左上が Actor の位置になる
// ・中の部品は GetRect() を基準に、ローカル座標（SetOffset）で配置する
//   → OnLayoutChanged() を override して部品のサイズ・位置を合わせる
// ・ウィンドウサイズ変更時は Application が ApplyLayout() を呼び直す
//==============================================================
class UIScreen : public Actor
{
public:
    explicit UIScreen(class Application* app);
    ~UIScreen() override;

    //--------------------------------------------------------------------------
    // Stack
    //--------------------------------------------------------------------------
    // 既に積まれていれば最前面へ移動する
    void PushToUIStack();
    void RemoveFromUIStack();
    bool IsOnUIStack() const;

    //--------------------------------------------------------------------------
    // Modal
    //--------------------------------------------------------------------------
    void SetModal(bool modal) { mModal = modal; }
    bool IsModal() const { return mModal; }

    //--------------------------------------------------------------------------
    // Focus（スタック最前面になった / でなくなった）
    //--------------------------------------------------------------------------
    bool HasFocus() const { return mHasFocus; }

    virtual void OnFocusGained() {}
    virtual void OnFocusLost() {}

    //--------------------------------------------------------------------------
    // Layout
    //--------------------------------------------------------------------------
    void SetLayout(const UILayout& layout);
    const UILayout& GetLayout() const { return mLayout; }

    // 解決済みの矩形（UI 論理座標、左上 = Actor の位置）
    const UIRect& GetRect() const { return mRect; }

    // 自分のサイズを基準にしたローカル矩形（左上が原点）
    UIRect GetLocalRect() const { return UIRect{ Vector2::Zero, mRect.size }; }

    // ルート矩形からレイアウトを解決し直す
    void ApplyLayout();

protected:
    // レイアウトが解決されたときに呼ばれる（中の部品の配置はここで行う）
    virtual void OnLayoutChanged(const UIRect& rect) {}

private:
    friend class UIStack;
    void SetFocus(bool focus);

    bool mModal    { true };
    bool mHasFocus { false };

    UILayout mLayout;
    UIRect   mRect;
};

} // namespace toy
