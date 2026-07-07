#pragma once

#include "stb_vorbis.h"

#include <string>
#include <cstddef>

struct stb_vorbis;

namespace toy {

class Music
{
public:
    Music();
    ~Music();

    bool Load(const std::string& fileName, class AssetManager* manager);
    void Unload();

    void Rewind();

    // デコード済み PCM を out に書き込む
    // 返値は実際に読み込んだバイト数
    size_t ReadChunk(unsigned char* out, size_t chunkSize);

    long GetRate() const     { return mRate; }
    int  GetChannels() const { return mChannels; }

    const std::string& GetFilePath() const { return mFilePath; }

private:
    std::string mFilePath;

    // Ogg Vorbis decoder
    stb_vorbis* mHandle { nullptr };

    long mRate { 0 };
    int  mChannels { 0 };
};

} // namespace toy
