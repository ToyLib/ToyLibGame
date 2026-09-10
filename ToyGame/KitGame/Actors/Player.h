#pragma once

#include "ToyKit.h"

//=============================================================================
// Player
//  旧 PlayerActor（toy::Actor 継承、フィールド移動＋ロックオン戦闘）を
//  新方針（toy::kit::Humanoid を内包する Game Logic クラス）で置き換えた版。
//
//  移動/カメラ切換え/ロックオンの選択・解除ロジックそのものは Humanoid が持つ。
//  Player はどの入力でどのアニメーションを再生するか（ゲーム固有の意味づけ）
//  だけを担当する。
//=============================================================================
class Player
{
public:
    explicit Player(toy::Application* app);

    void ProcessInput(const toy::InputState& state);
    void Update(float deltaTime);

    const Vector3&    GetPosition() const { return mBody.GetPosition(); }
    const Matrix4&    GetWorldTransform() const { return mBody.GetWorldTransform(); }

private:
    // アニメーションID（Hero/hero_f.gltf に対応。旧 PlayerMotion）
    enum PlayerMotion
    {
        H_Dead     = 0,
        H_Guard    = 1,
        H_Hit1     = 2,
        H_Hit2     = 3,
        H_Jump     = 4,
        H_JumpSS   = 5,
        H_Kick     = 6,
        H_KickSS   = 7,
        H_Pump     = 9,
        H_PumpSS   = 10,
        H_Run      = 11,
        H_RunSS    = 12,
        H_Slash    = 13,
        H_Spin     = 14,
        H_Stab     = 15,
        H_Standing = 16,
        H_Stand    = 17,
        H_Walk     = 18,
        H_WalkSS   = 19
    };

    void UpdateMovementAnimation(const toy::InputState& state, int moveMotion);
    void SelectTarget(const toy::InputState& state);
    void InputAttack(const toy::InputState& state);

    toy::kit::Humanoid mBody;
};
