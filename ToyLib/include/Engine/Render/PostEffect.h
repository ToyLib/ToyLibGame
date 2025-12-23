#pragma once

namespace toy {

enum class PostEffectType
{
    None,
    Sepia,
    CRT,
    Grayscale,
    Fade,
    // 追加前提
};

struct PostEffectState
{
    PostEffectType type = PostEffectType::None;

    float intensity = 1.0f;   // 効き具合（0〜1想定）
    float time      = 0.0f;   // アニメーション用（任意）
};

} // namespace toy
