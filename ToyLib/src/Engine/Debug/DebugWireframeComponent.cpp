#include "Engine/Debug/DebugWireframeComponent.h"
#include "Engine/Core/Application.h"
#include "Engine/Runtime/InputSystem.h"
#include "Engine/Core/Actor.h"

namespace toy {

//======================================================================
// コンストラクタ
//----------------------------------------------------------------------
// DebugWireframeComponent は WireframeComponent をそのまま拡張し、
// 描画可否の制御を Application / Renderer 側のデバッグ設定に委ねる。
// ここでは特別な初期化は行わず、基底クラスの設定をそのまま使用する。
//======================================================================
DebugWireframeComponent::DebugWireframeComponent(
    Actor* owner,
    int drawOrder,
    VisualLayer layer
)
    : WireframeComponent(owner, drawOrder, layer)
{
}

//======================================================================
// Draw
//----------------------------------------------------------------------
// デバッグ用ワイヤーフレーム描画。
// Renderer が管理している「デバッグワイヤーフレーム表示フラグ」
// が ON のときのみ描画を行う。
//
// ・ゲーム本編の描画ロジックとは分離
// ・キー入力やメニュー操作による ON / OFF 切り替えを想定
// ・OFF の場合は早期リターンして無駄な描画を防ぐ
//======================================================================
void DebugWireframeComponent::Draw()
{
    // Application 取得（安全ガード）
    auto* app = GetOwner()->GetApp();
    if (!app)
    {
        return;
    }

    // Renderer 側のデバッグ表示フラグを確認
    if (!app->GetRenderer()->GetDebugWireVisible())
    {
        return;
    }

    // 実際のワイヤーフレーム描画は基底クラスに委譲
    WireframeComponent::Draw();
}

} // namespace toy
