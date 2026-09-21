#pragma once

namespace toy {

enum class PostEffectType
{
    None = 0,
    Sepia,
    CRT,
    FeilyLand,
    Noisy,
    Grayscale,
    Monochrome,
    Watercolor,
    OldFilm
};

struct PostEffectStage
{
    PostEffectType type { PostEffectType::None };
    float intensity     { 1.0f };   // 0..1 想定（CRT/セピアの強さ等）
};

// 2段までのポストエフェクトチェーン。stage1.type が None なら1段のみで完結する。
struct PostEffectDesc
{
    PostEffectStage stage0;
    PostEffectStage stage1;
};

} // namespace toy
