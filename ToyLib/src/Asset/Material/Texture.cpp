#include "Asset/Material/Texture.h"

#include "Asset/AssetManager.h"
#include "Render/ITextureGPU.h"
#include "Render/GL/GLTextureGPU.h"

#include "Render/RenderBackendState.h"

#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <vector>

namespace toy {

//============================================================
// 内部：いまは GL 固定で生成（VK導入時に差し替えポイント）
//============================================================
static std::unique_ptr<ITextureGPU> CreateTextureGPU()
{
    if (RenderBackendState::Get().IsGL())
    {
        // 今は GL だけ
        return std::make_unique<GLTextureGPU>();
    }
    return nullptr;
}

//============================================================
// ctor / dtor
//============================================================
Texture::Texture()
{
    mGPU = CreateTextureGPU();
}

Texture::~Texture()
{
    Unload();
}

//============================================================
// Load : image file (SDL3_image)
//============================================================
bool Texture::Load(const std::string& fileName, AssetManager* assetManager)
{
    if (!assetManager)
    {
        std::cerr << "[Texture] Load failed: assetManager is null\n";
        return false;
    }
    if (!mGPU)
    {
        std::cerr << "[Texture] Load failed: GPU is null\n";
        return false;
    }

    // AssetsPath を基準にフルパスを組み立てる
    std::string fullName = assetManager->GetAssetsPath() + fileName;

    SDL_Surface* image = IMG_Load(fullName.c_str());
    if (!image)
    {
        std::cerr << "[Texture] Failed to load image: "
                  << fullName << " : " << SDL_GetError() << "\n";
        return false;
    }

    // GL で扱いやすいフォーマットへ（ABGR8888）
    SDL_Surface* conv = SDL_ConvertSurface(image, SDL_PIXELFORMAT_ABGR8888);
    SDL_DestroySurface(image);

    if (!conv)
    {
        std::cerr << "[Texture] SDL_ConvertSurface failed: "
                  << SDL_GetError() << "\n";
        return false;
    }

    mWidth  = conv->w;
    mHeight = conv->h;

    // ABGR8888 は little endian で RGBA と互換扱いできるので「hasAlpha=true」でOK
    const bool ok = mGPU->CreateFromPixels(conv->pixels, mWidth, mHeight, true);

    SDL_DestroySurface(conv);
    return ok;
}

//============================================================
// LoadFromMemory : compressed image in memory
//============================================================
bool Texture::LoadFromMemory(const void* data, int size)
{
    if (!mGPU)
    {
        std::cerr << "[Texture] LoadFromMemory failed: GPU is null\n";
        return false;
    }
    if (!data || size <= 0)
    {
        std::cerr << "[Texture] LoadFromMemory failed: invalid args\n";
        return false;
    }

    SDL_IOStream* io = SDL_IOFromConstMem(data, size);
    if (!io)
    {
        std::cerr << "[Texture] SDL_IOFromConstMem failed: "
                  << SDL_GetError() << "\n";
        return false;
    }

    SDL_Surface* image = IMG_Load_IO(io, true);
    if (!image)
    {
        std::cerr << "[Texture] IMG_Load_IO failed: "
                  << SDL_GetError() << "\n";
        return false;
    }

    const bool hasAlpha = SDL_ISPIXELFORMAT_ALPHA(image->format);
    mWidth  = image->w;
    mHeight = image->h;

    // ここは SDL_Surface の format に応じて「RGB/RGBA」どちらで解釈するかだけ決める
    // ※image->pixels の並びは format に依存するので、厳密にやるなら変換が必要。
    // ただ「既存コードの挙動を崩さない」が最優先なら、従来通りの運用でOK。
    // ここでは安全側で RGBA へ変換してから投げる（破綻しづらい）。
    SDL_Surface* conv = SDL_ConvertSurface(image, SDL_PIXELFORMAT_ABGR8888);
    SDL_DestroySurface(image);

    if (!conv)
    {
        std::cerr << "[Texture] SDL_ConvertSurface failed: "
                  << SDL_GetError() << "\n";
        return false;
    }

    mWidth  = conv->w;
    mHeight = conv->h;

    const bool ok = mGPU->CreateFromPixels(conv->pixels, mWidth, mHeight, true);

    SDL_DestroySurface(conv);
    (void)hasAlpha; // 情報としては取ってるが、ここでは統一でRGBA化している
    return ok;
}

//============================================================
// LoadFromMemory : raw pixels RGBA
//============================================================
bool Texture::LoadFromMemory(const void* data, int width, int height)
{
    // 既存運用：RGBA 前提
    return CreateFromPixels(data, width, height, true);
}

//============================================================
// CreateShadowMap
//============================================================
void Texture::CreateShadowMap(int width, int height)
{
    if (!mGPU)
    {
        std::cerr << "[Texture] CreateShadowMap failed: GPU is null\n";
        return;
    }

    mWidth  = width;
    mHeight = height;
    mGPU->CreateShadowMap(width, height);
}

//============================================================
// NOTE: OpenGL backend only.
// RenderTarget(GL) など GL直結コードから使うための暫定アクセサ。
// Vulkan backend では 0 を返す or 別APIになる想定。
unsigned int Texture::GetTextureID() const
{
    // GL のときだけ値が取れる
    if (auto* gl = dynamic_cast<GLTextureGPU*>(mGPU.get()))
    {
        return gl->GetTextureID();
    }
    return 0;
}

//============================================================
// CreateAlphaCircle
//============================================================
bool Texture::CreateAlphaCircle(int size,
                                float centerX,
                                float centerY,
                                Vector3 color,
                                float blendPow)
{
    if (!mGPU) return false;
    if (size <= 0) return false;

    std::vector<uint8_t> pixels(size * size * 4);

    float cx = centerX * size;
    float cy = centerY * size;

    for (int y = 0; y < size; ++y)
    {
        for (int x = 0; x < size; ++x)
        {
            float dx = x - cx;
            float dy = y - cy;
            float dist = std::sqrt(dx * dx + dy * dy) / (size / 3.0f);

            float alpha = 1.0f - std::pow(std::clamp(dist, 0.0f, 1.0f), blendPow);

            int index = (y * size + x) * 4;
            pixels[index + 0] = static_cast<uint8_t>(255.0f * color.x);
            pixels[index + 1] = static_cast<uint8_t>(255.0f * color.y);
            pixels[index + 2] = static_cast<uint8_t>(255.0f * color.z);
            pixels[index + 3] = static_cast<uint8_t>(alpha * 255.0f);
        }
    }

    mWidth  = size;
    mHeight = size;
    return mGPU->CreateFromPixels(pixels.data(), size, size, true);
}

//============================================================
// CreateRadialRays
//============================================================
bool Texture::CreateRadialRays(int size,
                               int numRays,
                               float fadePow,
                               float rayStrength,
                               float intensityScale)
{
    if (!mGPU) return false;
    if (size <= 0 || numRays <= 0) return false;

    std::vector<uint8_t> pixels(size * size * 4);

    float cx = size * 0.5f;
    float cy = size * 0.5f;
    float maxDist = size * 0.5f;

    for (int y = 0; y < size; ++y)
    {
        for (int x = 0; x < size; ++x)
        {
            float dx = x - cx;
            float dy = y - cy;
            float dist = std::sqrt(dx * dx + dy * dy) / maxDist;

            float angle = std::atan2(dy, dx);
            float ray   = std::abs(std::sin(angle * numRays));

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

    mWidth  = size;
    mHeight = size;
    return mGPU->CreateFromPixels(pixels.data(), size, size, true);
}

//============================================================
// CreateRenderColorRGBA8
//============================================================
void Texture::CreateRenderColorRGBA8(int w, int h)
{
    if (!mGPU)
    {
        std::cerr << "[Texture] CreateRenderColorRGBA8 failed: GPU is null\n";
        return;
    }

    mWidth  = w;
    mHeight = h;
    mGPU->CreateRenderColorRGBA8(w, h);
}

//============================================================
// CreateFromPixels (public API 維持)
//============================================================
bool Texture::CreateFromPixels(const void* pixels, int width, int height, bool hasAlpha)
{
    if (!mGPU)
    {
        std::cerr << "[Texture] CreateFromPixels failed: GPU is null\n";
        return false;
    }
    if (!pixels || width <= 0 || height <= 0)
    {
        std::cerr << "[Texture] CreateFromPixels invalid args\n";
        return false;
    }

    mWidth  = width;
    mHeight = height;
    return mGPU->CreateFromPixels(pixels, width, height, hasAlpha);
}

//============================================================
// SetActive
//============================================================
void Texture::SetActive(int unit)
{
    if (!mGPU) return;
    mGPU->SetActive(unit);
}

//============================================================
// Unload
//============================================================
void Texture::Unload()
{
    if (mGPU)
    {
        mGPU->Unload();
    }
    mWidth  = 0;
    mHeight = 0;
}

} // namespace toy
