#pragma once

#include "Utils/MathUtil.h"
#include <string>

namespace toy {

//============================================================
// Texture
//   ・OpenGL テクスチャを管理するクラス
//   ・画像ファイル / メモリからの読み込み
//   ・描画用テクスチャ / フォント描画用
//   ・シャドウマップ / ポストエフェクト用の生成
//============================================================
class Texture
{
public:
    Texture();
    ~Texture();

    // --------------------------------------------------------
    // 読み込み系
    // --------------------------------------------------------

    // SDL3_image を用いた画像ファイル読み込み
    bool Load(const std::string& fileName, class AssetManager* assetManager);

    // 埋め込み画像読み込み（Assimp の aiTexture 用）
    bool LoadFromMemory(const void* data, int size);                   // データサイズのみ（画像フォーマットを判別）
    bool LoadFromMemory(const void* data, int width, int height);      // RGBAピクセル直接

    // SDL_ttf 等から受け取ったピクセルデータによるテクスチャ生成
    bool CreateFromPixels(const void* pixels, int width, int height, bool hasAlpha = true);

    void CreateRenderColorRGBA8(int w, int h);

    // --------------------------------------------------------
    // 特殊テクスチャ生成
    // --------------------------------------------------------

    // 空のレンダリング用テクスチャ作成（FBO 等で利用）
    void CreateForRendering(int w, int h, unsigned int format);

    // グローエフェクト・レンズフレアなど用の円グラデーション
    bool CreateAlphaCircle(int size,
                           float centerX,
                           float centerY,
                           Vector3 color,
                           float blendPow = 1.0f);

    // 放射状の光芒（ゴッドレイ風ルック用）
    bool CreateRadialRays(int size,
                          int numRays,
                          float fadePow,
                          float rayStrength,
                          float intensityScale);
    
    // --------------------------------------------------------
    // OpenGL制御 & 基本情報
    // --------------------------------------------------------

    // GPU メモリを解放
    void Unload();

    // テクスチャをアクティブ化 → 指定テクスチャユニットへ
    void SetActive(int unit);

    // サイズ取得
    int GetWidth()  const { return mWidth; }
    int GetHeight() const { return mHeight; }

    // --------------------------------------------------------
    // シャドウマップ用テクスチャ生成（深度テクスチャ）
    // --------------------------------------------------------
    void CreateShadowMap(int width, int height);

    // Raw texture ID
    unsigned int GetTextureID() const { return mTextureID; }

private:
    // OpenGL が管理するテクスチャ ID
    unsigned int mTextureID = 0;

    // サイズ
    int mWidth  = 0;
    int mHeight = 0;
};

} // namespace toy
