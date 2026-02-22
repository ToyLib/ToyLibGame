//======================================================================
// GLRenderTarget.h
//======================================================================
#pragma once

#include "Render/IRenderTarget.h"

#include <cstdint>

namespace toy {

//==============================================================
// GLRenderTarget
//==============================================================
class GLRenderTarget : public IRenderTarget
{
public:
    ~GLRenderTarget() override;

    bool Create(int w, int h) override;
    void Unload() override;

    void Bind() override;
    void Unbind() override;

    // ★追加
    bool Resize(int w, int h) override;

    uint32_t GetFBO() const { return mFBO; }

private:
    uint32_t mFBO      { 0 };
    uint32_t mDepthRBO { 0 };
};
} // namespace toy
