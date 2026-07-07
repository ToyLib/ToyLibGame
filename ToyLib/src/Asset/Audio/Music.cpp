#include "Asset/Audio/Music.h"
#include "Asset/AssetManager.h"

// stb_vorbis.c を単体でビルドしない場合はこちらを使う
#define STB_VORBIS_IMPLEMENTATION
#include "stb_vorbis.c"

namespace toy {

Music::Music()
{
}

Music::~Music()
{
    Unload();
}

bool Music::Load(const std::string& fileName, AssetManager* manager)
{
    Unload();

    mFilePath = manager->GetAssetsPath() + fileName;

    int error = 0;
    mHandle = stb_vorbis_open_filename(
        mFilePath.c_str(),
        &error,
        nullptr
    );

    if (!mHandle)
    {
        mRate = 0;
        mChannels = 0;
        return false;
    }

    stb_vorbis_info info = stb_vorbis_get_info(mHandle);

    mRate = static_cast<long>(info.sample_rate);
    mChannels = info.channels;

    return true;
}

void Music::Unload()
{
    if (mHandle)
    {
        stb_vorbis_close(mHandle);
        mHandle = nullptr;
    }

    mRate = 0;
    mChannels = 0;
    mFilePath.clear();
}

void Music::Rewind()
{
    if (mHandle)
    {
        stb_vorbis_seek_start(mHandle);
    }
}

size_t Music::ReadChunk(unsigned char* out, size_t chunkSize)
{
    if (!mHandle || !out || chunkSize == 0)
    {
        return 0;
    }

    // 16bit signed PCM として読み出す
    const int bytesPerSample = static_cast<int>(sizeof(short));

    // stb_vorbis_get_samples_short_interleaved の第4引数は
    // 「short要素数」であり、バイト数ではない
    int maxShorts = static_cast<int>(chunkSize / bytesPerSample);

    if (maxShorts <= 0)
    {
        return 0;
    }

    int framesRead = stb_vorbis_get_samples_short_interleaved(
        mHandle,
        mChannels,
        reinterpret_cast<short*>(out),
        maxShorts
    );

    if (framesRead <= 0)
    {
        return 0;
    }

    // framesRead は「チャンネル単位ではないサンプルフレーム数」
    return static_cast<size_t>(framesRead)
         * static_cast<size_t>(mChannels)
         * sizeof(short);
}

} // namespace toy
