// Audio/SoundMixer.cpp
#include "Audio/SoundMixer.h"

#include "Asset/AssetManager.h"
#include "Asset/Audio/Music.h"
#include "Asset/Audio/SoundEffect.h"

#include <cstdio>
#include <algorithm>

namespace toy {

SoundMixer::SoundMixer(AssetManager* assetManager)
: mAssetManager(assetManager)
, mDevice(nullptr)
, mContext(nullptr)
, mBgmSource(0)
, mBgmPlaying(false)
, mBgmLoop(true)
, mBgmEnabled(true)
, mSoundEnabled(true)
, mVolume(1.0f)
{
    // OpenAL 初期化（デバイス + コンテキスト）
    InitOpenAL();

    // BGM デコード用一時バッファ
    mBgmDecodeBuffer.resize(BGM_CHUNK_SIZE);
}

SoundMixer::~SoundMixer()
{
    // BGM ソース・バッファ片付け
    ShutdownBGMSource();

    // SE 用ワンショットソースの片付け
    for (ALuint src : mOneShotSources)
    {
        alSourceStop(src);
        alDeleteSources(1, &src);
    }
    mOneShotSources.clear();

    // OpenAL デバイス/コンテキスト破棄
    ShutdownOpenAL();
}

//--------------------------------------
// OpenAL 初期化 / 後始末
//--------------------------------------
void SoundMixer::InitOpenAL()
{
    // 既定のデバイスを開く
    mDevice = alcOpenDevice(nullptr);
    if (!mDevice)
    {
        printf("[SoundMixer] Failed to open device\n");
        return;
    }

    // コンテキスト作成
    mContext = alcCreateContext(mDevice, nullptr);
    if (!mContext)
    {
        printf("[SoundMixer] Failed to create context\n");
        alcCloseDevice(mDevice);
        mDevice = nullptr;
        return;
    }

    alcMakeContextCurrent(mContext);

    // 距離減衰モデル（デフォルト: 逆数距離 + クランプ）
    alDistanceModel(AL_INVERSE_DISTANCE_CLAMPED);

    ALenum err = alGetError();
    if (err != AL_NO_ERROR)
    {
        printf("[SoundMixer] OpenAL error after init: 0x%x\n", err);
    }
}

void SoundMixer::ShutdownOpenAL()
{
    if (mContext)
    {
        alcMakeContextCurrent(nullptr);
        alcDestroyContext(mContext);
        mContext = nullptr;
    }
    if (mDevice)
    {
        alcCloseDevice(mDevice);
        mDevice = nullptr;
    }
}

//--------------------------------------
// BGM 用ソース/バッファ準備・破棄
//--------------------------------------
void SoundMixer::InitBGMSource()
{
    if (mBgmSource == 0)
    {
        // ストリーミング用ソース + バッファ
        alGenSources(1, &mBgmSource);
        alGenBuffers(BGM_NUM_BUFFERS, mBgmBuffers);

        // BGM は原点固定＆距離減衰なし（2D 的）
        alSource3f(mBgmSource, AL_POSITION, 0, 0, 0);
        alSourcef (mBgmSource, AL_ROLLOFF_FACTOR, 0.0f);
        alSourcei (mBgmSource, AL_LOOPING, AL_FALSE); // ループは自前で実装
    }
}

void SoundMixer::ShutdownBGMSource()
{
    if (mBgmSource != 0)
    {
        // 再生停止
        alSourceStop(mBgmSource);

        // キュー済みバッファを外す
        ALint queued = 0;
        alGetSourcei(mBgmSource, AL_BUFFERS_QUEUED, &queued);
        while (queued-- > 0)
        {
            ALuint buf = 0;
            alSourceUnqueueBuffers(mBgmSource, 1, &buf);
        }

        // ソース・バッファ削除
        alDeleteSources(1, &mBgmSource);
        alDeleteBuffers(BGM_NUM_BUFFERS, mBgmBuffers);

        mBgmSource = 0;
    }
}

//--------------------------------------
// 全体設定
//--------------------------------------
void SoundMixer::SetBGMEnable(bool enable)
{
    mBgmEnabled = enable;
    if (!mBgmEnabled)
        StopBGM();
}

void SoundMixer::SetSoundEnable(bool enable)
{
    mSoundEnabled = enable;
}

void SoundMixer::SetVolume(float volume)
{
    mVolume = std::clamp(volume, 0.0f, 1.0f);

    if (mBgmSource != 0)
        alSourcef(mBgmSource, AL_GAIN, mVolume);
}

//--------------------------------------
// BGM ロード / 再生制御
//--------------------------------------
bool SoundMixer::LoadBGM(const std::string& fileName)
{
    // AssetManager 経由で Music を取得
    mCurrentBGM = mAssetManager->GetMusic(fileName);
    return (mCurrentBGM != nullptr);
}

void SoundMixer::PlayBGM()
{
    if (!mBgmEnabled || !mCurrentBGM) return;

    InitBGMSource();

    // 既存キューをすべて外す
    alSourceStop(mBgmSource);
    ALint queued = 0;
    alGetSourcei(mBgmSource, AL_BUFFERS_QUEUED, &queued);
    while (queued-- > 0)
    {
        ALuint buf = 0;
        alSourceUnqueueBuffers(mBgmSource, 1, &buf);
    }

    // デコード位置を先頭に戻す
    mCurrentBGM->Rewind();

    // 最初に複数バッファを埋めてキューへ
    for (int i = 0; i < BGM_NUM_BUFFERS; ++i)
    {
        size_t bytes = mCurrentBGM->ReadChunk(
            mBgmDecodeBuffer.data(),
            BGM_CHUNK_SIZE
        );
        if (bytes == 0) break;

        ALenum format = (mCurrentBGM->GetChannels() == 2)
            ? AL_FORMAT_STEREO16
            : AL_FORMAT_MONO16;

        alBufferData(mBgmBuffers[i],
                     format,
                     mBgmDecodeBuffer.data(),
                     static_cast<ALsizei>(bytes),
                     static_cast<ALsizei>(mCurrentBGM->GetRate()));

        alSourceQueueBuffers(mBgmSource, 1, &mBgmBuffers[i]);
    }

    alSourcef(mBgmSource, AL_GAIN, mVolume);
    alSourcePlay(mBgmSource);
    mBgmPlaying = true;
}

void SoundMixer::StopBGM()
{
    if (mBgmSource != 0)
    {
        alSourceStop(mBgmSource);
        mBgmPlaying = false;

        if (mCurrentBGM)
            mCurrentBGM->Rewind();
    }
}

//--------------------------------------
// 効果音（ワンショット）
//--------------------------------------
void SoundMixer::PlaySoundEffect(const std::string& fileName)
{
    if (!mSoundEnabled) return;

    auto se = mAssetManager->GetSoundEffect(fileName);
    if (!se) return;

    // 1 回使い捨てソースを生成
    ALuint src = 0;
    alGenSources(1, &src);

    alSourcei(src, AL_BUFFER, se->GetBuffer());
    alSourcef(src, AL_GAIN, mVolume);
    alSource3f(src, AL_POSITION, 0, 0, 0);   // 現状は 2D 的に原点固定
    alSourcef(src, AL_ROLLOFF_FACTOR, 0.0f); // 距離減衰なし

    alSourcePlay(src);
    mOneShotSources.push_back(src);
}

//--------------------------------------
// 毎フレーム更新
//  - リスナー位置更新（カメラの invViewMatrix ベース）
//  - BGM ストリーミング
//  - 再生終了した SE ソースの削除
//--------------------------------------
void SoundMixer::Update(float,
                        const Matrix4& invViewMatrix)
{
    //====================
    // リスナー更新
    //====================
    Vector3 pos      = invViewMatrix.GetTranslation();
    Vector3 up       = invViewMatrix.GetYAxis();
    Vector3 forward  = invViewMatrix.GetZAxis();   // 左手系: +Z が奥 → 手前が前向きなので -Z を前とみなす
    forward = forward * -1.0f;

    alListener3f(AL_POSITION, pos.x, pos.y, pos.z);

    float ori[6] = {
        forward.x, forward.y, forward.z,
        up.x,      up.y,      up.z
    };
    alListenerfv(AL_ORIENTATION, ori);

    float vel[3] = {0, 0, 0};
    alListenerfv(AL_VELOCITY, vel);

    //====================
    // BGM ストリーミング
    //====================
    if (mBgmPlaying && mCurrentBGM && mBgmSource != 0)
    {
        ALint processed = 0;
        alGetSourcei(mBgmSource, AL_BUFFERS_PROCESSED, &processed);

        while (processed-- > 0)
        {
            ALuint buf = 0;
            alSourceUnqueueBuffers(mBgmSource, 1, &buf);

            size_t bytes = mCurrentBGM->ReadChunk(
                mBgmDecodeBuffer.data(),
                BGM_CHUNK_SIZE
            );

            if (bytes > 0)
            {
                ALenum format = (mCurrentBGM->GetChannels() == 2)
                    ? AL_FORMAT_STEREO16
                    : AL_FORMAT_MONO16;

                alBufferData(buf,
                             format,
                             mBgmDecodeBuffer.data(),
                             static_cast<ALsizei>(bytes),
                             static_cast<ALsizei>(mCurrentBGM->GetRate()));

                alSourceQueueBuffers(mBgmSource, 1, &buf);
            }
            else
            {
                // 曲末 → ループ再生（今のロジックはそのまま）
                mCurrentBGM->Rewind();
                bytes = mCurrentBGM->ReadChunk(
                    mBgmDecodeBuffer.data(),
                    BGM_CHUNK_SIZE
                );

                if (bytes > 0)
                {
                    ALenum format = (mCurrentBGM->GetChannels() == 2)
                        ? AL_FORMAT_STEREO16
                        : AL_FORMAT_MONO16;

                    alBufferData(buf,
                                 format,
                                 mBgmDecodeBuffer.data(),
                                 static_cast<ALsizei>(bytes),
                                 static_cast<ALsizei>(mCurrentBGM->GetRate()));

                    alSourceQueueBuffers(mBgmSource, 1, &buf);
                }
            }
        }

        // ★ここを追加：バッファがキューされているのに止まっていたら再生し直す
        ALint state  = 0;
        ALint queued = 0;
        alGetSourcei(mBgmSource, AL_SOURCE_STATE,     &state);
        alGetSourcei(mBgmSource, AL_BUFFERS_QUEUED,   &queued);

        if (queued > 0 && state != AL_PLAYING)
        {
            alSourcePlay(mBgmSource);
        }
    }


    //====================
    // SE ソースの掃除
    //====================
    std::vector<ALuint> alive;
    alive.reserve(mOneShotSources.size());

    for (ALuint src : mOneShotSources)
    {
        ALint state = 0;
        alGetSourcei(src, AL_SOURCE_STATE, &state);

        if (state == AL_STOPPED)
        {
            alDeleteSources(1, &src);
        }
        else
        {
            alive.push_back(src);
        }
    }

    mOneShotSources.swap(alive);
}

} // namespace toy
