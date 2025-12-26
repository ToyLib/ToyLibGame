#include "Asset/Font/TextFont.h"
#include <iostream>

namespace toy {

TextFont::TextFont()
    : mFont(nullptr)
    , mFilePath("")
    , mPointSize(0)
{
}

TextFont::~TextFont()
{
    //Unload();
}

bool TextFont::Load(const std::string& filePath, int pointSize)
{
    // 既存フォントを解放（再ロード時の安全措置）
    Unload();

    // SDL_ttf（SDL3_ttf）ではサイズが float のためキャスト
    mFont = TTF_OpenFont(filePath.c_str(), static_cast<float>(pointSize));
    if (!mFont)
    {
        std::cerr << "TTF_OpenFont failed: " << SDL_GetError()
                  << " (file: " << filePath
                  << ", size: " << pointSize << ")"
                  << std::endl;
        return false;
    }

    mFilePath  = filePath;
    mPointSize = pointSize;
    return true;
}

void TextFont::Unload()
{
    if (mFont)
    {
        TTF_CloseFont(mFont);
        mFont = nullptr;
    }
}

} // namespace toy
