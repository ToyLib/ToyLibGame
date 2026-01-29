// Engine/Render/RenderEnums.h
#pragma once

namespace toy {

enum class RenderPass
{
    Main,       // 通常描画（3Dなど）
    UI,         // UI/2D
    Shadow,     // 影（将来）
    Debug,      // ワイヤー等
};

enum class PrimitiveTopology
{
    Triangles,
    Lines,
};

enum class BlendMode
{
    Opaque,
    Alpha,
    Additive,
};

} // namespace toy
