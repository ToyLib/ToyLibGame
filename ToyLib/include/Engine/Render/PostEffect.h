#pragma once

namespace toy {

enum class PostEffectType
{
    None = 0,
    Sepia,
    CRT,
};

struct PostEffectDesc
{
    PostEffectType type = PostEffectType::None;
    float intensity     = 1.0f;   // 0..1 想定（CRT/セピアの強さ等）
};

} // namespace toy
