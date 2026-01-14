#include "Engine/Runtime/AnimationPlayer.h"
#include "Asset/Geometry/Mesh.h"

#include <algorithm> // std::clamp
#include <cmath>     // std::fmod
#include <iostream>

namespace toy {

namespace
{
    inline float Clamp01(float x)
    {
        if (x < 0.0f) return 0.0f;
        if (x > 1.0f) return 1.0f;
        return x;
    }
}

AnimationPlayer::AnimationPlayer(std::shared_ptr<Mesh> mesh)
    : mMesh(std::move(mesh))
{
}

//-------------------------------------------------------------
// ID 妥当性
//-------------------------------------------------------------
bool AnimationPlayer::IsValidAnimID(int animID) const
{
    if (!mMesh) return false;
    const auto& clips = mMesh->GetAnimationClips();
    return (animID >= 0 && animID < static_cast<int>(clips.size()) && clips[animID].mAnimation);
}

//-------------------------------------------------------------
// ticksPerSecond が 0 のケースを必ず救済する
//-------------------------------------------------------------
float AnimationPlayer::GetSafeTicksPerSecond(int animID) const
{
    // Assimp は mTicksPerSecond == 0 を返すことが普通にあるのでフォールバックする
    constexpr float kDefaultTps = 25.0f;

    if (!IsValidAnimID(animID))
    {
        return kDefaultTps;
    }

    const auto& clips = mMesh->GetAnimationClips();
    const auto* anim  = clips[animID].mAnimation;

    // 1) clip 側を優先
    if (clips[animID].mTicksPerSecond > 1e-4f)
    {
        return clips[animID].mTicksPerSecond;
    }

    // 2) aiAnimation 側
    if (anim && anim->mTicksPerSecond > 1e-4)
    {
        return static_cast<float>(anim->mTicksPerSecond);
    }

    // 3) どっちも無いならデフォルト
    return kDefaultTps;
}

//-------------------------------------------------------------
// Play
//  - 同じアニメ＋同じループ状態で、停止中でなければ何もしない
//  - ここを「状態の正規化ポイント」にする（PlayOnce/Blend の残骸を消す）
//-------------------------------------------------------------
void AnimationPlayer::Play(int animID, bool loop)
{
    if (!IsValidAnimID(animID))
    {
        return;
    }

    if (mAnimID == animID && mIsLooping == loop && !mIsPaused && !mBlend.isBlending && mNextAnimID < 0)
    {
        return;
    }

    mAnimID      = animID;
    mPlayTime    = 0.0f;
    mIsLooping   = loop;
    mIsPaused    = false;

    // PlayOnce 状態を必ずクリア
    mNextAnimID  = -1;
    mIsFinished  = false;

    // ブレンドも停止
    mBlend.Reset();
}

//-------------------------------------------------------------
// PlayOnce
//  - 非ループで animID を再生し、終わったら nextAnimID へ遷移
//-------------------------------------------------------------
void AnimationPlayer::PlayOnce(int animID, int nextAnimID)
{
    if (!IsValidAnimID(animID))
    {
        return;
    }
    // nextAnimID は -1 なら「遷移なし」扱いでも良い
    if (nextAnimID >= 0 && !IsValidAnimID(nextAnimID))
    {
        // 遷移先が不正なら「遷移なし」に丸める
        nextAnimID = -1;
    }

    mAnimID      = animID;
    mPlayTime    = 0.0f;
    mIsLooping   = false;
    mIsPaused    = false;

    mNextAnimID  = nextAnimID;
    mIsFinished  = false;

    // ブレンド中なら解除（PlayOnce が優先）
    mBlend.Reset();
}

//-------------------------------------------------------------
// PlayBlend
//  - from → to へ duration 秒でブレンド
//  - ブレンド中は「見た目上」は補間結果を出力する
//-------------------------------------------------------------
void AnimationPlayer::PlayBlend(int fromAnimID, int toAnimID, float duration)
{
    if (!IsValidAnimID(fromAnimID) || !IsValidAnimID(toAnimID))
    {
        return;
    }

    if (duration < 1e-4f)
    {
        // ほぼ瞬時なら普通に切り替え
        Play(toAnimID, true);
        return;
    }

    // ブレンド開始
    mBlend.fromAnimID    = fromAnimID;
    mBlend.toAnimID      = toAnimID;
    mBlend.blendDuration = duration;
    mBlend.blendTime     = 0.0f;
    mBlend.isBlending    = true;

    // 見た目上の「現在アニメ」は from にしておく（デバッグ用）
    mAnimID     = fromAnimID;
    mPlayTime   = 0.0f;      // ここは好み：遷移直前の時刻から繋ぎたいなら維持でもOK
    mIsPaused   = false;
    mIsFinished = false;

    // PlayOnce 状態を必ずクリア（状態が混ざるのを防ぐ）
    mNextAnimID = -1;

    // ブレンド後は基本ループで回す想定（必要なら外から Play() し直す）
    mIsLooping  = true;
}

//-------------------------------------------------------------
// Update
//-------------------------------------------------------------
static float SafeTPS(float tps)
{
    // Assimpで0のことがあるので保険
    return (tps > 1e-4f) ? tps : 30.0f;
}

void AnimationPlayer::Update(float deltaTime)
{
    if (!mMesh || mIsPaused) return;

    const auto& clips = mMesh->GetAnimationClips();
    if (mAnimID < 0 || mAnimID >= (int)clips.size()) return;

    const aiAnimation* anim = clips[mAnimID].mAnimation;
    const float tps = SafeTPS(clips[mAnimID].mTicksPerSecond);

    const float durationTicks = (float)anim->mDuration;
    const float durationSec   = (durationTicks > 0.0f) ? (durationTicks / tps) : 0.0f;

    //===========================
    // ★非ループ(PlayOnce) 終了判定：秒で判定（最強）
    //===========================
    if (!mIsLooping && durationSec > 0.0f)
    {
        const float curSec  = mPlayTime * mPlayRate;
        const float nextSec = (mPlayTime + deltaTime) * mPlayRate;

        // デバッグログ（必要なら）
        std::cout
            << "[Anim] id=" << mAnimID
            << " curSec=" << curSec
            << " nextSec=" << nextSec
            << " durationSec=" << durationSec
            << " rate=" << mPlayRate
            << " next=" << mNextAnimID
            << std::endl;

        if (nextSec >= durationSec)
        {
            std::cout << "[Anim] FINISHED id=" << mAnimID << std::endl;

            mIsFinished = true;

            if (mNextAnimID >= 0)
            {
                std::cout << "[Anim] -> Next id=" << mNextAnimID << std::endl;
                Play(mNextAnimID, true);      // Play()の中で mIsFinished=false にするのはOK
            }
            else
            {
                // nextが無いなら停止状態にしておくのもアリ
                // mIsPaused = true;
            }
            return;
        }
    }

    //===========================
    // 通常のポーズ計算
    //===========================
    const float timeInTicks = (mPlayTime * mPlayRate) * tps;
    const float animTime    = (durationTicks > 0.0f) ? std::fmod(timeInTicks, durationTicks) : 0.0f;

    mMesh->ComputePoseAtTime(animTime, anim, mFinalMatrices);

    mPlayTime += deltaTime;
}

//-------------------------------------------------------------
// ブレンド更新
//-------------------------------------------------------------
void AnimationPlayer::UpdateBlend(float deltaTime)
{
    // ブレンド係数（0→1）
    float t = (mBlend.blendDuration > 1e-6f) ? (mBlend.blendTime / mBlend.blendDuration) : 1.0f;
    t = Clamp01(t);

    const int fromID = mBlend.fromAnimID;
    const int toID   = mBlend.toAnimID;

    if (!IsValidAnimID(fromID) || !IsValidAnimID(toID))
    {
        // 不正になったらブレンド解除して安全側へ
        mBlend.Reset();
        return;
    }

    const auto& clips = mMesh->GetAnimationClips();
    const aiAnimation* fromAnim = clips[fromID].mAnimation;
    const aiAnimation* toAnim   = clips[toID].mAnimation;

    // tps は 0 があり得るので安全化
    const float tpsA = GetSafeTicksPerSecond(fromID);
    const float tpsB = GetSafeTicksPerSecond(toID);

    // ブレンド中も PlayRate を反映したい
    const float playSec = mPlayTime * mPlayRate;

    // 秒→tick
    float timeA = playSec * tpsA;
    float timeB = playSec * tpsB;

    // wrap
    float animTimeA = (fromAnim->mDuration > 0.0) ? std::fmod(timeA, static_cast<float>(fromAnim->mDuration)) : 0.0f;
    float animTimeB = (toAnim->mDuration   > 0.0) ? std::fmod(timeB, static_cast<float>(toAnim->mDuration))   : 0.0f;

    std::vector<Matrix4> poseA;
    std::vector<Matrix4> poseB;

    mMesh->ComputePoseAtTime(animTimeA, fromAnim, poseA);
    mMesh->ComputePoseAtTime(animTimeB, toAnim,   poseB);

    // サイズ不一致の保険（基本は一致する想定）
    const size_t count = std::min(poseA.size(), poseB.size());
    mFinalMatrices.resize(count);

    for (size_t i = 0; i < count; ++i)
    {
        mFinalMatrices[i] = LerpMatrix(poseA[i], poseB[i], t);
    }

    // 時間更新
    mBlend.blendTime += deltaTime;
    mPlayTime        += deltaTime;

    // 完了判定
    if (mBlend.blendTime >= mBlend.blendDuration)
    {
        const int next = mBlend.toAnimID;
        mBlend.Reset();

        // ブレンド完了後は to を通常再生へ
        Play(next, true);
    }
}

//-------------------------------------------------------------
// 通常更新（ループ/非ループ/PlayOnce）
//-------------------------------------------------------------
void AnimationPlayer::UpdateNormal(float deltaTime)
{
    if (!IsValidAnimID(mAnimID))
    {
        return;
    }

    const auto& clips = mMesh->GetAnimationClips();
    const aiAnimation* anim = clips[mAnimID].mAnimation;

    const float tps = GetSafeTicksPerSecond(mAnimID);

    // 秒→tick（次フレームを含めた終了判定が欲しいので next を作る）
    const float curTicks  = (mPlayTime) * mPlayRate * tps;
    const float nextTicks = (mPlayTime + deltaTime) * mPlayRate * tps;

    const float duration = (anim->mDuration > 0.0) ? static_cast<float>(anim->mDuration) : 0.0f;

    // ★★★ ここでログ ★★★
    if (!mIsLooping)
    {
        std::cout
            << "[Anim] id=" << mAnimID
            << " curTicks=" << curTicks
            << " nextTicks=" << nextTicks
            << " duration=" << duration
            << " playRate=" << mPlayRate
            << " finished=" << mIsFinished
            << " nextAnim=" << mNextAnimID
            << std::endl;
    }
    
    // 非ループ（PlayOnce含む）が終了したか
    if (!mIsLooping && duration > 0.0f && nextTicks >= duration)
    {
        mIsFinished = true;

        // 次が指定されていれば遷移（遷移した瞬間に finished は Play() 内で false に戻る）
        if (mNextAnimID >= 0)
        {
            Play(mNextAnimID, true);
        }
        else
        {
            // 「遷移なし」なら最後の姿勢を出して止める（好み）
            // duration - tiny で評価して最終フレーム近辺を作る
            float endTime = duration - 1e-3f;
            if (endTime < 0.0f) endTime = 0.0f;
            mMesh->ComputePoseAtTime(endTime, anim, mFinalMatrices);
        }
        return;
    }

    // ループ/再生中：wrap して姿勢計算
    float animTime = 0.0f;
    if (duration > 0.0f)
    {
        animTime = std::fmod(curTicks, duration);
        if (animTime < 0.0f) animTime = 0.0f;
    }

    mMesh->ComputePoseAtTime(animTime, anim, mFinalMatrices);

    // 時間更新
    mPlayTime += deltaTime;

    // ループの時は finished を立てない
    if (mIsLooping)
    {
        mIsFinished = false;
    }
}

//-------------------------------------------------------------
// 行列の線形補間（簡易）
//-------------------------------------------------------------
Matrix4 AnimationPlayer::LerpMatrix(const Matrix4& a, const Matrix4& b, float t)
{
    t = Clamp01(t);

    Matrix4 r;
    for (int i = 0; i < 4; ++i)
    {
        for (int j = 0; j < 4; ++j)
        {
            r.mat[i][j] = a.mat[i][j] * (1.0f - t) + b.mat[i][j] * t;
        }
    }
    return r;
}

} // namespace toy
