#include "Engine/Render/RenderTarget.h"
#include "Asset/Material/Texture.h"

#include "glad/glad.h"

#include <iostream>

namespace toy {


RenderTarget::RenderTarget()
{
}

RenderTarget::~RenderTarget()
{
    /*
    if (mDepthRBO) glDeleteRenderbuffers(1, &mDepthRBO);
    if (mFBO)      glDeleteFramebuffers(1, &mFBO);
    mDepthRBO = 0;
    mFBO = 0;
    mColorTex.reset();
    */
}

void RenderTarget::Unload()
{
    if (mDepthRBO) glDeleteRenderbuffers(1, &mDepthRBO);
    if (mFBO)      glDeleteFramebuffers(1, &mFBO);
    mDepthRBO = 0;
    mFBO = 0;
    mColorTex.reset();
}

//==============================================================================
// RenderTarget : 初期化
//------------------------------------------------------------------------------
bool RenderTarget::Create(int w, int h)
{
    // サイズ保持
    mW = w;
    mH = h;

    //----------------------------------------------------------
    // Framebuffer Object
    //----------------------------------------------------------
    glGenFramebuffers(1, &mFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, mFBO);

    //----------------------------------------------------------
    // Color Attachment（RGBA8 テクスチャ）
    //----------------------------------------------------------
    mColorTex = std::make_shared<Texture>();
    mColorTex->CreateRenderColorRGBA8(w, h);

    glFramebufferTexture2D(
        GL_FRAMEBUFFER,
        GL_COLOR_ATTACHMENT0,
        GL_TEXTURE_2D,
        mColorTex->GetTextureID(),
        0
    );

    //----------------------------------------------------------
    // Depth / Stencil Renderbuffer
    //----------------------------------------------------------
    glGenRenderbuffers(1, &mDepthRBO);
    glBindRenderbuffer(GL_RENDERBUFFER, mDepthRBO);
    glRenderbufferStorage(
        GL_RENDERBUFFER,
        GL_DEPTH24_STENCIL8,
        w,
        h
    );

    glFramebufferRenderbuffer(
        GL_FRAMEBUFFER,
        GL_DEPTH_STENCIL_ATTACHMENT,
        GL_RENDERBUFFER,
        mDepthRBO
    );

    //----------------------------------------------------------
    // 完成チェック
    //----------------------------------------------------------
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
        std::cerr << "[RenderTarget] FBO incomplete\n";
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        return false;
    }

    // デフォルトに戻す
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return true;
}

//==============================================================================
// バインド
//------------------------------------------------------------------------------
void RenderTarget::Bind()
{
    glBindFramebuffer(GL_FRAMEBUFFER, mFBO);
    glViewport(0, 0, mW, mH);
}

//==============================================================================
// アンバインド
//------------------------------------------------------------------------------
void RenderTarget::Unbind()
{
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

} // namespace toy
