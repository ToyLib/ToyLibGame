#include "Engine/Runtime/InputSystem.h"
#include "Utils/JsonHelper.h"

#include <SDL3/SDL.h>
#include <cstring>
#include <fstream>
#include <iostream>

namespace toy {

//=============================================================
// 文字列 → SDL_Scancode 変換
//   - JSON の "keyboard": ["W", "Space"] などを SDL_Scancode に
//=============================================================
static SDL_Scancode StringToScancode(const std::string& name)
{
    static const std::unordered_map<std::string, SDL_Scancode> table = {
        // アルファベット
        {"A", SDL_SCANCODE_A}, {"B", SDL_SCANCODE_B}, {"C", SDL_SCANCODE_C},
        {"D", SDL_SCANCODE_D}, {"E", SDL_SCANCODE_E}, {"F", SDL_SCANCODE_F},
        {"G", SDL_SCANCODE_G}, {"H", SDL_SCANCODE_H}, {"I", SDL_SCANCODE_I},
        {"J", SDL_SCANCODE_J}, {"K", SDL_SCANCODE_K}, {"L", SDL_SCANCODE_L},
        {"M", SDL_SCANCODE_M}, {"N", SDL_SCANCODE_N}, {"O", SDL_SCANCODE_O},
        {"P", SDL_SCANCODE_P}, {"Q", SDL_SCANCODE_Q}, {"R", SDL_SCANCODE_R},
        {"S", SDL_SCANCODE_S}, {"T", SDL_SCANCODE_T}, {"U", SDL_SCANCODE_U},
        {"V", SDL_SCANCODE_V}, {"W", SDL_SCANCODE_W}, {"X", SDL_SCANCODE_X},
        {"Y", SDL_SCANCODE_Y}, {"Z", SDL_SCANCODE_Z},

        // 数字キー（メインキー列）
        {"Num0", SDL_SCANCODE_0}, {"Num1", SDL_SCANCODE_1},
        {"Num2", SDL_SCANCODE_2}, {"Num3", SDL_SCANCODE_3},
        {"Num4", SDL_SCANCODE_4}, {"Num5", SDL_SCANCODE_5},
        {"Num6", SDL_SCANCODE_6}, {"Num7", SDL_SCANCODE_7},
        {"Num8", SDL_SCANCODE_8}, {"Num9", SDL_SCANCODE_9},

        // 特殊キー
        {"Space",      SDL_SCANCODE_SPACE},
        {"Escape",     SDL_SCANCODE_ESCAPE},
        {"Enter",      SDL_SCANCODE_RETURN},
        {"Tab",        SDL_SCANCODE_TAB},
        {"LeftShift",  SDL_SCANCODE_LSHIFT},
        {"RightShift", SDL_SCANCODE_RSHIFT},
        {"LeftCtrl",   SDL_SCANCODE_LCTRL},
        {"RightCtrl",  SDL_SCANCODE_RCTRL},

        // 矢印キー
        {"Up",    SDL_SCANCODE_UP},
        {"Down",  SDL_SCANCODE_DOWN},
        {"Left",  SDL_SCANCODE_LEFT},
        {"Right", SDL_SCANCODE_RIGHT},
    };

    auto it = table.find(name);
    if (it != table.end())
    {
        return it->second;
    }

    std::cerr << "[InputSystem] Unknown key name in config: "
              << name << std::endl;
    return SDL_SCANCODE_UNKNOWN;
}

//=============================================================
// 文字列 → SDL_GamepadButton 変換（SDL3 Gamepad）
//   - JSON の "gamepad": ["A", "DPadUp"] を SDL_GamepadButton に
//=============================================================
static SDL_GamepadButton StringToPadButton(const std::string& name)
{
    static const std::unordered_map<std::string, SDL_GamepadButton> table = {
        {"A", SDL_GAMEPAD_BUTTON_SOUTH},
        {"B", SDL_GAMEPAD_BUTTON_EAST},
        {"X", SDL_GAMEPAD_BUTTON_WEST},
        {"Y", SDL_GAMEPAD_BUTTON_NORTH},

        {"LeftShoulder",  SDL_GAMEPAD_BUTTON_LEFT_SHOULDER},  // L1
        {"RightShoulder", SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER}, // R1

        {"Start", SDL_GAMEPAD_BUTTON_START},
        {"Back",  SDL_GAMEPAD_BUTTON_BACK},

        {"DPadUp",    SDL_GAMEPAD_BUTTON_DPAD_UP},
        {"DPadDown",  SDL_GAMEPAD_BUTTON_DPAD_DOWN},
        {"DPadLeft",  SDL_GAMEPAD_BUTTON_DPAD_LEFT},
        {"DPadRight", SDL_GAMEPAD_BUTTON_DPAD_RIGHT},
    };

    auto it = table.find(name);
    if (it != table.end())
    {
        return it->second;
    }
    
    std::cerr << "[InputSystem] Unknown gamepad button name in config: "
              << name << std::endl;
    return SDL_GAMEPAD_BUTTON_INVALID;
}

//=============================================================
// 文字列 → GameButton 変換
//   - JSON の "A", "KeyW" を論理ボタン enum に変換
//=============================================================
static bool StringToGameButton(const std::string& name, GameButton& out)
{
    static const std::unordered_map<std::string, GameButton> table =
    {
        {"A", GameButton::A},
        {"B", GameButton::B},
        {"X", GameButton::X},
        {"Y", GameButton::Y},
        {"L1", GameButton::L1},
        {"L2", GameButton::L2},
        {"R1", GameButton::R1},
        {"R2", GameButton::R2},
        {"Start",  GameButton::Start},
        {"Select", GameButton::Select},
        {"DPadUp",    GameButton::DPadUp},
        {"DPadDown",  GameButton::DPadDown},
        {"DPadLeft",  GameButton::DPadLeft},
        {"DPadRight", GameButton::DPadRight},
        {"KeyW", GameButton::KeyW},
        {"KeyA", GameButton::KeyA},
        {"KeyS", GameButton::KeyS},
        {"KeyD", GameButton::KeyD},
    };

    auto it = table.find(name);
    if (it == table.end())
    {
        return false;
    }
    
    out = it->second;
    return true;
}

//=============================================================
// KeyboardState 実装
//=============================================================

bool KeyboardState::GetKeyValue(SDL_Scancode keyCode) const
{
    return mCurrState[keyCode] == 1;
}

ButtonState KeyboardState::GetKeyState(SDL_Scancode keyCode) const
{
    // 前フレーム / 今フレームからボタン状態を ENone/EPressed/EReleased/EHeld にマッピング
    if (mPrevState[keyCode] == 0)
    {
        if (mCurrState[keyCode] == 0)
        {
            return ENone;
        }
        else
        {
            return EPressed;
        }
    }
    else
    {
        if (mCurrState[keyCode] == 0)
        {
            return EReleased;
        }
        else
        {
            return EHeld;
        }
    }
}

//=============================================================
// ControllerState 実装（SDL3 Gamepad）
//=============================================================

bool ControllerState::GetButtonValue(SDL_GamepadButton button) const
{
    return mCurrButtons[button] == 1;
}

ButtonState ControllerState::GetButtonState(SDL_GamepadButton button) const
{
    if (mPrevButtons[button] == 0)
    {
        if (mCurrButtons[button] == 0)
        {
            return ENone;
        }
        else
        {
            return EPressed;
        }
    }
    else
    {
        if (mCurrButtons[button] == 0)
        {
            return EReleased;
        }
        else
        {
            return EHeld;
        }
    }
}

// ★ 追加: トリガーをボタン風に扱う
ButtonState ControllerState::GetTriggerState(bool isLeft) const
{
    constexpr float threshold = 0.5f; // 押した扱いにするしきい値（0〜1）

    const float prev = isLeft ? mPrevLeftTrigger  : mPrevRightTrigger;
    const float curr = isLeft ? mLeftTrigger      : mRightTrigger;

    const bool prevOn = (prev >= threshold);
    const bool currOn = (curr >= threshold);

    if (!prevOn && !currOn)
    {
        return ENone;
    }
    if (!prevOn && currOn)
    {
        return EPressed;
    }
    if (prevOn && !currOn)
    {
        return EReleased;
    }
    return EHeld;
}

//=============================================================
// InputState ラッパ
//   - ゲーム側からは InputSystem を意識せず GameButton ベースで扱える
//=============================================================

bool InputState::IsButtonDown(GameButton btn) const
{
    return mOwner && mOwner->IsButtonDown(btn);
}

bool InputState::IsButtonPressed(GameButton btn) const
{
    return mOwner && mOwner->IsButtonPressed(btn);
}

bool InputState::IsButtonReleased(GameButton btn) const
{
    return mOwner && mOwner->IsButtonReleased(btn);
}

void InputState::SetOwner(class InputSystem* inputSystem)
{
    mOwner = inputSystem;
}

//=============================================================
// InputSystem::Initialize (SDL3)
//   - キーボード／Gamepad の初期状態セット
//   - テキスト入力モード OFF
//=============================================================
bool InputSystem::Initialize(SDL_Window* window)
{
    mWindow = window;
    
    // キーボード状態取得
    int numKeys = 0;
    const bool* keys = SDL_GetKeyboardState(&numKeys);
    mState.Keyboard.mCurrState =
        reinterpret_cast<const Uint8*>(keys);
    std::memset(mState.Keyboard.mPrevState, 0,
                sizeof(mState.Keyboard.mPrevState));

    // Gamepad 検出 (SDL3 Gamepad API)
    mGamepad = nullptr;
    int count = 0;
    SDL_JoystickID* ids = SDL_GetGamepads(&count);
    if (ids && count > 0)
    {
        // とりあえず 1 個目を使用
        mGamepad = SDL_OpenGamepad(ids[0]);
    }
    if (ids)
    {
        SDL_free(ids);
    }

    mState.Controller.mIsConnected = (mGamepad != nullptr);
    std::memset(mState.Controller.mCurrButtons, 0,
                sizeof(mState.Controller.mCurrButtons));
    std::memset(mState.Controller.mPrevButtons, 0,
                sizeof(mState.Controller.mPrevButtons));

    mState.Controller.mLeftStick        = Vector2::Zero;
    mState.Controller.mRightStick       = Vector2::Zero;
    mState.Controller.mLeftTrigger      = 0.0f;
    mState.Controller.mRightTrigger     = 0.0f;
    mState.Controller.mPrevLeftTrigger  = 0.0f;
    mState.Controller.mPrevRightTrigger = 0.0f;

    // InputState から InputSystem に逆参照できるようにセット
    mState.SetOwner(this);

    // デフォルトではテキスト入力 OFF
    mTextInputMode = false;
    SDL_StopTextInput(mWindow);

    return true;
}

void InputSystem::Shutdown()
{
    if (mGamepad)
    {
        SDL_CloseGamepad(mGamepad);
        mGamepad = nullptr;
    }
}

//=============================================================
// フレーム更新前処理
//   - 前フレームの状態を Prev にコピーしておく
//=============================================================
void InputSystem::PrepareForUpdate()
{
    std::memcpy(mState.Keyboard.mPrevState,
                mState.Keyboard.mCurrState,
                sizeof(mState.Keyboard.mPrevState));

    std::memcpy(mState.Controller.mPrevButtons,
                mState.Controller.mCurrButtons,
                sizeof(mState.Controller.mPrevButtons));

    // ★ 追加: トリガーの前フレーム値を保存
    mState.Controller.mPrevLeftTrigger  = mState.Controller.mLeftTrigger;
    mState.Controller.mPrevRightTrigger = mState.Controller.mRightTrigger;
}

//=============================================================
// フレーム更新
//   - SDL_PollEvent による内部更新後に呼び出される想定
//   - Gamepad 状態を SDL から取得
//=============================================================
void InputSystem::Update()
{
    // SDL_PollEvent で内部キーボード状態は更新済み前提

    if (mState.Controller.mIsConnected && mGamepad)
    {
        // ボタン状態取得
        for (int i = 0; i < SDL_GAMEPAD_BUTTON_COUNT; ++i)
        {
            mState.Controller.mCurrButtons[i] =
                SDL_GetGamepadButton(
                    mGamepad,
                    static_cast<SDL_GamepadButton>(i)
                );
        }

        // トリガー（0〜1 に正規化）
        mState.Controller.mLeftTrigger =
            Filter1D(SDL_GetGamepadAxis(mGamepad, SDL_GAMEPAD_AXIS_LEFT_TRIGGER));
        mState.Controller.mRightTrigger =
            Filter1D(SDL_GetGamepadAxis(mGamepad, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER));

        // 左スティック
        int x = SDL_GetGamepadAxis(mGamepad, SDL_GAMEPAD_AXIS_LEFTX);
        int y = -SDL_GetGamepadAxis(mGamepad, SDL_GAMEPAD_AXIS_LEFTY);
        mState.Controller.mLeftStick = Filter2D(x, y);

        // 右スティック
        x = SDL_GetGamepadAxis(mGamepad, SDL_GAMEPAD_AXIS_RIGHTX);
        y = -SDL_GetGamepadAxis(mGamepad, SDL_GAMEPAD_AXIS_RIGHTY);
        mState.Controller.mRightStick = Filter2D(x, y);
    }
}

//=============================================================
// アナログ入力フィルタ（1D）
//   - デッドゾーン処理付きスカラー
//=============================================================
float InputSystem::Filter1D(int input)
{
    const int   deadZone = 250;
    const float maxValue = 32767.0f;

    float retVal = 0.0f;
    int absValue = input > 0 ? input : -input;

    if (absValue > deadZone)
    {
        retVal = static_cast<float>(absValue - deadZone) /
                 (maxValue - deadZone);
        retVal = input > 0 ? retVal : -retVal;
        retVal = Math::Clamp(retVal, -1.0f, 1.0f);
    }
    return retVal;
}

//=============================================================
// アナログ入力フィルタ（2D ベクトル）
//   - 円形デッドゾーン & 正規化
//=============================================================
Vector2 InputSystem::Filter2D(int inputX, int inputY)
{
    const float deadZone = 8000.0f;
    const float maxValue = 30000.0f;

    Vector2 dir(static_cast<float>(inputX),
                static_cast<float>(inputY));
    float length = dir.Length();

    if (length < deadZone)
    {
        dir = Vector2::Zero;
    }
    else
    {
        float f = (length - deadZone) / (maxValue - deadZone);
        f = Math::Clamp(f, 0.0f, 1.0f);
        dir *= f / length;
    }
    return dir;
}

//=============================================================
// ボタンコンフィグ読み込み (JSON)
//   - "buttons" オブジェクトを走査して、
//     各 GameButton に keyboard / gamepad バインドを登録
//=============================================================
bool InputSystem::LoadButtonConfig(const std::string& filePath)
{
    std::ifstream ifs(filePath);
    if (!ifs)
    {
        std::cerr << "[InputSystem] Failed to open button config: "
                  << filePath << std::endl;
        return false;
    }

    nlohmann::json root;
    try
    {
        ifs >> root;
    }
    catch (const std::exception& e)
    {
        std::cerr << "[InputSystem] JSON parse error in "
                  << filePath << ": " << e.what() << std::endl;
        return false;
    }

    if (!root.contains("buttons") || !root["buttons"].is_object())
    {
        std::cerr << "[InputSystem] 'buttons' object not found in: "
                  << filePath << std::endl;
        return false;
    }

    const auto& buttonsObj = root["buttons"];

    // 既存のバインドをクリア
    for (auto& b : mButtonBindings)
    {
        b.Keyboard.clear();
        b.Gamepad.clear();
    }

    // "buttons": { "A": { "keyboard": [...], "gamepad": [...] }, ... }
    for (auto it = buttonsObj.begin(); it != buttonsObj.end(); ++it)
    {
        const std::string       buttonName = it.key();
        const nlohmann::json&   buttonJson = it.value();

        GameButton gb;
        if (!StringToGameButton(buttonName, gb))
        {
            std::cerr << "[InputSystem] Unknown GameButton in config: "
                      << buttonName << std::endl;
            continue;
        }

        auto idx = static_cast<size_t>(gb);
        ButtonBinding& binding = mButtonBindings[idx];

        // キーボード側
        std::vector<std::string> keyNames;
        if (JsonHelper::GetStringArray(buttonJson, "keyboard", keyNames))
        {
            for (const auto& keyName : keyNames)
            {
                SDL_Scancode sc = StringToScancode(keyName);
                if (sc != SDL_SCANCODE_UNKNOWN)
                {
                    binding.Keyboard.push_back(sc);
                }
            }
        }

        // ゲームパッド側（トリガーはここでは扱わない）
        std::vector<std::string> padNames;
        if (JsonHelper::GetStringArray(buttonJson, "gamepad", padNames))
        {
            for (const auto& padName : padNames)
            {
                SDL_GamepadButton b = StringToPadButton(padName);
                if (b != SDL_GAMEPAD_BUTTON_INVALID)
                {
                    binding.Gamepad.push_back(b);
                }
            }
        }
    }

    std::cerr << "[InputSystem] Loaded button config: "
              << filePath << std::endl;
    return true;
}

//=============================================================
// 論理ボタン問い合わせ（Down / Pressed / Released）
//   - キーボード + Gamepad のバインドをまとめて判定
//   - IsButtonDown は「押しっぱ + 押した瞬間」両方を true にする
//=============================================================
bool InputSystem::IsButtonDown(GameButton button) const
{
    auto idx = static_cast<size_t>(button);
    const ButtonBinding& binding = mButtonBindings[idx];

    // キーボード
    for (auto sc : binding.Keyboard)
    {
        ButtonState s = mState.Keyboard.GetKeyState(sc);
        if (s == EPressed || s == EHeld)
        {
            return true;
        }
    }

    // パッドボタン
    for (auto pb : binding.Gamepad)
    {
        ButtonState s = mState.Controller.GetButtonState(pb);
        if (s == EPressed || s == EHeld)
        {
            return true;
        }
    }

    // ★ 追加: L2/R2 はアナログトリガーもボタンとして扱う
    if (button == GameButton::L2)
    {
        ButtonState s = mState.Controller.GetTriggerState(true);
        if (s == EPressed || s == EHeld)
        {
            return true;
        }
    }
    else if (button == GameButton::R2)
    {
        ButtonState s = mState.Controller.GetTriggerState(false);
        if (s == EPressed || s == EHeld)
        {
            return true;
        }
    }

    // --- 右スティックを WASD としても扱う ---
    const Vector2& rs = mState.Controller.GetRightStick();
    const float threshold = 0.3f;

    switch (button)
    {
    case GameButton::KeyW:
        if (rs.y > threshold)
        {
            return true;
        }
        break;
    case GameButton::KeyS:
        if (rs.y < -threshold)
        {
            return true;
        }
        break;
    case GameButton::KeyD:
        if (rs.x > threshold)
        {
            return true;
        }
        break;
    case GameButton::KeyA:
        if (rs.x < -threshold)
        {
            return true;
        }
        break;
    default:
        break;
    }

    return false;
}

bool InputSystem::IsButtonPressed(GameButton button) const
{
    auto idx = static_cast<size_t>(button);
    const ButtonBinding& binding = mButtonBindings[idx];

    // キーボード
    for (auto sc : binding.Keyboard)
    {
        if (mState.Keyboard.GetKeyState(sc) == EPressed)
        {
            return true;
        }
    }

    // パッドボタン
    for (auto pb : binding.Gamepad)
    {
        if (mState.Controller.GetButtonState(pb) == EPressed)
        {
            return true;
        }
    }

    // ★ 追加: L2/R2 はトリガーの「押された瞬間」も見る
    if (button == GameButton::L2)
    {
        if (mState.Controller.GetTriggerState(true) == EPressed)
        {
            return true;
        }
    }
    else if (button == GameButton::R2)
    {
        if (mState.Controller.GetTriggerState(false) == EPressed)
        {
            return true;
        }
    }

    return false;
}

bool InputSystem::IsButtonReleased(GameButton button) const
{
    auto idx = static_cast<size_t>(button);
    const ButtonBinding& binding = mButtonBindings[idx];

    // キーボード
    for (auto sc : binding.Keyboard)
    {
        if (mState.Keyboard.GetKeyState(sc) == EReleased)
        {
            return true;
        }
    }

    // パッドボタン
    for (auto pb : binding.Gamepad)
    {
        if (mState.Controller.GetButtonState(pb) == EReleased)
        {
            return true;
        }
    }

    // ★ 追加: L2/R2 はトリガーの「離された瞬間」も見る
    if (button == GameButton::L2)
    {
        if (mState.Controller.GetTriggerState(true) == EReleased)
        {
            return true;
        }
    }
    else if (button == GameButton::R2)
    {
        if (mState.Controller.GetTriggerState(false) == EReleased)
        {
            return true;
        }
    }

    return false;
}

//=============================================================
// テキスト入力モード切り替え
//   - true  : SDL_StartTextInput
//   - false : SDL_StopTextInput
//   - すでに同じ状態なら何もしない
//=============================================================
void InputSystem::SetTextInputMode(bool enabled)
{
    if (enabled == mTextInputMode)
    {
        return;
    }
    mTextInputMode = enabled;

    if (enabled)
    {
        SDL_StartTextInput(mWindow);
    }
    else
    {
        SDL_StopTextInput(mWindow);
    }
}

} // namespace toy
