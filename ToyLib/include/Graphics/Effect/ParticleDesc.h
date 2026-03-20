#pragma once

#include "Utils/MathUtil.h"
#include <cstdint>

namespace toy
{

enum class ParticleMode
{
    Spark = 0,
    Water = 1,
    Smoke = 2,
    SnowField = 3
};

struct ParticleDesc
{
    ParticleMode mode { ParticleMode::Spark };
    uint32_t maxParticles { 64 };

    float componentLife   { 0.0f }; // 0 = infinite
    float particleLife    { 0.6f };
    float size            { 1.0f };

    float spawnRatePerSec { 60.0f };
    float spawnRampSec    { 0.0f };

    float spread          { 2.0f };
    float gravity         { 0.0f };
    float lift            { 0.0f };

    bool  additiveBlend   { true };
    bool  warmStart       { true };

    Vector3 emitterOffset { Vector3::Zero };

    // ---------------------------------------------------------
    // SnowField 用
    // ---------------------------------------------------------
    Vector3 fieldExtent   { 20.0f, 12.0f, 20.0f };
    Vector3 wind          { 0.0f, 0.0f, 0.0f };

    bool  followCamera    { false };
    bool  respawnTop      { true };

};

} // namespace toy
