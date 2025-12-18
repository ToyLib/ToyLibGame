// Audio/SoundComponent.cpp
#include "Audio/SoundComponent.h"

#include "Engine/Core/Actor.h"
#include "Engine/Core/Application.h"
#include "Asset/AssetManager.h"
#include "Asset/Audio/SoundEffect.h"
#include "Engine/Render/Renderer.h"
#include "Utils/MathUtil.h"
#include <iostream>

namespace toy {

SoundComponent::SoundComponent(Actor* owner, int updateOrder)
    : Component(owner, updateOrder)
    , mVolume(1.0f)
    , mIsLoop(false)
    , mAutoPlay(false)
    , mUse3DSound(false)
    , mIsExclusive(false)
    , mHasPlayed(false)
    , mSource(0)
{
}

SoundComponent::~SoundComponent()
{
    // 自前のソースを停止・破棄
    Stop();
    if (mSource != 0)
    {
        alDeleteSources(1, &mSource);
        mSource = 0;
    }
}

//--------------------------------------
// 再生するサウンド（SE名）を指定
//--------------------------------------
void SoundComponent::SetSound(const std::string& fileName)
{
    mSoundName = fileName;
}

//--------------------------------------
// 再生開始
//--------------------------------------
void SoundComponent::Play()
{
    if (mSoundName.empty())
    {
        return;
    }
    
    // 排他再生モードなら、すでに再生中のときは無視
    if (mIsExclusive && IsPlaying())
    {
        return;
    }
    
    auto* app    = GetOwner()->GetApp();
    auto* assets = app->GetAssetManager();
    auto sound   = assets->GetSoundEffect(mSoundName);
    if (!sound)
    {
        return;
    }
    // ソース生成（初回のみ）
    if (mSource == 0)
    {
        alGenSources(1, &mSource);

        // 距離減衰に関するパラメータ（3D SE 用）
        // ※ mUse3DSound のオン/オフに応じて
        //    ここを分ける実装も可能
        alSourcef(mSource, AL_REFERENCE_DISTANCE, 3.0f);  // この距離まではほぼフル音量
        alSourcef(mSource, AL_MAX_DISTANCE,      50.0f);  // これ以上離れてもあまり変わらない
        alSourcef(mSource, AL_ROLLOFF_FACTOR,    1.0f);   // 減衰の強さ
    }

    // バッファ・ボリューム・ループ設定
    alSourcei(mSource, AL_BUFFER, sound->GetBuffer());
    alSourcef(mSource, AL_GAIN, mVolume);
    alSourcei(mSource, AL_LOOPING, mIsLoop ? AL_TRUE : AL_FALSE);

    // 現在の Actor 位置を OpenAL ソースに反映
    auto pos = GetOwner()->GetPosition();
    alSource3f(mSource, AL_POSITION, pos.x, pos.y, pos.z);

    // 再生開始
    alSourcePlay(mSource);
}

//--------------------------------------
// 再生停止
//--------------------------------------
void SoundComponent::Stop()
{
    if (mSource != 0)
    {
        alSourceStop(mSource);
    }
}

//--------------------------------------
// 再生中かどうか
//--------------------------------------
bool SoundComponent::IsPlaying() const
{
    if (mSource == 0)
    {
        return false;
    }
    
    ALint state = 0;
    alGetSourcei(mSource, AL_SOURCE_STATE, &state);
    return (state == AL_PLAYING);
}

//--------------------------------------
// 毎フレーム更新
//--------------------------------------
void SoundComponent::Update(float)
{
    // 自動再生フラグが立っていて、まだ未再生なら一度だけ再生
    if (mAutoPlay && !mHasPlayed)
    {
        Play();
        mHasPlayed = true;
    }

    if (mSource != 0)
    {
        ALint state = 0;
        alGetSourcei(mSource, AL_SOURCE_STATE, &state);

        // 再生中なら Actor の位置に追従させる
        if (state == AL_PLAYING)
        {
            auto pos = GetOwner()->GetPosition();
            alSource3f(mSource, AL_POSITION, pos.x, pos.y, pos.z);
        }

        // ループしない場合、再生終了後にフラグリセットなどを行いたければここで処理
        if (!mIsLoop && state == AL_STOPPED)
        {
            // TODO: 必要ならここでイベント通知やフラグリセットを行う
            // 例: mHasPlayed = false;
        }
    }
}

} // namespace toy
