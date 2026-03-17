#pragma once

#include <memory>

namespace toy {

enum class PostEffectType
{
    None = 0,
    Sepia,
    CRT,
    FeilyLand,
    Watercolor,
    Grayscale,
    Monochrome
};

struct PostEffectDesc
{
    PostEffectType type { PostEffectType::None };
    float intensity     { 1.0f };   // 0..1 想定（CRT/セピアの強さ等）
    std::shared_ptr<class Texture> paperTex;
};

} // namespace toy
