// Engine/Render/RenderSurface.cpp
#include "Engine/Render/RenderSurface.h"
#include "Asset/Material/Texture.h"
#include "glad/glad.h"
#include <iostream>

namespace toy {

void RenderSurface::Create(int width, int height)
{
    Destroy();

    w = width;
    h = height;

    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    color = std::make_shared<Texture>();
    color->CreateRenderColorRGBA8(w, h);

    glFramebufferTexture2D(GL_FRAMEBUFFER,
                           GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D,
                           color->GetTextureID(),
                           0);

    glGenRenderbuffers(1, &depthRbo);
    glBindRenderbuffer(GL_RENDERBUFFER, depthRbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, w, h);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER,
                              GL_DEPTH_STENCIL_ATTACHMENT,
                              GL_RENDERBUFFER,
                              depthRbo);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
        std::cerr << "[RenderSurface] FBO incomplete\n";
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void RenderSurface::Destroy()
{
    if (depthRbo) glDeleteRenderbuffers(1, &depthRbo);
    if (fbo)      glDeleteFramebuffers(1, &fbo);
    depthRbo = 0;
    fbo = 0;
    color.reset();
    w = h = 0;
}

void RenderSurface::BindFBO() const
{
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
}

} // namespace toy
