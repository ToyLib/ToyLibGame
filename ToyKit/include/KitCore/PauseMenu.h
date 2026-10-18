#pragma once

#include "Engine/Runtime/InputSystem.h"
#include "UI/ChoiceListActor.h"

#include <functional>
#include <string>
#include <vector>

namespace toy::kit {

//============================================================
// PauseMenuItem
//  - ポーズメニューの1項目（ゲーム全体 / Scene から追加する）
//  - closeMenu = false にすると、メニューを開いたまま onSelect を呼ぶ
//    （確認ダイアログを重ねて、「いいえ」ならメニューに戻る、等）
//============================================================
struct PauseMenuItem
{
    std::string           label;
    std::function<void()> onSelect;
    bool                  closeMenu { true };
};

//============================================================
// PauseMenuDesc
//  - GameFlow::EnablePauseMenu() に渡す設定
//  - 項目は「再開 → Scene の項目 → items（ゲーム全体）」の順に並ぶ
//============================================================
struct PauseMenuDesc
{
    // ゲーム全体の項目（どの Scene でも出る）
    std::vector<PauseMenuItem> items;

    // 先頭の「再開」の表示名
    std::string resumeText { "再開" };

    // 開閉ボタン（開いている間にもう一度押すと再開）
    toy::GameButton toggleButton { toy::GameButton::Start };

    // 見た目・配置（pauseGame は常に true として扱う）
    toy::ChoiceListActor::Desc list { DefaultList() };

    static toy::ChoiceListActor::Desc DefaultList()
    {
        toy::ChoiceListActor::Desc d;
        d.layout.anchor = toy::UIAnchor::Center;
        d.layout.size   = Vector2(280.0f, 0.0f);   // 高さは項目数から自動
        d.frame.bgAlpha = 0.85f;
        return d;
    }
};

} // namespace toy::kit
