//==============================================================================
// Renderer_TextProject.cpp
//  - CreateTextTexture (multi-line)
//  - WorldToScreen
//==============================================================================

#include "Engine/Render/Renderer.h"

#include "Asset/Font/TextFont.h"
#include "Asset/Material/Texture.h"

#include <algorithm>
#include <iostream>
#include <string>
#include <vector>
#include <cmath> // isfinite

namespace toy {

//=============================================================
// テキスト → テクスチャ生成（SDL3_ttf）改行対応
//=============================================================
std::shared_ptr<Texture> IRenderer::CreateTextTexture(
    const std::string& text,
    const Vector3& color,
    std::shared_ptr<TextFont> font)
{
    if (!font || !font->IsValid())
    {
        std::cerr << "[Renderer] CreateTextTexture: invalid font" << std::endl;
        return nullptr;
    }
    if (text.empty())
    {
        return nullptr;
    }

    TTF_Font* nativeFont = font->GetNativeFont();

    SDL_Color sdlColor;
    sdlColor.r = static_cast<Uint8>(std::clamp(color.x, 0.0f, 1.0f) * 255.0f);
    sdlColor.g = static_cast<Uint8>(std::clamp(color.y, 0.0f, 1.0f) * 255.0f);
    sdlColor.b = static_cast<Uint8>(std::clamp(color.z, 0.0f, 1.0f) * 255.0f);
    sdlColor.a = 255;

    // 1行
    if (text.find('\n') == std::string::npos)
    {
        SDL_Surface* surface = TTF_RenderText_Blended(
            nativeFont,
            text.c_str(),
            static_cast<int>(text.size()),
            sdlColor
        );
        if (!surface)
        {
            std::cerr << "[Renderer] TTF_RenderText_Blended failed: "
                      << SDL_GetError() << std::endl;
            return nullptr;
        }

        SDL_Surface* conv = SDL_ConvertSurface(surface, SDL_PIXELFORMAT_RGBA32);
        SDL_DestroySurface(surface);

        if (!conv)
        {
            std::cerr << "[Renderer] SDL_ConvertSurface failed: "
                      << SDL_GetError() << std::endl;
            return nullptr;
        }

        auto tex = std::make_shared<Texture>();
        if (!tex->CreateFromPixels(conv->pixels, conv->w, conv->h, true))
        {
            SDL_DestroySurface(conv);
            std::cerr << "[Renderer] CreateFromPixels failed" << std::endl;
            return nullptr;
        }

        SDL_DestroySurface(conv);
        return tex;
    }

    // 複数行
    std::vector<std::string> lines;
    {
        std::string current;
        for (char c : text)
        {
            if (c == '\n')
            {
                lines.push_back(current);
                current.clear();
            }
            else
            {
                current += c;
            }
        }
        lines.push_back(current);
    }

    std::vector<SDL_Surface*> lineSurfaces;
    lineSurfaces.reserve(lines.size());

    int maxW   = 0;
    int totalH = 0;

    for (auto& line : lines)
    {
        const std::string drawStr = line.empty() ? std::string(" ") : line;

        SDL_Surface* s = TTF_RenderText_Blended(
            nativeFont,
            drawStr.c_str(),
            static_cast<int>(drawStr.size()),
            sdlColor
        );

        if (!s)
        {
            std::cerr << "[Renderer] TTF_RenderText_Blended (multi-line) failed: "
                      << SDL_GetError() << std::endl;
            continue;
        }

        lineSurfaces.push_back(s);
        maxW   = std::max(maxW, s->w);
        totalH += s->h;
    }

    if (lineSurfaces.empty())
    {
        std::cerr << "[Renderer] CreateTextTexture: no valid line surfaces" << std::endl;
        return nullptr;
    }

    SDL_Surface* combined = SDL_CreateSurface(maxW, totalH, SDL_PIXELFORMAT_RGBA32);
    if (!combined)
    {
        std::cerr << "[Renderer] SDL_CreateSurface (combined) failed: "
                  << SDL_GetError() << std::endl;
        for (auto* s : lineSurfaces) SDL_DestroySurface(s);
        return nullptr;
    }

    int offsetY = 0;
    for (auto* s : lineSurfaces)
    {
        SDL_Rect dst{0, offsetY, s->w, s->h};
        SDL_BlitSurface(s, nullptr, combined, &dst);
        offsetY += s->h;
        SDL_DestroySurface(s);
    }
    lineSurfaces.clear();

    SDL_Surface* conv = SDL_ConvertSurface(combined, SDL_PIXELFORMAT_RGBA32);
    SDL_DestroySurface(combined);

    if (!conv)
    {
        std::cerr << "[Renderer] SDL_ConvertSurface (combined) failed: "
                  << SDL_GetError() << std::endl;
        return nullptr;
    }

    auto tex = std::make_shared<Texture>();
    if (!tex->CreateFromPixels(conv->pixels, conv->w, conv->h, true))
    {
        SDL_DestroySurface(conv);
        std::cerr << "[Renderer] CreateFromPixels (combined) failed" << std::endl;
        return nullptr;
    }

    SDL_DestroySurface(conv);
    return tex;
}

//=============================================================
// World → Screen
//=============================================================
ScreenProjectResult IRenderer::WorldToScreen(const Vector3& worldPos) const
{
    ScreenProjectResult result{};
    result.visible = false;
    result.screen  = Vector2::Zero;
    result.depth   = 1.0f;

    const Matrix4 view = GetViewMatrix();
    const Matrix4 proj = GetProjectionMatrix();
    const Matrix4 viewProj = view * proj; // ToyLib 流

    const Vector3 ndc = Vector3::TransformWithPerspDiv(worldPos, viewProj, 1.0f);

    const float ndcX = ndc.x;
    const float ndcY = ndc.y;
    const float ndcZ = ndc.z;

    if (!std::isfinite(ndcX) || !std::isfinite(ndcY) || !std::isfinite(ndcZ))
    {
        return result;
    }

    if (ndcX < -1.0f || ndcX >  1.0f ||
        ndcY < -1.0f || ndcY >  1.0f ||
        ndcZ <  0.0f || ndcZ >  1.0f)
    {
        return result;
    }

    const float screenX = (ndcX * 0.5f + 0.5f) * GetScreenWidth();
    const float screenY = (1.0f - (ndcY * 0.5f + 0.5f)) * GetScreenHeight();

    const float dpiScale = GetUIScaleInfo().scale;
    const float virtualX = screenX / dpiScale;
    const float virtualY = screenY / dpiScale;

    result.visible       = true;
    result.screen        = Vector2(screenX, screenY);
    result.virtualScreen = Vector2(virtualX, virtualY);
    result.depth         = ndcZ;
    return result;
}

} // namespace toy
