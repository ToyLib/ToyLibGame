#pragma once

#include "Utils/MathUtil.h"

namespace toy {

//==============================================================
// Easing
//  - 0..1 の進み具合 t を、動きのカーブに沿った 0..1 に変換する
//  - Tween / UI の開閉アニメーションなどで使う
//==============================================================
enum class Ease
{
    Linear,
    InQuad,
    OutQuad,
    InOutQuad,
    InCubic,
    OutCubic,
    InOutCubic,
    OutBack,     // 少し行き過ぎて戻る（ポップ感）
};

inline float ApplyEase(Ease ease, float t)
{
    t = Math::Clamp(t, 0.0f, 1.0f);

    switch (ease)
    {
        case Ease::Linear:
            return t;

        case Ease::InQuad:
            return t * t;
        case Ease::OutQuad:
            return 1.0f - (1.0f - t) * (1.0f - t);
        case Ease::InOutQuad:
            return (t < 0.5f) ? 2.0f * t * t
                              : 1.0f - 2.0f * (1.0f - t) * (1.0f - t);

        case Ease::InCubic:
            return t * t * t;
        case Ease::OutCubic:
        {
            const float u = 1.0f - t;
            return 1.0f - u * u * u;
        }
        case Ease::InOutCubic:
        {
            if (t < 0.5f) return 4.0f * t * t * t;
            const float u = 1.0f - t;
            return 1.0f - 4.0f * u * u * u;
        }

        case Ease::OutBack:
        {
            constexpr float c1 = 1.70158f;
            constexpr float c3 = c1 + 1.0f;
            const float u = t - 1.0f;
            return 1.0f + c3 * u * u * u + c1 * u * u;
        }
    }
    return t;
}

} // namespace toy
