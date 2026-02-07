// Render/RenderHandles.h
#pragma once
#include <cstdint>

namespace toy {

class VertexArray;
class Texture;
class Shader;
class Material;   // ★追加

struct GeometryHandle
{
    VertexArray* ptr = nullptr;
};

struct TextureHandle
{
    Texture*    ptr = nullptr;
};

struct PipelineHandle
{
    Shader*     ptrGLShader = nullptr;
};

// ★追加：MaterialHandle
struct MaterialHandle
{
    Material* ptr = nullptr;
};

} // namespace toy
