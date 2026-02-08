#pragma once

namespace toy {

//============================================================
// ITextureGPU
//  - Texture の public API を変えずに、GPU層だけ差し替えるための接口
//  - VK 実装が来たら VkTextureGPU を作って差し替えればOK
//============================================================
class ITextureGPU
{
public:
    virtual ~ITextureGPU() = default;

    virtual bool CreateFromPixels(const void* pixels,
                                  int width,
                                  int height,
                                  bool hasAlpha) = 0;

    virtual void CreateShadowMap(int width, int height) = 0;

    virtual void CreateRenderColorRGBA8(int w, int h) = 0;

    virtual void SetActive(int unit) = 0;

    virtual void Unload() = 0;
};

} // namespace toy
