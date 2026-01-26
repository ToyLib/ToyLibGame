// Asset/Audio/Music.h
#pragma once
#include <string>
#include <cstddef>
#include <memory>

#include <mpg123.h>

namespace toy {

//======================================================================
// Music
//   - BGM 再生専用 MP3 アセット
//   - AssetManager が管理し、SoundMixer（または AudioSystem）が使用
//   - MP3 デコーダー mpg123 を内部に保持してストリーミング再生を行う
//
//   ※ mp3 はファイル全体を decode して持たず、部分的に ReadChunk()
//      する「ストリーミング方式」なので、大容量BGMでもメモリ効率が高い
//======================================================================
class Music
{
public:
    Music();
    ~Music();
    
    //----------------------------------------------------------------------
    // Load()
    //   - AssetManager が呼ぶ
    //   - mpg123 の mp3 デコーダーを開き、サンプルレート・チャンネル数を取得
    //----------------------------------------------------------------------
    bool Load(const std::string& fileName, class AssetManager* manager);
    
    void Unload();
    
    //----------------------------------------------------------------------
    // Rewind()
    //   - 再生位置を先頭に戻す（ループ再生時に使用）
    //----------------------------------------------------------------------
    void Rewind();
    
    //----------------------------------------------------------------------
    // ReadChunk()
    //   - デコード済みPCMを一部だけ out に書き込む
    //   - chunkSize 分だけ読み込む
    //   - 返値：実際に読み込まれたバイト数（0なら終端）
    //   - SoundMixer がストリーミング再生用として利用
    //----------------------------------------------------------------------
    size_t ReadChunk(unsigned char* out, size_t chunkSize);
    
    // 情報取得系
    long GetRate() const     { return mRate; }      // 例：44100
    int  GetChannels() const { return mChannels; }  // 例：2 (stereo)
    
    const std::string& GetFilePath() const { return mFilePath; }
    
private:
    //----------------------------------------------------------------------
    // mpg123 のグローバル初期化は一度だけ必要。
    // ToyLib 全体で複数の Music オブジェクトが使われることを考え、
    // sRefCount により参照カウント方式で初期化/終了を管理する。
    //----------------------------------------------------------------------
    static int  sRefCount;
    static void InitLib();
    static void ShutdownLib();
    
private:
    std::string    mFilePath;
    mpg123_handle* mHandle { nullptr };
    
    long           mRate     { 0 };   // サンプルレート
    int            mChannels { 0 };   // モノラル or ステレオ
    int            mEncoding { 0 };   // mpg123 の内部エンコード（PCM 形式）
    
};

} // namespace toy
