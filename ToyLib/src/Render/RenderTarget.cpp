#include "Render/RenderTarget.h"
#include "Asset/Material/Texture.h"

#include "glad/glad.h"

#include <iostream>

namespace toy {

void RenderTarget::Unload()
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

    // Texture が GL resource を持つなら、ここで Unload する方が安全
    // （Texture::Unload がある前提。無ければ reset だけでOK）
    if (mColorTex)
    {
        // mColorTex->Unload();
        mColorTex.reset();
    }

    mW = 0;
    mH = 0;
}

//==============================================================================
// Create
//------------------------------------------------------------------------------
bool RenderTarget::Create(int w, int h)
{
    if (w <= 0 || h <= 0)
    {
        std::cerr << "[RenderTarget] Create failed: invalid size "
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

    // 失敗時は必ず Unload して戻る
    auto fail = [&](const char* msg)
    {
        std::cerr << "[RenderTarget] " << msg << "\n";
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

    // NOTE:
    // - make_shared 直後なので mColorTex が null になることはない
    // - GL側の生成に失敗したかどうかは TextureID で判定する
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

    // ---------------------------------------------------------
    // restore
    // ---------------------------------------------------------
    glBindRenderbuffer(GL_RENDERBUFFER, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return true;
}

//==============================================================================
// Bind
//------------------------------------------------------------------------------
void RenderTarget::Bind()
{
    glBindFramebuffer(GL_FRAMEBUFFER, mFBO);
    glViewport(0, 0, mW, mH);
}

//==============================================================================
// Unbind
//------------------------------------------------------------------------------
void RenderTarget::Unbind()
{
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

} // namespace toy
