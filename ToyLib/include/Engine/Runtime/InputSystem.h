#pragma once

#include "Utils/MathUtil.h"

#include <SDL3/SDL.h>   // SDL3 はこれ1つでOK
#include <string>
#include <vector>
#include <array>
#include <unordered_map>

namespace toy {

//-------------------------------------------------------------
// ボタンステータス
// ・1フレーム単位での状態を表す
//-------------------------------------------------------------
enum ButtonState
{
    ENone,      // 何もしていない
    EPressed,   // このフレームで押された
    EReleased,  // このフレームで離された
    EHeld       // 押しっぱなし
};

//=============================================================
// 論理ボタン（ゲーム側で使う入力）
// ・「どのキー／パッドに割り当てるか」はバインディングで決める
//=============================================================
enum class GameButton
{
    A,
    B,
    X,
    Y,
    L1,
    L2,
    R1,
    R2,
    Start,
    Select,
    DPadUp,
    DPadDown,
    DPadLeft,
    DPadRight,

    KeyW,
    KeyA,
    KeyS,
    KeyD,

    Count
};

//=============================================================
// バインディング構造体
// ・1つの GameButton に対して
//   複数のキーボードキー／ゲームパッドボタンを割り当てられる
//=============================================================
struct ButtonBinding
{
    std::vector<SDL_Scancode>      Keyboard; // キーボード
    std::vector<SDL_GamepadButton> Gamepad;  // パッドボタン (SDL3 Gamepad)
};

//=============================================================
// 低レベル入力：キーボード
// ・SDL の生状態（スキャンコード）をラップ
//=============================================================
class KeyboardState
{
public:
    friend class InputSystem;

    // 現在押されているか
    bool GetKeyValue(SDL_Scancode keyCode) const;

    // 1フレーム単位での状態を返す（押された／離された／保持 等）
    ButtonState GetKeyState(SDL_Scancode keyCode) const;

private:
    const Uint8* mCurrState = nullptr;                     // SDL_GetKeyboardState()
    Uint8        mPrevState[SDL_SCANCODE_COUNT]{};         // 前フレームの状態
};

//=============================================================
// 低レベル入力：コントローラー（SDL3 Gamepad）
// ・ボタン／スティック／トリガーをラップ
//=============================================================
class ControllerState
{
public:
    friend class InputSystem;

    bool GetButtonValue(SDL_GamepadButton button) const;
    ButtonState GetButtonState(SDL_GamepadButton button) const;

    const Vector2& GetLeftStick()  const { return mLeftStick;  }
    const Vector2& GetRightStick() const { return mRightStick; }
    float GetLeftTrigger()  const { return mLeftTrigger;  }
    float GetRightTrigger() const { return mRightTrigger; }

    // ★ 追加: トリガーをボタン風に扱うための状態取得
    ButtonState GetTriggerState(bool isLeft) const;

    bool GetIsConnected() const { return mIsConnected; }

private:
    Uint8   mCurrButtons[SDL_GAMEPAD_BUTTON_COUNT] {};   // 現フレームのボタン状態
    Uint8   mPrevButtons[SDL_GAMEPAD_BUTTON_COUNT] {};   // 前フレームのボタン状態

    Vector2 mLeftStick        { Vector2::Zero };
    Vector2 mRightStick       { Vector2::Zero };
    float   mLeftTrigger      {};
    float   mRightTrigger     {};
    // トリガーの前フレーム値
    float   mPrevLeftTrigger  {};
    float   mPrevRightTrigger {};

    bool    mIsConnected      { false };
};

//=============================================================
// 高レベル入力：InputState
// ・KeyboardState / ControllerState をまとめたフレームスナップショット
// ・GameButton 単位の問い合わせもここから行う
//=============================================================
struct InputState
{
    KeyboardState   Keyboard   {};
    ControllerState Controller {};

    // 論理ボタンの状態問い合わせ
    bool IsButtonDown(GameButton button) const;
    bool IsButtonPressed(GameButton button) const;
    bool IsButtonReleased(GameButton button) const;

    // 自分を管理する InputSystem を登録
    void SetOwner(class InputSystem* inputSystem);

private:
    const class InputSystem* mOwner = nullptr;
};

//=============================================================
// 入力システム本体
// ・SDL イベントから KeyboardState / ControllerState を更新
// ・GameButton 単位の問い合わせ API を提供
// ・バインディング設定（JSON）にも対応
//=============================================================
class InputSystem
{
public:
    // 初期化（Gamepad オープンなどを行う）
    bool Initialize(SDL_Window* window);
    void Shutdown();
    
    // SDL_PollEvents 前に呼ぶ（前フレーム状態の確保など）
    void PrepareForUpdate();
    
    // SDL_PollEvents 後に呼ぶ（実際の入力更新）
    void Update();
    
    // 現在の入力状態（フレームスナップショット）を取得
    const InputState& GetState() const { return mState; }
    
    // JSON からボタンバインディングを読み込み
    bool LoadButtonConfig(const std::string& filePath);
    
    // 論理ボタン問い合わせ API
    bool IsButtonDown(GameButton button) const;
    bool IsButtonPressed(GameButton button) const;
    bool IsButtonReleased(GameButton button) const;
    
    // テキスト入力モード（SDL_StartTextInput / StopTextInput 用）
    void SetTextInputMode(bool enabled);
    bool IsTextInputMode() const { return mTextInputMode; }
    
private:
    // スティック／トリガー入力のデッドゾーンフィルタ（1D / 2D）
    float   Filter1D(int input);
    Vector2 Filter2D(int inputX, int inputY);
    
    SDL_Gamepad* mGamepad { nullptr };  // SDL3 Gamepad ハンドル
    InputState   mState {};             // 現在の入力状態
    
    // GameButton ごとのバインディング
    std::array<ButtonBinding, static_cast<size_t>(GameButton::Count)> mButtonBindings;
    
    bool mTextInputMode { false };      // テキスト入力中かどうか
    SDL_Window* mWindow { nullptr };    // 必要に応じて IME 等に利用
};

} // namespace toy
