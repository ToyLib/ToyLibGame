#include "Asset/Material/Texture.h"

#include "Asset/AssetManager.h"

#include "Asset/Material/ITextureGPU.h"
#include "Asset/Material/GLTextureGPU.h"
#include "Asset/Material/VKTextureGPU.h"

#include "Render/RenderBackendState.h"

#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <vector>

namespace toy {

//============================================================
// 内部：Backend に応じて GPU 実装を生成
//============================================================
static std::unique_ptr<ITextureGPU> CreateTextureGPU()
{
    if (RenderBackendState::Get().IsGL())
    {
        return std::make_unique<GLTextureGPU>();
    }
    if (RenderBackendState::Get().IsVK())
    {
        return std::make_unique<VKTextureGPU>();
    }

    // Unknown の場合はまだ作れない（Initialize 前の可能性）
    return nullptr;
}

//============================================================
// 内部：必要なら GPU を作る（Unknown の場合は失敗）
//============================================================
static bool EnsureGPU(std::unique_ptr<ITextureGPU>& gpu)
{
    if (gpu)
    {
        return true;
    }

    gpu = CreateTextureGPU();
    if (!gpu)
    {
        std::cerr << "[Texture] EnsureGPU failed: backend is not ready (Unknown?)\n";
        return false;
    }
    return true;
}

//============================================================
// ctor / dtor
//============================================================
Texture::Texture()
{
    // ★ここでは作らない（Backend 未確定の可能性がある）
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
    if (!EnsureGPU(mGPU))
    {
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

    // まず RGBA とみなせる形式へ（ABGR8888）
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

    // ABGR8888 は little endian 環境で RGBA 相当として扱える運用
    const bool ok = mGPU->CreateFromPixels(conv->pixels, mWidth, mHeight, true);

    SDL_DestroySurface(conv);

    if (!ok)
    {
        // GPU側が失敗したらサイズも戻す
        mWidth = 0;
        mHeight = 0;
    }

    return ok;
}

//============================================================
// LoadFromMemory : compressed image in memory
//============================================================
bool Texture::LoadFromMemory(const void* data, int size)
{
    if (!EnsureGPU(mGPU))
    {
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

    // 安全側：常に ABGR8888 に変換して GPU へ
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

    if (!ok)
    {
        mWidth = 0;
        mHeight = 0;
    }

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
    if (!EnsureGPU(mGPU))
    {
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
//============================================================
unsigned int Texture::GetTextureID() const
{
    if (!mGPU) return 0;

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
    if (!EnsureGPU(mGPU)) return false;
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

    const bool ok = mGPU->CreateFromPixels(pixels.data(), size, size, true);
    if (!ok)
    {
        mWidth = 0;
        mHeight = 0;
    }
    return ok;
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
    if (!EnsureGPU(mGPU)) return false;
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

    const bool ok = mGPU->CreateFromPixels(pixels.data(), size, size, true);
    if (!ok)
    {
        mWidth = 0;
        mHeight = 0;
    }
    return ok;
}

//============================================================
// CreateRenderColorRGBA8
//============================================================
void Texture::CreateRenderColorRGBA8(int w, int h)
{
    if (!EnsureGPU(mGPU))
    {
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
    if (!EnsureGPU(mGPU))
    {
        return false;
    }
    if (!pixels || width <= 0 || height <= 0)
    {
        std::cerr << "[Texture] CreateFromPixels invalid args\n";
        return false;
    }

    mWidth  = width;
    mHeight = height;

    const bool ok = mGPU->CreateFromPixels(pixels, width, height, hasAlpha);
    if (!ok)
    {
        mWidth = 0;
        mHeight = 0;
    }
    return ok;
}

//============================================================
// SetActive
//============================================================
void Texture::SetActive(int unit)
{
    // VK では no-op 実装でも OK（descriptor bind が本命）
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
        // ★Texture は “再利用” される可能性があるので GPU オブジェクト自体は保持してOK
        // ただし backend 切替の可能性を完全に排除したいなら reset() でも良い
        // mGPU.reset();
    }

    mWidth  = 0;
    mHeight = 0;
}

} // namespace toy
