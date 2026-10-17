#include "UI/MessageBoxActor.h"

#include "Engine/Core/Application.h"
#include "Engine/Runtime/InputSystem.h"
#include "Asset/AssetManager.h"
#include "Asset/Font/TextFont.h"

#include "Asset/Material/Texture.h"

#include "Graphics/Sprite/TextSpriteComponent.h"
#include "UI/MessageTextComponent.h"
#include "UI/ChoiceListActor.h"
#include "Render/IRenderer.h"

#include <cmath>
#include <iostream>

namespace toy {

MessageBoxActor::MessageBoxActor(Application* app, const Desc& desc)
    : UIScreen(app)
    , mDesc(desc)
{
    SetActorID("MessageBox");

    // 背景
    mFrame.Create(this, 900, 901);
    mFrame.ApplyStyle(mDesc.frame);

    SetTransition(mDesc.transition);
    SetPausesGame(mDesc.pauseGame);

    // 本文
    mText = CreateComponent<MessageTextComponent>(9001, VisualLayer::UI);

    // フォント（未指定ならデフォルト）
    if (mDesc.font)
    {
        mText->SetFont(mDesc.font);
    }
    else
    {
        auto f = app->GetSysAssetManager()->GetFont(mDesc.defaultFontPath, mDesc.defaultFontSize);
        mText->SetFont(f);
        mDesc.font = f; // 次回以降のために保持（任意）
    }

    // 色・行間
    mText->SetColor(mDesc.textColor);
    mText->SetLineGapPx(mDesc.lineGapPx);

    // 送りインジケータ（本文と同じフォント・色）
    mIndicator = CreateComponent<TextSpriteComponent>(9002, VisualLayer::UI);
    ApplyIndicatorStyle();

    // 初期レイアウト反映（位置・背景サイズ・折り返し幅）
    SetLayout(mDesc.layout);

    // 初期は非表示
    SetEnabled(false);
}

void MessageBoxActor::ApplyDesc(const Desc& desc)
{
    mDesc = desc;

    // drawOrder/layer変更は再生成が必要なのでここでは触らない（必要なら作り直し関数を用意）
    mFrame.ApplyStyle(mDesc.frame);
    SetTransition(mDesc.transition);
    SetPausesGame(mDesc.pauseGame);

    if (mText)
    {
        if (mDesc.font)
        {
            mText->SetFont(mDesc.font);
        }
        else
        {
            auto f = GetApp()->GetAssetManager()->GetFont(mDesc.defaultFontPath, mDesc.defaultFontSize);
            mText->SetFont(f);
            mDesc.font = f;
        }

        mText->SetColor(mDesc.textColor);
        mText->SetLineGapPx(mDesc.lineGapPx);
    }

    ApplyIndicatorStyle();
    SetLayout(mDesc.layout);
}

void MessageBoxActor::SetEnabled(bool enabled)
{
    mEnabled = enabled;
    mFrame.SetVisible(enabled);
    if (mText) mText->SetVisible(enabled);
    if (mIndicator) mIndicator->SetVisible(false);   // 表示は UpdateIndicator が決める
}

void MessageBoxActor::Open(const std::string& text, std::function<void()> onClose)
{
    mOnClose = std::move(onClose);
    mOpen = true;

    // 通常メッセージとして開く（前回の選択肢は捨てる）
    mChoices.clear();
    mOnDecide        = nullptr;
    mChoiceShown     = false;
    mChoiceCancelled = false;

    SetEnabled(true);
    PushToUIStack();

    // 直前にDescが変わってても正しく反映
    ApplyLayout();

    mText->SetMessage(text);

    BeginShow();
}

void MessageBoxActor::OpenWithChoices(const std::string& text,
                                      const std::vector<std::string>& choices,
                                      DecideFn onDecide)
{
    if (!mChoiceList)
    {
        std::cerr << "[MessageBox] OpenWithChoices: ChoiceList is not attached. Opened as a plain message.\n";
    }

    Open(text);

    if (mChoiceList)
    {
        mChoices  = choices;
        mOnDecide = std::move(onDecide);
    }
}

void MessageBoxActor::Close()
{
    if (!mOpen) return;

    // 外から閉じられた場合も選択肢リストを片付ける
    if (mChoiceShown && mChoiceList)
    {
        mChoiceShown = false;
        mChoiceList->Close();
    }
    mChoices.clear();
    mOnDecide = nullptr;

    mOpen = false;
    RemoveFromUIStack();   // 入力はすぐ手放す（見た目はフェードアウト後に消える）
    BeginHide();

    // コールバック内で Open し直すと mOnClose が差し替わるので、退避してから呼ぶ
    auto onClose = std::move(mOnClose);
    mOnClose = nullptr;
    if (onClose)
    {
        onClose();
    }
}

void MessageBoxActor::OnHidden()
{
    // 閉じるアニメーションが終わったら部品を隠す
    if (!mOpen)
    {
        SetEnabled(false);
    }
}

void MessageBoxActor::ActorInput(const InputState& state)
{
    if (!mOpen || !mEnabled) return;

    if (state.IsButtonPressed(GameButton::A))
    {
        if (mText->IsTyping())
        {
            mText->FinishTyping();
        }
        else if (mText->HasNextPage())
        {
            mText->NextPage();
        }
        else if (HasPendingChoices())
        {
            // キャンセル後の出し直し
            ShowChoices();
        }
        else
        {
            Close();
        }
    }

    if (mDesc.enableBToClose && !HasPendingChoices() && state.IsButtonPressed(GameButton::B))
    {
        Close();
    }
}

void MessageBoxActor::UpdateActor(float dt)
{
    if (!mOpen || !mEnabled) return;

    // 最後のページを出し切ったら選択肢を出す
    if (HasPendingChoices() && !mChoiceShown && !mChoiceCancelled && IsOnLastPageReady())
    {
        ShowChoices();
    }

    UpdateIndicator(dt);
}

//------------------------------------------------------------------------------
// 選択肢
//------------------------------------------------------------------------------
bool MessageBoxActor::IsOnLastPageReady() const
{
    return !mText->IsTyping() && !mText->HasNextPage();
}

void MessageBoxActor::ShowChoices()
{
    if (!mChoiceList || mChoices.empty()) return;

    // メッセージの右上に、右端をそろえて置く
    const UIRect root = GetUIRootRect(GetApp()->GetRenderer());
    const UIRect& box = GetRect();

    UILayout layout = mChoiceList->GetListLayout();   // 幅などはリスト側の設定を使う
    layout.anchor = UIAnchor::TopLeft;
    layout.pivot  = UIAnchor::BottomRight;
    layout.offset = Vector2(box.Right() - root.Left(), box.Top() - mDesc.choiceGap - root.Top());
    layout.stretch = UIStretch::None;
    mChoiceList->SetListLayout(layout);

    mChoiceShown     = true;
    mChoiceCancelled = false;

    mChoiceList->Open(mChoices,
                      [this](int index)
                      {
                          // 閉じてから通知（onDecide の中で Open し直せるように）
                          auto onDecide = std::move(mOnDecide);
                          mChoiceShown = false;
                          Close();
                          if (onDecide)
                          {
                              onDecide(index);
                          }
                      },
                      [this]()
                      {
                          // キャンセル：メッセージに戻る（A でもう一度出せる）
                          mChoiceShown     = false;
                          mChoiceCancelled = true;
                      });
}

//------------------------------------------------------------------------------
// 送りインジケータ
//  - 文字送り中は隠し、ページを出し切ったら右下で上下に揺らす
//------------------------------------------------------------------------------
void MessageBoxActor::ApplyIndicatorStyle()
{
    if (!mIndicator) return;

    mIndicator->SetFont(mDesc.font);
    mIndicator->SetColor(mDesc.textColor);
    mIndicator->SetText(mDesc.indicatorText);
}

void MessageBoxActor::UpdateIndicator(float dt)
{
    if (!mIndicator) return;

    // 選択肢を出している間（最後のページ）は ▼ を出さない
    const bool waitingChoice = mChoiceShown && !mText->HasNextPage();
    const bool show = mDesc.showIndicator && !mText->IsTyping() && !waitingChoice;
    if (!show)
    {
        mIndicator->SetVisible(false);
        mIndicatorTime = 0.0f;   // 出るたびに同じ位相から揺らす
        return;
    }

    mIndicatorTime += dt;
    const float bob = std::sin(mIndicatorTime * mDesc.indicatorBobSpeed) * mDesc.indicatorBobPx;

    mIndicator->SetOffset(mIndicatorBase + Vector3(0.0f, bob, 0.0f));
    mIndicator->SetVisible(true);
}

void MessageBoxActor::OnLayoutChanged(const UIRect& rect)
{
    if (!mText) return;

    // 背景サイズ（Actor 位置 = 矩形の左上）
    mFrame.SetRect(GetLocalRect());

    // 本文の折り返し条件
    mText->SetTextBoxSize(rect.size);
    mText->SetPadding(mDesc.padding);

    // 本文開始位置（padding分だけ内側）
    mText->SetOffset(Vector3(mDesc.padding.x, mDesc.padding.y, 0.0f));

    // インジケータ：padding の内側、右下
    if (mIndicator)
    {
        Vector2 indSize = Vector2::Zero;
        if (auto tex = mIndicator->GetTexture())
        {
            indSize = Vector2(static_cast<float>(tex->GetWidth()), static_cast<float>(tex->GetHeight()));
        }
        mIndicatorBase = Vector3(rect.size.x - mDesc.padding.x - indSize.x,
                                 rect.size.y - mDesc.padding.y - indSize.y,
                                 0.0f);
    }
}

} // namespace toy
