#pragma once

namespace toy {

//-------------------------------------------------------------
// GameTime
// ・ゲーム内の時間を表す構造体
// ・日付／時／分／秒を保持し、時間帯を 0.0〜1.0 で返す便利関数つき
//-------------------------------------------------------------
struct GameTime
{
    int   day    {  1 };    // 拡張用（今は 1日目固定）
    int   hour   { 12 };    // 0〜23
    int   minute {  0 };     // 0〜59
    float second { 0.0f };  // 秒（小数で滑らかに）

    // 1日のうちどれくらい経過したか（0.0〜1.0）
    float GetTimeOfDay01() const
    {
        float totalSec = hour * 3600.0f + minute * 60.0f + second;
        return totalSec / (24.0f * 3600.0f);
    }

    // 時間を「小数付きの時刻」として返す（例: 14.25 = 14:15）
    float GetHourFloat() const
    {
        return hour + minute / 60.0f + second / 3600.0f;
    }
};


//-------------------------------------------------------------
// TimeOfDaySystem
// ・ゲーム内の時間進行を管理するクラス
// ・昼夜サイクルや SkyDome、天候システムと連動して使用
//-------------------------------------------------------------
class TimeOfDaySystem
{
public:
    TimeOfDaySystem() = default;
    
    //---------------------------------------------------------
    // 更新
    //---------------------------------------------------------
    
    // deltaTime * timeScale ぶん時間を進める
    void Update(float deltaTime);
    
    
    //---------------------------------------------------------
    // 時間設定（直接セット）
    //---------------------------------------------------------
    
    void SetTime(int hour, int minute = 0, float second = 0.0f);
    
    
    //---------------------------------------------------------
    // 時間進行制御
    //---------------------------------------------------------
    
    // 時間の進行倍率（例: 60 → 1秒で1分進む）
    void SetTimeScale(float scale) { mTimeScale = scale; } // 0 なら停止
    
    // 時間の進行ON/OFF
    void SetRunning(bool r) { mRunning = r; }
    
    
    //---------------------------------------------------------
    // 取得 API
    //---------------------------------------------------------
    
    const GameTime& GetGameTime()  const { return mTime; }
    
    // 0.0〜1.0 の昼夜割合（SkyDome で使用）
    float GetTimeOfDay01() const { return mTime.GetTimeOfDay01(); }
    
    // 小数時刻（影の角度などに使いやすい）
    float GetHourFloat() const { return mTime.GetHourFloat(); }
    
    // 純粋な時 / 分 / 秒
    int GetHour()   const { return mTime.hour;   }
    int GetMinute() const { return mTime.minute; }

    // 秒の getter（typo: GetSccond → 正しくは GetSecond にしたいが本体は未変更）
    int GetSccond() const { return mTime.second; }
    
    
private:
    GameTime mTime      {};         // 現在のゲーム内時刻
    float    mTimeScale { 60.0f };  // 進行速度倍率（例: 60 = 1秒で1分進む）
    bool     mRunning   { true };   // 時間が進むかどうか
};

} // namespace toy
