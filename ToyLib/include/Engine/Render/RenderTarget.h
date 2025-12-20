#pragma once
#include <memory>

namespace toy {

class Texture;

class RenderTarget
{
public:
    bool Create(int w, int h);
    void Bind();
    static void Unbind();

    std::shared_ptr<Texture> GetColorTexture() const { return mColorTex; }
    
    int GetWidth() const { return mW; }
    int GetHeight() const { return mH; }

private:
    unsigned int mFBO = 0;
    unsigned int mDepthRBO = 0;

    int mW = 0;
    int mH = 0;

    std::shared_ptr<Texture> mColorTex;
};

}
