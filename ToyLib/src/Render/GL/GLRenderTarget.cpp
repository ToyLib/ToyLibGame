//======================================================================
// GLRenderTarget.cpp
//======================================================================
#include "Render/GL/GLRenderTarget.h"

#include "Asset/Material/Texture.h"
#include "glad/glad.h"

#include <iostream>

namespace toy {

GLRenderTarget::~GLRenderTarget()
{
    Unload();
}

//==============================================================================
// Unload
//------------------------------------------------------------------------------
void GLRenderTarget::Unload()
{
    // 呼び出し側で GL context current を保証すること
    if (mDepthRBO != 0)
    {
        glDeleteRenderbuffers(1, &mDepthRBO);
        mDepthRBO = 0;
    }
    if (mFBO != 0)
    {
        glDeleteFramebuffers(1, &mFBO);
        mFBO = 0;
    }

    if (mColorTex)
    {
        // Texture が GL resource を持つなら、ここで Unload してもOK
        // mColorTex->Unload();
        mColorTex.reset();
    }

    mW = 0;
    mH = 0;
}

//==============================================================================
// Create
//------------------------------------------------------------------------------
bool GLRenderTarget::Create(int w, int h)
{
    if (w <= 0 || h <= 0)
    {
        std::cerr << "[GLRenderTarget] Create failed: invalid size "
                  << w << "x" << h << "\n";
        return false;
    }

    // 既に作られている場合は作り直す（リーク防止）
    if (mFBO != 0 || mDepthRBO != 0 || mColorTex)
    {
        Unload();
    }

    mW = w;
    mH = h;

    auto fail = [&](const char* msg)
    {
        std::cerr << "[GLRenderTarget] " << msg << "\n";
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glBindRenderbuffer(GL_RENDERBUFFER, 0);
        Unload();
        return false;
    };

    // ---------------------------------------------------------
    // FBO
    // ---------------------------------------------------------
    glGenFramebuffers(1, &mFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, mFBO);

    // ---------------------------------------------------------
    // Color attachment (RGBA8)
    // ---------------------------------------------------------
    mColorTex = std::make_shared<Texture>();
    mColorTex->CreateRenderColorRGBA8(w, h);

    if (!mColorTex || mColorTex->GetTextureID() == 0)
    {
        return fail("CreateRenderColorRGBA8 failed");
    }

    glFramebufferTexture2D(GL_FRAMEBUFFER,
                           GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D,
                           mColorTex->GetTextureID(),
                           0);

    // ---------------------------------------------------------
    // Depth/Stencil RBO
    // ---------------------------------------------------------
    glGenRenderbuffers(1, &mDepthRBO);
    glBindRenderbuffer(GL_RENDERBUFFER, mDepthRBO);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, w, h);

    glFramebufferRenderbuffer(GL_FRAMEBUFFER,
                              GL_DEPTH_STENCIL_ATTACHMENT,
                              GL_RENDERBUFFER,
                              mDepthRBO);

    // ---------------------------------------------------------
    // Check complete
    // ---------------------------------------------------------
    const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE)
    {
        return fail("FBO incomplete");
    }

    // restore
    glBindRenderbuffer(GL_RENDERBUFFER, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return true;
}

//==============================================================================
// Bind
//------------------------------------------------------------------------------
void GLRenderTarget::Bind()
{
    glBindFramebuffer(GL_FRAMEBUFFER, mFBO);
    glViewport(0, 0, mW, mH);
}

//==============================================================================
// Unbind
//------------------------------------------------------------------------------
void GLRenderTarget::Unbind()
{
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

//==============================================================================
// Resize
//------------------------------------------------------------------------------
bool GLRenderTarget::Resize(int w, int h)
{
    // サイズ同じなら何もしない（重要：無駄な再生成防止）
    if (w == mW && h == mH)
    {
        return true;
    }

    return Create(w, h);
}

} // namespace toy
