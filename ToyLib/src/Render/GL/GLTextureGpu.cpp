#include "Render/GL/GLTextureGPU.h"

#include "glad/glad.h"

#include <algorithm>
#include <iostream>

namespace toy {

unsigned int GLTextureGPU::sCurrentTextureIDPerUnit[32] = {};

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
        GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32F,
        width, height, 0,
        GL_DEPTH_COMPONENT, GL_FLOAT,
        nullptr
    );

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

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
    if (unit < 0 || unit >= 32) return;

    // ★ mTextureID==0 のときも「0をbindしてクリア」しておくのが安全
    if (sCurrentTextureIDPerUnit[unit] == mTextureID)
    {
        return;
    }

    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(GL_TEXTURE_2D, mTextureID); // mTextureID==0 なら unbind

    sCurrentTextureIDPerUnit[unit] = mTextureID;
}

void GLTextureGPU::Unload()
{
    if (mTextureID != 0)
    {
        glDeleteTextures(1, &mTextureID);

        // ★消したIDがキャッシュに残ってたら全部クリア
        for (int i = 0; i < 32; ++i)
        {
            if (sCurrentTextureIDPerUnit[i] == mTextureID)
            {
                sCurrentTextureIDPerUnit[i] = 0;
            }
        }

        mTextureID = 0;
    }

    mWidth  = 0;
    mHeight = 0;
}
} // namespace toy
