//======================================================================
// RenderTarget.h
//======================================================================
#pragma once

#include <memory>

namespace toy {

class Texture;

//==============================================================
// RenderTarget (backend-agnostic base)
//  - Create/Bind/Unbind の共通APIだけを提供
//  - 実体は GLRenderTarget / VkRenderTarget に分離
//==============================================================
class IRenderTarget
{
public:
    virtual ~IRenderTarget() = default;

    virtual bool Create(int w, int h) = 0;
    virtual void Unload() = 0;

    virtual void Bind() = 0;
    virtual void Unbind() = 0;

    int GetWidth()  const { return mW; }
    int GetHeight() const { return mH; }

    std::shared_ptr<Texture> GetColorTexture() const { return mColorTex; }

protected:
    int mW { 0 };
    int mH { 0 };

    // NOTE:
    // - ここは “結果として使えるテクスチャ” を返すために残す
    // - VK版では VkTexture 的なものになる可能性があるので、
    //   将来 Texture 自体を ITexture に寄せるなら差し替えやすい
    std::shared_ptr<Texture> mColorTex;
};

} // namespace toy
