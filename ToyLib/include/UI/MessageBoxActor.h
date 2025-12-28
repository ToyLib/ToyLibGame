#pragma once

#include "Engine/Core/Actor.h"
#include "Utils/MathUtil.h"

#include <functional>
#include <memory>
#include <string>

namespace toy {

class SpriteComponent;
class MessageTextComponent;
class TextFont;

//==============================================================
// MessageBoxActor
//------------------------------------------------------------------------------
// ・ゲーム中に表示する「メッセージボックス UI」を Actor として提供する。
// ・内部に背景(SpriteComponent)と本文(MessageTextComponent)を内包して組み立てる。
// ・Scene 側からは Open()/Close() と、レイアウト用 Setter を呼ぶだけで利用できる。
// ・入力は ActorInput() で処理し、A でページ送り / 末尾で閉じる、B で即閉じなどの
//   “基本操作” をこの Actor に集約する。
//
// 設計意図
// ・MessageTextComponent は「テキストの折り返し/改ページ/文字送り」だけを担当。
// ・MessageBoxActor は「UIの塊としての振る舞い（表示/非表示/入力/背景）」を担当。
// ・Actor の親子を使わなくても、SpriteComponent 側の Offset を使えば
//   “同一Actor内で部品を組む” ことができる。
//
// 使い方例（Scene側）
//   auto box = std::make_unique<MessageBoxActor>(app);
//   box->SetBoxPosition({40, 520, 0});
//   box->SetBoxSize({640, 160});
//   box->SetPadding({16, 16});
//   box->Open("こんにちは。\nAで進む / Bで閉じる");
//   AddActor(std::move(box));
//
// 座標/サイズの考え方
// ・SetBoxPosition(): ボックスの基準点（背景スプライトの描画基準）を決める。
// ・SetBoxSize(): 背景のサイズを決める。
// ・SetPadding(): 背景内側の余白。本文をこの分だけ内側に寄せる。
//   （内部では mText->SetOffset(padding) を行い、文字開始位置を調整する）
//
// 注意
// ・本 Actor は UI 表示を目的としているため、VisualLayer::UI 前提。
// ・フォント未指定の場合は、内部で “デフォルトフォント” を取得して使用する想定。
//==============================================================
class MessageBoxActor : public Actor
{
public:
    //--------------------------------------------------------------------------
    // ctor
    //--------------------------------------------------------------------------
    // ・font を渡すとそのフォントで表示する（shared_ptr 共有）。
    // ・font == nullptr の場合はデフォルトフォントを自動取得して使用する。
    //--------------------------------------------------------------------------
    MessageBoxActor(class Application* app,
                    std::shared_ptr<TextFont> font = nullptr);

    ~MessageBoxActor() override = default;

    //--------------------------------------------------------------------------
    // Actor hooks
    //--------------------------------------------------------------------------
    // ・UpdateActor: 文字送り等の “時間経過で進む演出” を回す用途。
    // ・ActorInput : A/B などの入力でページ送り・閉じるを処理。
    //--------------------------------------------------------------------------
    void UpdateActor(float dt) override;
    void ActorInput(const struct InputState& state) override;

    //--------------------------------------------------------------------------
    // Open / Close
    //--------------------------------------------------------------------------
    // ・Open: メッセージを表示し、必要なら onClose を登録。
    // ・Close: 表示を閉じ、登録されていれば onClose を呼ぶ。
    //
    // ※Open は “表示開始” の入口。
    //   内部では MessageTextComponent にテキストを渡し、
    //   折り返し/改ページ/文字送りの初期化を行う想定。
    //--------------------------------------------------------------------------
    void Open(const std::string& text,
              std::function<void()> onClose = nullptr);

    void Close();

    // 現在表示中か
    bool IsOpen() const { return mOpen; }

    //--------------------------------------------------------------------------
    // Enabled（表示/非表示）
    //--------------------------------------------------------------------------
    // ・Open/Close とは独立して、強制的に表示を隠したい場合に使う。
    // ・例：デバッグ、フェード中、一時的に UI を消す等。
    //--------------------------------------------------------------------------
    void SetEnabled(bool enabled);
    bool IsEnabled() const { return mEnabled; }

    //--------------------------------------------------------------------------
    // Layout setters（外部からレイアウトを注入する）
    //--------------------------------------------------------------------------
    // ・Scene 側で “どこに/どのサイズで/どの余白で” を決められるようにする。
    // ・各 Setter は内部状態を更新し、必要なら ApplyLayout() で反映する。
    //--------------------------------------------------------------------------
    void SetBoxPosition(const Vector3& pos);   // Actor位置＝UI基準点
    void SetBoxSize(const Vector2& size);      // 背景サイズ
    void SetPadding(const Vector2& padding);   // 余白（本文開始位置/折り返し幅に影響）

private:
    //--------------------------------------------------------------------------
    // ApplyLayout
    //--------------------------------------------------------------------------
    // ・mBoxSize / mPadding をもとに、背景と本文の見た目を反映する。
    // ・背景: SpriteComponent の scale（幅/高さ）を設定
    // ・本文: MessageTextComponent の text box size / padding を設定
    //         さらに “表示開始位置” として Offset を padding 分だけ与える
    //
    // ※SetBoxSize/SetPadding を呼んだ後に反映したい場合に内部から呼ぶ。
    //--------------------------------------------------------------------------
    void ApplyLayout();

private:
    // 背景スプライト（半透明の矩形など）
    SpriteComponent*      mBg;

    // 本文（折り返し/改ページ/文字送りを持つ TextSprite 派生）
    MessageTextComponent* mText;

    // 状態
    bool    mOpen;      // Open中か（会話UIが優先される状態）
    bool    mEnabled;   // 描画ON/OFF（表示の強制切り替え）

    // レイアウト（外部から注入される想定の値）
    Vector2 mBoxSize; // 背景サイズ
    Vector2 mPadding;   // 余白
    
    // Close時に呼ぶコールバック（Scene側の “操作再開” などに使う）
    std::function<void()> mOnClose;
};

} // namespace toy
