//======================================================================
// RenderTarget.h
//======================================================================
#pragma once

#include <memory>

namespace toy {

//==============================================================
// RenderTarget (backend-agnostic base)
//  - Create/Bind/Unbind の共通APIだけを提供
//  - 実体は GLRenderTarget / VkRenderTarget に分離
//==============================================================
//==============================================================
// RenderTarget (backend-agnostic base)
//==============================================================
class IRenderTarget
{
public:
    virtual ~IRenderTarget() = default;

    virtual bool Create(int w, int h) = 0;
    virtual void Unload() = 0;

    virtual void Bind() = 0;
    virtual void Unbind() = 0;

    // ★追加：Resize（RTT前提なら必須）
    virtual bool Resize(int w, int h) = 0;

    int GetWidth()  const { return mW; }
    int GetHeight() const { return mH; }

    std::shared_ptr<class Texture> GetColorTexture() const { return mColorTex; }

protected:
    int mW { 0 };
    int mH { 0 };

    std::shared_ptr<class Texture> mColorTex;
};
} // namespace toy
