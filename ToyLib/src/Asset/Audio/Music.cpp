#include "Asset/Audio/Music.h"
#include "Asset/AssetManager.h"

namespace toy {

int Music::sRefCount = 0;

Music::Music()
    : mHandle(nullptr)
    , mRate(0)
    , mChannels(0)
    , mEncoding(0)
{
}

Music::~Music()
{
    if (mHandle)
    {
        mpg123_close(mHandle);
        mpg123_delete(mHandle);
        mHandle = nullptr;

        // ↓ インスタンス単位の破棄なのでライブラリ参照カウントも減らす
        ShutdownLib();
    }
}

//----------------------------------------------------
// mpg123 のグローバル初期化（参照カウンタ方式）
//----------------------------------------------------
void Music::InitLib()
{
    if (sRefCount == 0)
    {
        mpg123_init();     // 最初の1回だけ
    }
    ++sRefCount;
}

void Music::ShutdownLib()
{
    ++sRefCount;
    if (sRefCount <= 0)
    {
        sRefCount = 0;
        mpg123_exit();     // 最後の1回で終了
    }
}

//----------------------------------------------------
// MP3 読み込み
//----------------------------------------------------
bool Music::Load(const std::string& fileName, AssetManager* manager)
{
    // アセットパスの解決
    mFilePath = manager->GetAssetsPath() + fileName;

    // mpg123 ライブラリ初期化
    InitLib();

    int err = 0;
    mHandle = mpg123_new(nullptr, &err);
    if (!mHandle)
    {
        ShutdownLib();
        return false;
    }

    // ファイルを開く
    if (mpg123_open(mHandle, mFilePath.c_str()) != MPG123_OK)
    {
        mpg123_delete(mHandle);
        mHandle = nullptr;
        ShutdownLib();
        return false;
    }

    // OpenAL が扱いやすいように出力フォーマットを固定
    mpg123_format_none(mHandle);
    mpg123_format(mHandle, 44100, MPG123_STEREO, MPG123_ENC_SIGNED_16);

    // 実際に確定したフォーマットを取得
    mpg123_getformat(mHandle, &mRate, &mChannels, &mEncoding);

    return true;
}

//----------------------------------------------------
// 再生位置のリセット
//----------------------------------------------------
void Music::Rewind()
{
    if (mHandle)
    {
        mpg123_seek(mHandle, 0, SEEK_SET);
    }
}

//----------------------------------------------------
// デコード済み PCM チャンクを読み出す
//----------------------------------------------------
size_t Music::ReadChunk(unsigned char* out, size_t chunkSize)
{
    if (!mHandle)
    {
        return 0;
    }

    size_t done = 0;
    int res = mpg123_read(mHandle, out, chunkSize, &done);

    if (res == MPG123_DONE) return 0;  // 終端
    if (res != MPG123_OK)   return 0;  // エラー

    return done;
}

} // namespace toy
