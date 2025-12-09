#include "Engine/Runtime/TimeOfDaySystem.h"
#include <algorithm>
#include <iostream>

namespace toy {

//======================================================================
// TimeOfDaySystem
//   - ゲーム内の「日付・時刻」を管理するシステム
//   - 時間の進行速度（タイムスケール）を指定可能
//   - deltaTime に応じて内部時刻を進める
//   - Renderer → WeatherComponent → SkyDome などから参照される前提
//======================================================================

TimeOfDaySystem::TimeOfDaySystem()
: mTimeScale(60.0f)  // ● デフォルト：1秒でゲーム内1分進む
, mRunning(true)     // ● 時間の進行 ON/OFF
{
}

//----------------------------------------------------------------------
// 明示的に時間をセット 例: SetTime(6, 30, 0.0f)
//----------------------------------------------------------------------
void TimeOfDaySystem::SetTime(int hour, int minute, float second)
{
    mTime.hour   = std::clamp(hour,   0, 23);
    mTime.minute = std::clamp(minute, 0, 59);
    mTime.second = std::clamp(second, 0.0f, 59.999f);
}

//----------------------------------------------------------------------
// 時間更新処理（Game::UpdateFrame から呼ばれる）
//   - deltaTime × TimeScale の分だけ時間が進む
//   - 桁あふれ（秒→分、分→時、時→日）も処理
//----------------------------------------------------------------------
void TimeOfDaySystem::Update(float deltaTime)
{
    if (!mRunning) return;  // 停止中なら進めない
    
    float scaled = deltaTime * mTimeScale;   // タイムスケール適用（倍速・スロー）
    mTime.second += scaled;

    // --- 秒 → 分 ---
    while (mTime.second >= 60.0f)
    {
        mTime.second -= 60.0f;
        mTime.minute++;
    }

    // --- 分 → 時 ---
    while (mTime.minute >= 60)
    {
        mTime.minute -= 60;
        mTime.hour++;
    }

    // --- 時 → 日 ---
    while (mTime.hour >= 24)
    {
        mTime.hour -= 24;
        mTime.day++;
    }
}

} // namespace toy
