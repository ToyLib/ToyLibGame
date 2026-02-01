#pragma once

#include "Utils/MathUtil.h" // Vector3
#include "glad/glad.h"
#include <string>

namespace toy {

//==============================================================================
// Texture
//------------------------------------------------------------------------------
// ・OpenGL テクスチャを管理するクラス
// ・画像ファイル / メモリ / ピクセル配列からの生成に対応
// ・描画用 / フォント用 / レンダーターゲット / シャドウマップなどを統一管理
//==============================================================================
class Texture
{
public:
    //--------------------------------------------------------------------------
    // ctor / dtor
    //--------------------------------------------------------------------------
    Texture();
    ~Texture();

    //--------------------------------------------------------------------------
    // 読み込み・生成（外部データ）
    //--------------------------------------------------------------------------

    // SDL3_image を用いた画像ファイル読み込み
    bool Load(const std::string& fileName, class AssetManager* assetManager);

    // 埋め込み画像読み込み（Assimp の aiTexture 用）
    // ・data + size から画像フォーマットを自動判別
    bool LoadFromMemory(const void* data, int size);

    // RGBA ピクセル配列を直接指定して生成
    bool LoadFromMemory(const void* data, int width, int height);

    // SDL_ttf 等から受け取ったピクセル配列から生成
    bool CreateFromPixels(const void* pixels,
                          int width,
                          int height,
                          bool hasAlpha = true);

    //--------------------------------------------------------------------------
    // レンダリング用途テクスチャ生成
    //--------------------------------------------------------------------------

    // RGBA8 の描画用カラーテクスチャ（RenderTarget 用）
    void CreateRenderColorRGBA8(int w, int h);


    //--------------------------------------------------------------------------
    // エフェクト用テクスチャ生成
    //--------------------------------------------------------------------------

    // 円形アルファグラデーション（グロー・レンズフレア等）
    bool CreateAlphaCircle(int size,
                           float centerX,
                           float centerY,
                           Vector3 color,
                           float blendPow = 1.0f);

    // 放射状光芒（ゴッドレイ風）
    bool CreateRadialRays(int size,
                          int numRays,
                          float fadePow,
                          float rayStrength,
                          float intensityScale);

    //--------------------------------------------------------------------------
    // シャドウマップ用（深度テクスチャ）
    //--------------------------------------------------------------------------

    // 深度専用テクスチャ生成（CSM / シャドウマッピング用）
    void CreateShadowMap(int width, int height);

    //--------------------------------------------------------------------------
    // OpenGL 制御
    //--------------------------------------------------------------------------

    // GPU メモリ解放
    void Unload();

    // 指定テクスチャユニットにバインド
    void SetActive(int unit);

    //--------------------------------------------------------------------------
    // 情報取得
    //--------------------------------------------------------------------------

    int GetWidth()  const { return mWidth; }
    int GetHeight() const { return mHeight; }

    // Raw OpenGL texture ID
    unsigned int GetTextureID() const { return mTextureID; }

private:
    //--------------------------------------------------------------------------
    // 内部データ
    //--------------------------------------------------------------------------

    // OpenGL テクスチャ ID
    GLuint mTextureID {};
    static GLuint sCurrentTextureID;
    

    // サイズ
    int mWidth {};
    int mHeight {};
};

} // namespace toy
