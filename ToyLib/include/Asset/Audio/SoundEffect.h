#pragma once
#include <string>
#include <vector>

#include <AL/al.h>
#include <AL/alc.h>

namespace toy {

//======================================================================
// SoundEffect
//   - 効果音(SE)用の単発再生アセット
//   - AssetManager によって管理される
//   - WAV(16bit PCM) をメモリに読み込み → OpenAL バッファ化する
//
//   ※ MP3 のようなストリーミングは行わず、全データ常駐方式。
//     SE は短いので高速 & 再生遅延ゼロ。
//======================================================================
class SoundEffect
{
public:
    SoundEffect();
    ~SoundEffect();

    //----------------------------------------------------------------------
    // Load()
    //   - WAV ファイルを読み込み
    //   - 16bit PCM を要求（エンディアンを含め WAV を手動解析）
    //   - OpenAL バッファに書き込む
    //----------------------------------------------------------------------
    bool Load(const std::string& fileName, class AssetManager* manager);
    
    void Unload();

    //----------------------------------------------------------------------
    // GetBuffer()
    //   - SoundMixer などが OpenAL Source にバインドして使用
    //----------------------------------------------------------------------
    ALuint GetBuffer() const { return mBuffer; }

private:
    //----------------------------------------------------------------------
    // LoadWav16()
    //   - WAV 解析（ヘッダチェック・チャンク解析・16bit PCM 取得）
    //   - 成功時：outData = PCM 生データ
    //             outFormat = AL_FORMAT_MONO16 / AL_FORMAT_STEREO16
    //             outFreq   = サンプルレート
    //----------------------------------------------------------------------
    bool LoadWav16(
        const std::string& fullPath,
        std::vector<char>& outData,
        ALenum& outFormat,
        ALsizei& outFreq
    );
    
private:
    ALuint      mBuffer;          // OpenAL バッファ
    std::string mFilePath;        // デバッグ・再読み込み用

};

} // namespace toy
