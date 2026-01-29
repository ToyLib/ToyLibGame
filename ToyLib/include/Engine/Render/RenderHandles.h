// Engine/Render/RenderHandles.h
#pragma once
#include <cstdint>

namespace toy {

// 前方宣言（実体は今まで通り）
class VertexArray;
class Texture;
class Shader;

struct GeometryHandle
{
    VertexArray* ptr = nullptr;
};

struct TextureHandle
{
    Texture* ptr = nullptr;
};

struct ShaderHandle
{
    Shader* ptr = nullptr; // 当面GL Shader。後でPipelineHandleに置換。
};

} // namespace toy
