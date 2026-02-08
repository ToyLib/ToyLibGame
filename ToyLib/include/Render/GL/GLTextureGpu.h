#pragma once

#include "Render/ITextureGPU.h"

namespace toy {

//============================================================
// GLTextureGPU
//  - OpenGL 実装（旧 Texture.cpp の GL 部分をこちらへ移す）
//============================================================
class GLTextureGPU final : public ITextureGPU
{
public:
    GLTextureGPU();
    ~GLTextureGPU() override;

    bool CreateFromPixels(const void* pixels,
                          int width,
                          int height,
                          bool hasAlpha) override;

    void CreateShadowMap(int width, int height) override;

    void CreateRenderColorRGBA8(int w, int h) override;

    void SetActive(int unit) override;

    void Unload() override;
    
    unsigned int GetTextureID() const { return mTextureID; }

private:
    unsigned int mTextureID { 0 };
    int mWidth  { 0 };
    int mHeight { 0 };

    // 同値スキップ用（GL実装側に寄せる）
    static unsigned int sCurrentTextureID;

private:
    // internal helpers
    static void DrainGLErrors();
    static bool LogGLError(const char* tag);
};

} // namespace toy
