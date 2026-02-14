#pragma once

#include "Utils/MathUtil.h" // Vector3 値渡しのため必須
#include <memory>
#include <string>

namespace toy {

class AssetManager;

// forward decl
class ITextureGPU;

//============================================================
// Texture
//  - public API は維持
//  - GPU実装は ITextureGPU に委譲（GL/VK差し替え可能に）
//============================================================
class Texture
{
public:
    Texture();
    ~Texture();

    //============================================================
    // 読み込み：画像ファイル（SDL3_image）
    //============================================================
    bool Load(const std::string& fileName, AssetManager* assetManager);

    //============================================================
    // 読み込み：メモリ上の画像データ（埋め込みテクスチャなど）
    //============================================================
    bool LoadFromMemory(const void* data, int size);

    //============================================================
    // 読み込み：生のピクセルから作成（RGBA 前提）
    //============================================================
    bool LoadFromMemory(const void* data, int width, int height);

    //============================================================
    // 生成：シャドウマップ用（depth）
    //============================================================
    void CreateShadowMap(int width, int height);

    //============================================================
    // 生成：自前生成（円形グラデーション）
    //============================================================
    bool CreateAlphaCircle(int size,
                           float centerX,
                           float centerY,
                           Vector3 color,
                           float blendPow);

    //============================================================
    // 生成：自前生成（放射状の光芒）
    //============================================================
    bool CreateRadialRays(int size,
                          int numRays,
                          float fadePow,
                          float rayStrength,
                          float intensityScale);

    //============================================================
    // 生成：RenderTarget 用 RGBA8 カラーテクスチャ
    //============================================================
    void CreateRenderColorRGBA8(int w, int h);

    //============================================================
    // 生成：ピクセルデータからテクスチャ作成（RGB/RGBA）
    // ※ public API を消さない
    //============================================================
    bool CreateFromPixels(const void* pixels, int width, int height, bool hasAlpha);

    //============================================================
    // Bind
    //============================================================
    void SetActive(int unit);

    //============================================================
    // リソース解放
    //============================================================
    void Unload();

    // optional: デバッグ・確認用（public API に追加したくないなら消してOK）
    int GetWidth()  const { return mWidth;  }
    int GetHeight() const { return mHeight; }
    
    // NOTE: OpenGL backend only.
    // RenderTarget(GL) など GL直結コードから使うための暫定アクセサ。
    // Vulkan backend では 0 を返す or 別APIになる想定。
    unsigned int GetTextureID() const;
    
    const ITextureGPU* GetGPU() const { return mGPU.get(); }
    ITextureGPU*       GetGPU()       { return mGPU.get(); }

private:
    // GPU実装（GL/VK差し替え可能）
    std::unique_ptr<ITextureGPU> mGPU;

    int mWidth  { 0 };
    int mHeight { 0 };
};

} // namespace toy
