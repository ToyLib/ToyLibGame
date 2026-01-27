#pragma once

#include "Utils/MathUtil.h"
#include <assimp/scene.h>

#include <vector>
#include <memory>

namespace toy {

//-------------------------------------------------------------
// BlendInfo
//  - fromAnim → 現在のアニメーション（ブレンド元）
//  - toAnim   → 遷移先のアニメーション（ブレンド先）
//  - blendDuration の間、0→1 に向かって補間
//-------------------------------------------------------------
struct BlendInfo
{
    int   fromAnimID     { -1 };     // ブレンド元クリップID
    int   toAnimID       { -1 };     // ブレンド先クリップID
    float blendDuration  { 0.30f };  // ブレンド時間（秒）
    float blendTime      { 0.0f };   // 経過時間（秒）
    bool  isBlending     { false };  // ブレンド中か

    void Reset()
    {
        fromAnimID    = -1;
        toAnimID      = -1;
        blendDuration = 0.30f;
        blendTime     = 0.0f;
        isBlending    = false;
    }
};

//-------------------------------------------------------------
// AnimationPlayer
//  - Mesh に紐づいたスケルトンアニメーションの再生制御
//  - ループ / 一回再生(PlayOnce) / クリップ間ブレンド(PlayBlend)
//-------------------------------------------------------------
class AnimationPlayer
{
public:
    explicit AnimationPlayer(std::shared_ptr<class Mesh> mesh);

    //---------------------------------------------------------
    // 更新
    //---------------------------------------------------------
    void Update(float deltaTime);

    //---------------------------------------------------------
    // 再生制御
    //---------------------------------------------------------
    void Play(int animID, bool loop = true);
    void PlayOnce(int animID, int nextAnimID);
    void PlayBlend(int fromAnimID, int toAnimID, float duration);

    void SetPlayRate(float rate) { mPlayRate = rate; }
    void Pause(bool paused) { mIsPaused = paused; }

    //---------------------------------------------------------
    // 状態問い合わせ
    //---------------------------------------------------------
    bool IsFinished() const { return mIsFinished; }     // PlayOnce の終了フラグ（or 非ループの終了）
    bool IsLooping()  const { return mIsLooping; }
    bool IsPaused()   const { return mIsPaused; }

    int  GetAnimID() const { return mAnimID; }

    // PlayOnce が走っているか（nextAnimID が有効なら true）
    bool IsPlayingOnce() const { return mNextAnimID >= 0; }

    //---------------------------------------------------------
    // 出力
    //---------------------------------------------------------
    const std::vector<Matrix4>& GetFinalMatrices() const { return mFinalMatrices; }

private:
    // Mesh/Clip 参照ヘルパ
    bool IsValidAnimID(int animID) const;

    // ticksPerSecond の安全化（Assimp で 0 が来ても止まらない）
    float GetSafeTicksPerSecond(int animID) const;

    // ブレンド更新
    void UpdateBlend(float deltaTime);

    // 通常更新
    void UpdateNormal(float deltaTime);

    // 行列の簡易補間（要素Lerp）
    Matrix4 LerpMatrix(const Matrix4& a, const Matrix4& b, float t);

private:
    std::shared_ptr<class Mesh> mMesh {};

    int   mAnimID      { 0 };
    float mPlayTime    { 0.0f };   // 秒
    float mPlayRate    { 1.0f };

    bool  mIsLooping   { true };
    bool  mIsPaused    { false };

    // PlayOnce 用
    int   mNextAnimID  { -1 };     // >=0 なら PlayOnce 中
    bool  mIsFinished  { false };

    std::vector<Matrix4> mFinalMatrices {};

    BlendInfo mBlend {};
};

} // namespace toy
