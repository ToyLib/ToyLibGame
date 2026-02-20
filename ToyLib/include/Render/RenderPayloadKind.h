#pragma once
#include <cstdint>

namespace toy {

enum class PayloadKind : uint8_t
{
    None = 0,
    Sprite,
    Mesh,
    SkinnedMesh,
    Billboard,
    Particle,
    SkyDome,
    Overlay,
    Debug,
    Surface
};

} // namespace toy
