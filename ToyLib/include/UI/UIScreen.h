#pragma once

#include "Engine/Core/Actor.h"
#include "UI/UILayout.h"
#include "Utils/Easing.h"

#include <unordered_map>

namespace toy {

//==============================================================
// UITransition
//  - UIScreen の開閉アニメーション設定
//  - 開く：fade/slide の「開始状態」→ 通常状態
//    閉じる：通常状態 → 「開始状態」（同じカーブを逆再生）
//  - duration <= 0 でアニメーションなし（即時）
//==============================================================
struct UITransition
{
    float   duration  { 0.15f };
    Ease    ease      { Ease::OutCubic };
    float   fadeFrom  { 0.0f };                  // 開始時のアルファ倍率
    Vector2 slideFrom { Vector2(0.0f, 16.0f) };  // 開始時のずれ（px、+Y 下）
};

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
// ・PausesGame（既定 false）：積まれている間、ゲームの更新を止める
//
// 使い方
// ・派生クラスで開くときに PushToUIStack()、閉じるときに RemoveFromUIStack()
// ・破棄時はデストラクタで自動的にスタックから外れる
// ・積んだ順に描画順が底上げされる（後から開いた画面が必ず手前に描かれる）
//
// レイアウト
// ・SetLayout() で画面（UI ルート矩形）に対する配置を指定する
//   → 解決した矩形の左上が Actor の位置になる
// ・中の部品は GetRect() を基準に、ローカル座標（SetOffset）で配置する
//   → OnLayoutChanged() を override して部品のサイズ・位置を合わせる
// ・ウィンドウサイズ変更時は Application が ApplyLayout() を呼び直す
//
// 開閉アニメーション
// ・派生クラスの Open で部品を表示してから BeginShow()、
//   Close で BeginHide() を呼ぶ
// ・閉じ終わると OnHidden() が呼ばれる → 派生で部品を非表示にする
// ・閉じる途中に BeginShow() されたら、その位置から開き直す
// ・アルファは各 VisualComponent の倍率（SetAlphaMultiplier）で掛けるので、
//   部品ごとのアルファ設定はそのまま保たれる
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
    // Pause
    //--------------------------------------------------------------------------
    // true：スタックに積まれている間、ゲーム（Scene の Update と通常 Actor の更新）を止める
    // ・UIScreen 自身と SetUpdateWhilePaused(true) の Actor は動き続ける
    // ・閉じる（スタックから外れる）とすぐ再開する。閉じるアニメーション中はもう動いている
    void SetPausesGame(bool pause) { mPausesGame = pause; }
    bool PausesGame() const { return mPausesGame; }

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

    //--------------------------------------------------------------------------
    // Transition
    //--------------------------------------------------------------------------
    void SetTransition(const UITransition& t) { mTransition = t; }
    const UITransition& GetTransition() const { return mTransition; }

    // 開閉アニメーション中か
    bool IsTransitioning() const { return mTransitionDir != 0; }

protected:
    // レイアウトが解決されたときに呼ばれる（中の部品の配置はここで行う）
    virtual void OnLayoutChanged(const UIRect& rect) {}

    // 開く / 閉じるアニメーションを開始する（派生の Open / Close から呼ぶ）
    void BeginShow();
    void BeginHide();

    // 閉じるアニメーションが終わったときに呼ばれる
    virtual void OnHidden() {}

private:
    friend class UIStack;
    friend class UIScreenTransitionDriver;
    void SetFocus(bool focus);

    // スタック上の段数に応じて、全 VisualComponent の描画順を底上げする
    void ApplyStackDepth(int depth);

    void TickTransition(float dt);
    void ApplyTransitionVisual();
    void UpdateScreenPosition();

    bool mModal    { true };
    bool mPausesGame { false };
    bool mHasFocus { false };

    UILayout mLayout;
    UIRect   mRect;

    UITransition mTransition;
    float mTransitionT   { 1.0f };   // 0 = 閉じた状態 / 1 = 開いた状態
    int   mTransitionDir { 0 };      // +1 開く途中 / -1 閉じる途中 / 0 停止
    bool  mShown         { false };  // 表示中（開く途中・閉じる途中を含む）

    // 各 VisualComponent の元の描画順（底上げ前）
    std::unordered_map<class VisualComponent*, int> mBaseDrawOrder;
};

} // namespace toy
