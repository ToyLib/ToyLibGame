// Audio/SoundMixer.h
#pragma once

#include <string>
#include <vector>
#include <memory>

#include <AL/al.h>
#include <AL/alc.h>

#include "Utils/MathUtil.h"      // Matrix4 / Vector3

namespace toy {

//--------------------------------------
// サウンド制御クラス
//  - OpenAL を使った BGM/SE ミキサー
//  - AssetManager から Music/SoundEffect を取得して再生
//  - BGM はストリーミング再生、SE はワンショット再生を想定
//--------------------------------------
class SoundMixer
{
public:
    // AssetManager は外部から渡す（所有権は持たない）
    SoundMixer(class AssetManager* assetManager);
    ~SoundMixer();
    
    // 有効/無効の切り替え（BGM/SE）
    void SetBGMEnable(bool enable);
    void SetSoundEnable(bool enable);
    
    // BGMボリューム（0.0〜1.0 を想定）
    void SetBgmVolume(float volume);
    void SetMasterVolume(float volume);
    
    // BGM 読み込み＆制御
    bool LoadBGM(const std::string& fileName);
    void PlayBGM();
    void StopBGM();
    
    // 効果音のワンショット再生
    void PlaySoundEffect(const std::string& fileName);
    
    // 毎フレーム呼び出し
    //  - BGM のストリーミング更新
    //  - カメラ位置（invViewMatrix）からリスナー位置・向きを更新
    void Update(float deltaTime, const Matrix4& invViewMatrix);
    
    float GetMasterVolume() const { return mMasterVolume; }
    
private:
    // OpenAL 初期化/終了
    void InitOpenAL();
    void ShutdownOpenAL();
    
    // BGM 用ソース/バッファの初期化/破棄
    void InitBGMSource();
    void ShutdownBGMSource();
    
private:
    class AssetManager* mAssetManager { nullptr };   // アセット取得用ポインタ（非所有）
    
    // OpenAL デバイス／コンテキスト
    ALCdevice*  mDevice   { nullptr };
    ALCcontext* mContext  { nullptr };

    // 効果音（ワンショット再生用ソース群）
    std::vector<ALuint> mOneShotSources;

    // --- BGM ストリーミング用設定 ---
    static constexpr int BGM_NUM_BUFFERS = 4;          // ローテーションするバッファ数
    static constexpr int BGM_CHUNK_SIZE  = 32 * 1024;  // 一度にデコードするバイト数

    ALuint mBgmSource { 0 };                           // BGM 再生用のソース
    ALuint mBgmBuffers[BGM_NUM_BUFFERS] {};            // BGM 用バッファ群

    bool  mBgmPlaying { false };                       // 再生中フラグ
    bool  mBgmLoop    { true };                        // ループ再生するか

    std::shared_ptr<class Music> mCurrentBGM;          // 現在再生中の BGM
    std::vector<unsigned char> mBgmDecodeBuffer;       // デコード先バッファ（mpg123 → PCM）

    // 共通設定（ミュート系／音量）
    bool  mBgmEnabled   { true };                      // BGM 全体の ON/OFF
    bool  mSoundEnabled { true };                      // 効果音 全体の ON/OFF
    float mBgmVolume       { 1.0f };                      // BGM全体ボリューム
    float mMasterVolume    { 1.0f };
};

} // namespace toy
