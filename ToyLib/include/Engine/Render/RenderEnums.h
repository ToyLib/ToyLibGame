// Engine/Render/RenderEnums.h
#pragma once

namespace toy {

enum class RenderPass
{
    World,
    UI,
    Shadow,
    Debug,
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

// ★追加：カリング
enum class CullMode
{
    None,
    Back,
    Front,
};

// ★追加：FrontFace
enum class FrontFace
{
    CCW,
    CW,
};

} // namespace toy
