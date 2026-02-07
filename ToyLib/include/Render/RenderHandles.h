// Render/RenderHandles.h
#pragma once
#include <cstdint>

namespace toy {

enum class PipelineBackend : uint8_t
{
    None = 0,
    GL,
    VK
};

struct GeometryHandle
{
    class VertexArray* ptr = nullptr;
};

struct TextureHandle
{
    class Texture*    ptr = nullptr;
};

struct PipelineHandle
{
    PipelineBackend backend { PipelineBackend::None };
    class Shader*     ptrGLShader { nullptr };
    bool IsValidGL() const { return ptrGLShader != nullptr; }
};

// ★追加：MaterialHandle
struct MaterialHandle
{
    class Material* ptr = nullptr;
};

} // namespace toy
