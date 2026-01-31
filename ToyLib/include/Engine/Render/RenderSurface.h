// Engine/Render/RenderSurface.h
#pragma once
#include <memory>

namespace toy {

struct RenderSurface
{
    int w = 0;
    int h = 0;

    unsigned int fbo = 0;
    unsigned int depthRbo = 0; // depth24-stencil8

    std::shared_ptr<class Texture> color; // RGBA8

    void Create(int width, int height);
    void Destroy();

    void BindFBO() const; // viewportは触らない
};

} // namespace toy
