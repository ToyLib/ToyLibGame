// Audio/SoundComponent.h
#pragma once
#include "Engine/Core/Component.h"
#include <string>

#include <AL/al.h>
#include <AL/alc.h>

namespace toy {
//----------------------------------------------
// SoundComponent
//  - Actor に取り付けて使用する 3D/2D サウンド再生コンポーネント
//  - OpenAL の "ソース" を 1 つ保持
//  - 距離減衰やループ、オート再生、排他制御などをサポート
//----------------------------------------------
class SoundComponent : public Component
{
public:
    SoundComponent(class Actor* owner, int updateOrder = 100);
    ~SoundComponent();
    
    // 再生する効果音ファイル（AssetManager 経由でロード）
    void SetSound(const std::string& fileName);
    
    // 再生 / 停止 / 状態チェック
    void Play();
    void Stop();
    bool IsPlaying() const;
    
    // 基本設定
    void SetVolume(float volume) { mVolume = volume; }
    void SetLoop(bool loop) { mIsLoop = loop; }
    void SetAutoPlay(bool autoPlay) { mAutoPlay = autoPlay; }
    
    // 3D 音響設定
    //  - useAttenuation = true の場合、Actor のワールド位置から距離減衰を行う
    void Enable3DSound(bool use3DSound) { mUse3DSound = use3DSound; }
    
    // 排他モード
    //  - true の場合、他の SoundComponent が同じ音を再生中なら再生しない
    void SetExclusive(bool isExclusive) { mIsExclusive = isExclusive; }
    
    // 毎フレーム呼ばれる（Actor の位置を反映し、減衰計算など）
    void Update(float deltaTime) override;
    
private:
    // 再生するサウンド名（AssetManager のキーになる）
    std::string mSoundName;
    
    // ボリューム（0.0〜1.0）
    float mVolume       { 1.0f };

    bool  mIsLoop       { false };      // ループ再生するか
    bool  mAutoPlay     { false };      // 生成直後に自動再生するか
    bool  mUse3DSound   { false };      // 3Dサウンド（距離減衰を使うか)
    bool  mIsExclusive  { false };      // 排他モード

    bool  mHasPlayed    { false };      // AutoPlay 用フラグ

    ALuint mSource      {};             // OpenAL ソース（このコンポ用）
};

} // namespace toy
