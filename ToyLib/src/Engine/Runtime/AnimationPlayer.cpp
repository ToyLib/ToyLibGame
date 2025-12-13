#include "Asset/Geometry/Mesh.h"
#include "Engine/Runtime/AnimationPlayer.h"
#include <assimp/scene.h>
#include <iostream>

namespace toy {

//=============================================================
// AnimationPlayer
//  - Mesh に紐づくスケルトンアニメーションの再生制御
//  - ループ再生 / 一回再生 / クリップ間ブレンド などを担当
//=============================================================

AnimationPlayer::AnimationPlayer(std::shared_ptr<Mesh> mesh)
    : mMesh(mesh)
    , mAnimID(0)
    , mPlayTime(0.0f)
    , mPlayRate(1.0f)
    , mIsLooping(true)
    , mIsPaused(false)
    , mNextAnimID(-1)
    , mIsFinished(false)
{
}

//-------------------------------------------------------------
// アニメ再生開始
//  - animID: 再生するクリップ番号
//  - loop  : ループするかどうか
//  - 同じアニメ & 設定で再生中の場合は何もしない
//-------------------------------------------------------------
void AnimationPlayer::Play(int animID, bool loop)
{
    // 同じ内容なら再生しなおさない
    if (mAnimID == animID && mIsLooping == loop && !mIsPaused)
    {
        return;
    }
    
    mAnimID     = animID;
    mPlayTime   = 0.0f;
    mIsLooping  = loop;
    mIsPaused   = false;
    mIsFinished = false;
    mNextAnimID = -1;
}

//-------------------------------------------------------------
// 毎フレーム更新
//  - deltaTime: 経過秒
//  - ブレンド中ならブレンド用の姿勢計算
//  - 通常時は現在クリップの姿勢を計算
//-------------------------------------------------------------
void AnimationPlayer::Update(float deltaTime)
{
    if (!mMesh || mIsPaused)
    {
        return;
    }
    
    //=========================================================
    // ① クリップ間ブレンド中
    //=========================================================
    if (mBlend.isBlending)
    {
        // ブレンド係数 t (0〜1) を進める
        float t = mBlend.blendTime / mBlend.blendDuration;
        t = std::clamp(t, 0.0f, 1.0f);
        
        std::vector<Matrix4> poseA;
        std::vector<Matrix4> poseB;
        
        // from/to それぞれで「現在の再生時間」をティックに変換
        float timeA = mPlayTime * mBlend.fromAnim->mTicksPerSecond;
        float timeB = mPlayTime * mBlend.toAnim->mTicksPerSecond;
        
        // アニメーション周期で wrap してポーズを取得
        mMesh->ComputePoseAtTime(
            fmod(timeA, mBlend.fromAnim->mDuration),
            mBlend.fromAnim,
            poseA
        );
        mMesh->ComputePoseAtTime(
            fmod(timeB, mBlend.toAnim->mDuration),
            mBlend.toAnim,
            poseB
        );
        
        // 2 つのポーズを行列補間して最終ボーン行列を生成
        mFinalMatrices.resize(poseA.size());
        for (size_t i = 0; i < poseA.size(); i++)
        {
            mFinalMatrices[i] = LerpMatrix(poseA[i], poseB[i], t);
        }
        
        // ブレンド時間と全体の再生時間を進める
        mBlend.blendTime += deltaTime;
        mPlayTime        += deltaTime;
        
        // ブレンド完了したら、toAnim 側のクリップへ通常再生で切り替え
        if (mBlend.blendTime >= mBlend.blendDuration)
        {
            mBlend.isBlending = false;
            Play(FindClipIndex(mBlend.toAnim));  // 次の通常再生へ
        }
        
        // ブレンド中はここで処理終了
        return;
    }
    
    //=========================================================
    // ② 通常のアニメ再生
    //=========================================================
    const auto& clips = mMesh->GetAnimationClips();
    if (mAnimID < 0 || mAnimID >= static_cast<int>(clips.size()))
    {
        return;
    }
    
    const aiAnimation* anim = clips[mAnimID].mAnimation;
    float ticksPerSecond    = clips[mAnimID].mTicksPerSecond;
    
    // 経過秒 → ティック に変換
    float timeInTicks = mPlayTime * mPlayRate * ticksPerSecond;
    float animTime    = fmod(timeInTicks, anim->mDuration);
    
    // 非ループアニメで最後まで再生しきった場合
    if (!mIsLooping && timeInTicks >= anim->mDuration)
    {
        mIsFinished = true;
        
        // 次に再生するアニメが指定されていれば切り替え
        if (mNextAnimID >= 0)
        {
            Play(mNextAnimID);
        }
        return;
    }
    
    // 現在時間のポーズを計算して最終ボーン行列として保持
    mMesh->ComputePoseAtTime(animTime, anim, mFinalMatrices);
    
    // 経過時間更新
    mPlayTime += deltaTime;
}

//-------------------------------------------------------------
// 1 回再生用
//  - animID    : 1 回だけ再生するクリップ
//  - nextAnimID: 再生終了後に遷移するクリップ（-1 でなし）
//-------------------------------------------------------------
void AnimationPlayer::PlayOnce(int animID, int nextAnimID)
{
    mAnimID     = animID;
    mPlayTime   = 0.0f;
    mIsLooping  = false;
    mIsPaused   = false;
    mIsFinished = false;
    mNextAnimID = nextAnimID;
}

//-------------------------------------------------------------
// クリップ間ブレンド開始
//  - fromAnimID: ブレンド元クリップ
//  - toAnimID  : ブレンド先クリップ
//  - duration  : ブレンドにかける時間（秒）
//-------------------------------------------------------------
void AnimationPlayer::PlayBlend(int fromAnimID, int toAnimID, float duration)
{
    const auto& clips = mMesh->GetAnimationClips();
    if (fromAnimID < 0 || fromAnimID >= static_cast<int>(clips.size()))
    {
        return;
    }
    if (toAnimID < 0 || toAnimID >= static_cast<int>(clips.size()))
    {
        return;
    }
    mBlend.fromAnim      = clips[fromAnimID].mAnimation;
    mBlend.toAnim        = clips[toAnimID].mAnimation;
    mBlend.blendDuration = duration;
    mBlend.blendTime     = 0.0f;
    mBlend.isBlending    = true;
}

//-------------------------------------------------------------
// 行列の線形補間（要素ごと）
//  - ※本来は平行移動/回転/スケールを分解して補間した方が綺麗だが
//    ここでは簡易に 4x4 行列の要素を直接 Lerp している
//-------------------------------------------------------------
Matrix4 AnimationPlayer::LerpMatrix(const Matrix4& a, const Matrix4& b, float t)
{
    Matrix4 result;
    for (int i = 0; i < 4; i++)
    {
        for (int j = 0; j < 4; j++)
        {
            result.mat[i][j] = a.mat[i][j] * (1.0f - t) + b.mat[i][j] * t;
        }
    }
    return result;
}

//-------------------------------------------------------------
// aiAnimation* から対応するクリップ番号を探す
//-------------------------------------------------------------
int AnimationPlayer::FindClipIndex(const aiAnimation* anim) const
{
    const auto& clips = mMesh->GetAnimationClips();
    for (size_t i = 0; i < clips.size(); i++)
    {
        if (clips[i].mAnimation == anim)
        {
            return static_cast<int>(i);
        }
    }
    return -1; // 見つからない場合
}

} // namespace toy
