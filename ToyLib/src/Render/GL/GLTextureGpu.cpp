#include "Render/GL/GLTextureGPU.h"

#include "glad/glad.h"

#include <algorithm>
#include <iostream>

namespace toy {

unsigned int GLTextureGPU::sCurrentTextureID = 0;

GLTextureGPU::GLTextureGPU()
{
}

GLTextureGPU::~GLTextureGPU()
{
    Unload();
}

void GLTextureGPU::DrainGLErrors()
{
    while (glGetError() != GL_NO_ERROR) {}
}

bool GLTextureGPU::LogGLError(const char* tag)
{
    GLenum err = glGetError();
    if (err == GL_NO_ERROR) return false;
    std::cerr << "[GL ERROR] " << tag << " : 0x"
              << std::hex << err << std::dec << "\n";
    return true;
}

bool GLTextureGPU::CreateFromPixels(const void* pixels,
                                    int width,
                                    int height,
                                    bool hasAlpha)
{
    if (!pixels || width <= 0 || height <= 0)
    {
        std::cerr << "[GLTextureGPU] CreateFromPixels invalid args\n";
        return false;
    }

    DrainGLErrors();

    if (mTextureID != 0)
    {
        glDeleteTextures(1, &mTextureID);
        mTextureID = 0;
    }

    mWidth  = width;
    mHeight = height;

    glGenTextures(1, &mTextureID);
    if (LogGLError("glGenTextures")) return false;
    if (mTextureID == 0)
    {
        std::cerr << "[GLTextureGPU] glGenTextures returned 0\n";
        return false;
    }

    glBindTexture(GL_TEXTURE_2D, mTextureID);
    if (LogGLError("glBindTexture")) return false;

    GLint prevUnpack = 0;
    glGetIntegerv(GL_UNPACK_ALIGNMENT, &prevUnpack);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    const GLenum srcFormat = hasAlpha ? GL_RGBA : GL_RGB;
    const GLenum internal  = hasAlpha ? GL_RGBA8 : GL_RGB8;

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glTexImage2D(GL_TEXTURE_2D, 0, internal,
                 width, height, 0,
                 srcFormat, GL_UNSIGNED_BYTE, pixels);

    if (LogGLError("glTexImage2D"))
    {
        glBindTexture(GL_TEXTURE_2D, 0);
        glPixelStorei(GL_UNPACK_ALIGNMENT, prevUnpack);
        return false;
    }

    glPixelStorei(GL_UNPACK_ALIGNMENT, prevUnpack);
    glBindTexture(GL_TEXTURE_2D, 0);
    return true;
}

void GLTextureGPU::CreateShadowMap(int width, int height)
{
    if (width <= 0 || height <= 0)
    {
        std::cerr << "[GLTextureGPU] CreateShadowMap invalid args\n";
        return;
    }

    if (mTextureID != 0)
    {
        glDeleteTextures(1, &mTextureID);
        mTextureID = 0;
    }

    mWidth  = width;
    mHeight = height;

    glGenTextures(1, &mTextureID);
    glBindTexture(GL_TEXTURE_2D, mTextureID);

    glTexImage2D(
        GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24,
        width, height, 0,
        GL_DEPTH_COMPONENT, GL_FLOAT,
        nullptr
    );

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    const float border[4] = { 1.f, 1.f, 1.f, 1.f };
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);

    glBindTexture(GL_TEXTURE_2D, 0);
}

void GLTextureGPU::CreateRenderColorRGBA8(int w, int h)
{
    if (w <= 0 || h <= 0)
    {
        std::cerr << "[GLTextureGPU] CreateRenderColorRGBA8 invalid args\n";
        return;
    }

    if (mTextureID != 0)
    {
        glDeleteTextures(1, &mTextureID);
        mTextureID = 0;
    }

    mWidth  = w;
    mHeight = h;

    glGenTextures(1, &mTextureID);
    glBindTexture(GL_TEXTURE_2D, mTextureID);

    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RGBA8,
        w,
        h,
        0,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        nullptr
    );

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glBindTexture(GL_TEXTURE_2D, 0);
}

void GLTextureGPU::SetActive(int unit)
{
    if (mTextureID == 0) return;

    if (sCurrentTextureID == mTextureID)
    {
        // ここで unit を無視する設計は「旧実装互換」だけど注意点あり：
        // unit をまたいで同じテクスチャを貼る用途があるなら、
        // sCurrentTextureID だけだとスキップしすぎる。
        return;
    }

    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(GL_TEXTURE_2D, mTextureID);

    sCurrentTextureID = mTextureID;
}

void GLTextureGPU::Unload()
{
    if (mTextureID != 0)
    {
        glDeleteTextures(1, &mTextureID);
        if (sCurrentTextureID == mTextureID)
        {
            sCurrentTextureID = 0;
        }
        mTextureID = 0;
    }

    mWidth  = 0;
    mHeight = 0;
}

} // namespace toy
