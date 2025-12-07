#include "Asset/Material/Texture.h"
#include "Asset/AssetManager.h"
#include "glad/glad.h"

#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>

#include <vector>
#include <cmath>
#include <algorithm>
#include <iostream>

namespace toy {

Texture::Texture()
: mTextureID(0)
, mWidth(0)
, mHeight(0)
{
}

Texture::~Texture()
{
    Unload();
}

//============================================================
// 画像ファイル読み込み（SDL3_image）
//============================================================
bool Texture::Load(const std::string& fileName, AssetManager* assetManager)
{
    // AssetManager で設定された AssetsPath を基準にフルパスを組み立てる
    std::string fullName = assetManager->GetAssetsPath() + fileName;

    SDL_Surface* image = IMG_Load(fullName.c_str());
    if (!image)
    {
        std::cerr << "[Texture] Failed to load image: "
                  << fullName << " : " << SDL_GetError() << std::endl;
        return false;
    }

    // --------------------------------------------------------
    // 1) OpenGL 用フォーマットへ変換（ABGR8888 → RGBA 相当）
    //    ※ SDL3 でも SDL_ConvertSurface は利用可能
    // --------------------------------------------------------
    SDL_Surface* conv = SDL_ConvertSurface(image, SDL_PIXELFORMAT_ABGR8888);
    SDL_DestroySurface(image); // 元 surface は破棄

    if (!conv)
    {
        std::cerr << "[Texture] SDL_ConvertSurface failed: "
                  << SDL_GetError() << std::endl;
        return false;
    }

    const int w = conv->w;
    const int h = conv->h;

    // --------------------------------------------------------
    // 2) 行パディング対策
    //    UNPACK_ALIGNMENT=1 にして 4byte アライメントを気にしないようにする
    // --------------------------------------------------------
    GLint prevUnpack = 0;
    glGetIntegerv(GL_UNPACK_ALIGNMENT, &prevUnpack);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    // --------------------------------------------------------
    // 3) OpenGL テクスチャ生成
    //    ABGR8888 だが little endian では RGBA 順と互換になるため GL_RGBA で扱う
    // --------------------------------------------------------
    glGenTextures(1, &mTextureID);
    glBindTexture(GL_TEXTURE_2D, mTextureID);

    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RGBA8,          // 内部フォーマット
        w,
        h,
        0,
        GL_RGBA,           // 入力フォーマット
        GL_UNSIGNED_BYTE,
        conv->pixels
    );

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // 元の alignment に戻す
    glPixelStorei(GL_UNPACK_ALIGNMENT, prevUnpack);

    mWidth  = w;
    mHeight = h;

    SDL_DestroySurface(conv);
    return true;
}

//============================================================
// メモリ上の画像データから読み込み（埋め込みテクスチャなど）
//   - Assimp の aiTexture などに対応
//============================================================
bool Texture::LoadFromMemory(const void* data, int size)
{
    // SDL3: SDL_RWops の代わりに SDL_IOStream を使用
    SDL_IOStream* io = SDL_IOFromConstMem(data, size);
    if (!io)
    {
        std::cerr << "[Texture] SDL_IOFromConstMem failed: "
                  << SDL_GetError() << std::endl;
        return false;
    }

    // SDL3_image: IMG_Load_IO
    //   第二引数 true で、読み込み終了後に io を自動クローズ
    SDL_Surface* image = IMG_Load_IO(io, true);
    if (!image)
    {
        std::cerr << "[Texture] Failed to load image from memory: "
                  << SDL_GetError() << std::endl;
        return false;
    }

    // SDL3: ピクセルフォーマットからアルファ有無を判定
    bool hasAlpha = SDL_ISPIXELFORMAT_ALPHA(image->format);
    GLenum srcFormat = hasAlpha ? GL_RGBA : GL_RGB;
    GLenum internal  = hasAlpha ? GL_RGBA8 : GL_RGB8;

    glGenTextures(1, &mTextureID);
    glBindTexture(GL_TEXTURE_2D, mTextureID);

    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        internal,
        image->w,
        image->h,
        0,
        srcFormat,
        GL_UNSIGNED_BYTE,
        image->pixels
    );

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    mWidth  = image->w;
    mHeight = image->h;

    SDL_DestroySurface(image);
    return true;
}

//============================================================
// 生のピクセルから作成（RGBA 前提）
//   - フォントレンダリング結果などをそのままテクスチャ化
//============================================================
bool Texture::LoadFromMemory(const void* data, int width, int height)
{
    if (mTextureID != 0)
    {
        glDeleteTextures(1, &mTextureID);
    }

    glGenTextures(1, &mTextureID);
    glBindTexture(GL_TEXTURE_2D, mTextureID);

    glTexImage2D(
        GL_TEXTURE_2D, 0, GL_RGBA8,
        width, height, 0,
        GL_RGBA, GL_UNSIGNED_BYTE,
        data
    );

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    mWidth  = width;
    mHeight = height;
    return true;
}

//============================================================
// レンダリングターゲット用テクスチャ（カラー）
//   - ポストエフェクト用 FBO のアタッチ先などで使用
//============================================================
void Texture::CreateForRendering(int w, int h, unsigned int format)
{
    mWidth  = w;
    mHeight = h;

    glGenTextures(1, &mTextureID);
    glBindTexture(GL_TEXTURE_2D, mTextureID);

    glTexImage2D(
        GL_TEXTURE_2D, 0, format,
        mWidth, mHeight, 0,
        GL_RGB, GL_FLOAT,
        nullptr
    );

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
}

//============================================================
// シャドウマップ用テクスチャ（depth）
//   - sampler2DShadow 前提の深度比較テクスチャ
//============================================================
void Texture::CreateShadowMap(int width, int height)
{
    mWidth  = width;
    mHeight = height;

    glGenTextures(1, &mTextureID);
    glBindTexture(GL_TEXTURE_2D, mTextureID);

    glTexImage2D(
        GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24,
        width, height, 0,
        GL_DEPTH_COMPONENT, GL_FLOAT,
        nullptr
    );

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
}

//============================================================
// 自前生成（レンズフレア用などの円形グラデーション）
//============================================================
bool Texture::CreateAlphaCircle(int size,
                                float centerX,
                                float centerY,
                                Vector3 color,
                                float blendPow)
{
    if (size <= 0) return false;

    std::vector<uint8_t> pixels(size * size * 4);

    float cx = centerX * size;
    float cy = centerY * size;

    for (int y = 0; y < size; y++)
    {
        for (int x = 0; x < size; x++)
        {
            float dx = x - cx;
            float dy = y - cy;
            float dist = std::sqrt(dx * dx + dy * dy) / (size / 3.0f);

            // 外側に行くほどアルファが減衰
            float alpha = 1.0f - std::pow(std::clamp(dist, 0.0f, 1.0f), blendPow);

            int index = (y * size + x) * 4;
            pixels[index + 0] = static_cast<uint8_t>(255.f * color.x);
            pixels[index + 1] = static_cast<uint8_t>(255.f * color.y);
            pixels[index + 2] = static_cast<uint8_t>(255.f * color.z);
            pixels[index + 3] = static_cast<uint8_t>(alpha * 255);
        }
    }

    glGenTextures(1, &mTextureID);
    glBindTexture(GL_TEXTURE_2D, mTextureID);

    glTexImage2D(
        GL_TEXTURE_2D, 0, GL_RGBA,
        size, size, 0,
        GL_RGBA, GL_UNSIGNED_BYTE,
        pixels.data()
    );

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    mWidth  = size;
    mHeight = size;
    return true;
}

//============================================================
// 自前生成（放射状の光芒・ゴッドレイ風テクスチャ）
//============================================================
bool Texture::CreateRadialRays(int size,
                               int numRays,
                               float fadePow,
                               float rayStrength,
                               float intensityScale)
{
    if (size <= 0 || numRays <= 0) return false;

    std::vector<uint8_t> pixels(size * size * 4);

    float cx = size * 0.5f;
    float cy = size * 0.5f;
    float maxDist = size * 0.5f;

    for (int y = 0; y < size; y++)
    {
        for (int x = 0; x < size; x++)
        {
            float dx = x - cx;
            float dy = y - cy;
            float dist = std::sqrt(dx * dx + dy * dy) / maxDist;

            float angle = std::atan2(dy, dx);
            float ray   = std::abs(std::sin(angle * numRays));

            // 距離減衰 × 光芒の強さ
            float alpha = (1.0f - std::clamp(dist, 0.0f, 1.0f));
            alpha = std::pow(alpha, fadePow) * ray * rayStrength;
            alpha = std::clamp(alpha * intensityScale, 0.0f, 1.0f);

            int index = (y * size + x) * 4;
            pixels[index + 0] = static_cast<uint8_t>(alpha * 255);
            pixels[index + 1] = static_cast<uint8_t>(alpha * 255);
            pixels[index + 2] = static_cast<uint8_t>(alpha * 200);
            pixels[index + 3] = static_cast<uint8_t>(alpha * 255);
        }
    }

    glGenTextures(1, &mTextureID);
    glBindTexture(GL_TEXTURE_2D, mTextureID);

    glTexImage2D(
        GL_TEXTURE_2D, 0, GL_RGBA,
        size, size, 0,
        GL_RGBA, GL_UNSIGNED_BYTE,
        pixels.data()
    );

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    mWidth  = size;
    mHeight = size;
    return true;
}

//============================================================
// 任意ピクセル列 → テクスチャ
//   - hasAlpha=false の場合は RGB テクスチャとして扱う
//============================================================
bool Texture::CreateFromPixels(const void* pixels,
                               int width, int height, bool hasAlpha)
{
    if (mTextureID != 0)
    {
        glDeleteTextures(1, &mTextureID);
        mTextureID = 0;
    }

    mWidth  = width;
    mHeight = height;

    glGenTextures(1, &mTextureID);
    glBindTexture(GL_TEXTURE_2D, mTextureID);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    GLenum format = hasAlpha ? GL_RGBA : GL_RGB;

    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        format,
        width,
        height,
        0,
        format,
        GL_UNSIGNED_BYTE,
        pixels
    );

    glBindTexture(GL_TEXTURE_2D, 0);
    return true;
}

//============================================================
// OpenGL へのバインド
//============================================================
void Texture::SetActive(int unit)
{
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(GL_TEXTURE_2D, mTextureID);
}

//============================================================
// リソース解放
//============================================================
void Texture::Unload()
{
    if (mTextureID != 0)
    {
        glDeleteTextures(1, &mTextureID);
        mTextureID = 0;
    }
}

} // namespace toy
