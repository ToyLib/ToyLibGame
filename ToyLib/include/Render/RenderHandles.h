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
    class Texture* ptr = nullptr;
};

struct PipelineHandle
{
    PipelineBackend backend { PipelineBackend::None };

    // GL
    class GLShader* ptrGLShader { nullptr };

    // VK（VKPipelineのラッパーを指す）
    void* ptrVKPipeline { nullptr };

    bool IsValidGL() const { return (backend == PipelineBackend::GL) && (ptrGLShader != nullptr); }
    bool IsValidVK() const { return (backend == PipelineBackend::VK) && (ptrVKPipeline != nullptr); }
    bool IsValid()   const { return IsValidGL() || IsValidVK(); }
};

// MaterialHandle
struct MaterialHandle
{
    class Material* ptr = nullptr;
};

} // namespace toy
