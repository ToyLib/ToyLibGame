#pragma once

#include "Utils/MathUtil.h"
#include "glad/glad.h"

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <SDL3/SDL.h>

namespace toy {

//-------------------------------------------------------------
// VisualLayer
// ・描画順や用途ごとにレイヤーを分けるための種別
// ・DrawVisualLayer() で使われる
//-------------------------------------------------------------
enum class VisualLayer
{
    Background2D,   // 2D背景（遠景など）
    Effect3D,       // パーティクルやエフェクト
    Object3D,       // 通常3Dオブジェクト
    OverlayScreen,  // 画面全体のポストエフェクトなど
    UI,             // UI / HUD
};

//-------------------------------------------------------------
// UI スケール情報
// ・物理解像度 / 論理解像度 / スケール係数をまとめて保持
//-------------------------------------------------------------
struct UIScaleInfo
{
    float screenW  = 0.0f;  // 物理スクリーン幅（ピクセル）
    float screenH  = 0.0f;  // 物理スクリーン高さ（ピクセル）

    float virtualW = 0.0f;  // 論理解像度の幅（UI座標系）
    float virtualH = 0.0f;  // 論理解像度の高さ（UI座標系）

    float scaleX   = 1.0f;  // screenW / virtualW
    float scaleY   = 1.0f;  // screenH / virtualH
    float scale    = 1.0f;  // min(scaleX, scaleY)  レターボックス用の共通スケール
    float offsetX  = 0.0f;  // レターボックスの左右余白
    float offsetY  = 0.0f;  // レターボックスの上下余白
};

//-------------------------------------------------------------
// Renderer
// ・SDL ウィンドウと OpenGL コンテキストを管理
// ・カメラ行列、ライト、シャドウ、スプライト等を一括して扱う
// ・Application から Draw() が呼ばれてフレームを描画する
//-------------------------------------------------------------
class Renderer
{
public:
    Renderer();
    virtual ~Renderer();
    
    //---------------------------------------------------------
    // 初期化／終了
    //---------------------------------------------------------
    
    // OpenGL コンテキストの初期化
    bool Initialize(SDL_Window* window);
    
    // SDL_Window 取得
    SDL_Window* GetSDLWindow() const { return mWindow; }
    
    // 1フレーム描画（Application から呼ばれるメイン描画）
    void Draw();
    
    // 破棄処理（OpenGL リソース等の解放）
    void Shutdown();
    
    // クリアカラー設定
    void SetClearColor(const Vector3& color);
    const Vector3& GetClearColor() const { return mClearColor; }
    
    // ワイヤーフレームカラー
    void SetWireColor(const Vector3& color) { mWireColor = color; }
    const Vector3& GetWireColor() const { return mWireColor; }
    
    
    //---------------------------------------------------------
    // カメラ／ビュー系
    //---------------------------------------------------------
    
    // ビュー行列設定（内部で逆行列もキャッシュ）
    void SetViewMatrix(const Matrix4& view) { mInvView = mViewMatrix = view; mInvView.Invert(); }
    
    Matrix4 GetViewMatrix() const { return mViewMatrix; }
    Matrix4 GetInvViewMatrix() const { return mInvView; }
    Matrix4 GetProjectionMatrix() const { return mProjectionMatrix; }
    
    // View * Projection（描画時によく使う）
    Matrix4 GetViewProjMatrix() const { return mViewMatrix * mProjectionMatrix; }
    
    // 視野角（Perspective FOV／度数法）
    float GetPerspectiveFov() const { return mPerspectiveFOV; }
    void SetPerspectiveFov(float f) { mPerspectiveFOV = f; }
    
    
    //---------------------------------------------------------
    // スクリーン情報
    //---------------------------------------------------------
    
    float GetScreenWidth() const { return mScreenWidth; }
    float GetScreenHeight() const { return mScreenHeight; }
    
    float GetVirtualWidth() const { return mVirtualWidth; }
    float GetVirtualHeight() const { return mVirtualHeight; }

    // 論理解像度の設定（UI 座標系用）
    void SetVirtualResolution(float w, float h);

    // UI 用のスケール情報をまとめて取得
    UIScaleInfo GetUIScaleInfo() const;
    
    // DPI スケール（Retina 等でのスケーリング用）
    float GetWindowDisplayScale() const { return mWindowDisplayScale; }
    
    // ウィンドウの実ピクセルサイズの変更を受け取る
    void OnWindowResized(int pixelW, int pixelH);
    
    //---------------------------------------------------------
    // VisualComponent 管理
    //---------------------------------------------------------
    
    // VisualComponent を登録／解除
    void AddVisualComp(class VisualComponent* comp);
    void RemoveVisualComp(class VisualComponent* comp);
    
    
    //---------------------------------------------------------
    // デバッグ系
    //---------------------------------------------------------
    
    void SetDebugMode(const bool b) { mIsDebugMode = b; }
    bool GetDebugMode() const { return mIsDebugMode; }
    bool IsDebugMode() const { return mIsDebugMode; }
    
    
    //---------------------------------------------------------
    // リソース管理／補助
    //---------------------------------------------------------
    
    // 全リソースの解放（シーン切り替えなど）
    void UnloadData();
    
    // スカイドーム登録（ゲーム側で生成し生ポインタで渡す）
    void RegisterSkyDome(class SkyDomeComponent* sky);
    class SkyDomeComponent* GetSkyDome() const { return mSkyDomeComp; }
    
    // ライティング管理（ライト情報の一元管理）
    std::shared_ptr<class LightingManager> GetLightingManager() const { return mLightingManager; }
    
    // 名前指定でシェーダ取得
    std::shared_ptr<class Shader> GetShader(const std::string& name) { return mShaders[name]; }
    
    
    //---------------------------------------------------------
    // シャドウマップ／ライト空間
    //---------------------------------------------------------
    
    // ライト空間行列（ShadowMap 用の ViewProj）
    Matrix4 GetLightSpaceMatrix() const { return mLightSpaceMatrix; }
    
    // シャドウマップテクスチャ（デプス or sampler2DShadow 等）
    std::shared_ptr<class Texture> GetShadowMapTexture() const { return mShadowMapTexture; }
    
    
    //---------------------------------------------------------
    // 共通ジオメトリ（スプライト / フルスクリーン）
    //---------------------------------------------------------
    
    // スプライト描画用の四角形 VAO（Billboard 等にも利用）
    std::shared_ptr<class VertexArray> GetSpriteVerts() const { return mSpriteVerts; }
    
    // フルスクリーン用ポリゴン（ポストプロセス等）
    std::shared_ptr<class VertexArray> GetFullScreenQuad() const { return mFullScreenQuad; }
    
    
    //---------------------------------------------------------
    // テキスト描画補助
    //---------------------------------------------------------
    
    // テキストからテクスチャを生成（フォント＋カラー指定）
    std::shared_ptr<class Texture> CreateTextTexture(
        const std::string& text,
        const Vector3& color,
        std::shared_ptr<class TextFont> font
    );
    
private:
    //---------------------------------------------------------
    // 設定／初期化周り
    //---------------------------------------------------------
    
    // 設定ファイル読み込み（ウィンドウサイズ／タイトルなど）
    bool LoadSettings(const std::string& filePath);
    
    // ライティング管理
    std::shared_ptr<class LightingManager> mLightingManager;
    
    // シェーダーの配置パスf
    std::string mShaderPath;
    
   
    // スクリーンサイズ (物理・仮想)
    float mScreenWidth;
    float mScreenHeight;
    float mVirtualWidth;
    float mVirtualHeight;
    
    // 視野角（Perspective FOV／度）
    float mPerspectiveFOV;
    
    // デバッグ描画 ON/OFF
    bool mIsDebugMode;
    
    // クリアカラー
    Vector3 mClearColor;
    
    // ワイヤフレームのカラー
    Vector3 mWireColor;
    
    
    //---------------------------------------------------------
    // シャドウマッピング設定
    //---------------------------------------------------------
    
    float mShadowNear;
    float mShadowFar;
    float mShadowOrthoWidth;
    float mShadowOrthoHeight;
    int   mShadowFBOWidth;
    int   mShadowFBOHeight;
    
    
    //---------------------------------------------------------
    // カメラ行列
    //---------------------------------------------------------
    
    Matrix4 mViewMatrix;
    Matrix4 mInvView;
    Matrix4 mProjectionMatrix;
    
    
    //---------------------------------------------------------
    // SDL / OpenGL ハンドル
    //---------------------------------------------------------
    
    SDL_Window*   mWindow;
    SDL_GLContext mGLContext;
    
    
    //---------------------------------------------------------
    // 共通ジオメトリ（フルスクリーン／スプライト）
    //---------------------------------------------------------
    
    // フルスクリーン描画用 VAO
    std::shared_ptr<class VertexArray> mFullScreenQuad;
    void CreateFullScreenQuad();
    
    // スプライト用頂点（Billboard 等でも使う）
    std::shared_ptr<class VertexArray> mSpriteVerts;
    void CreateSpriteVerts();
    
    
    //---------------------------------------------------------
    // シェーダ関連
    //---------------------------------------------------------
    
    std::unordered_map<std::string, std::shared_ptr<class Shader>> mShaders;
    bool LoadShaders();
    
    
    //---------------------------------------------------------
    // シャドウマッピング処理
    //---------------------------------------------------------
    
    GLuint mShadowFBO;
    bool   InitializeShadowMapping();
    void   RenderShadowMap();
    
    Matrix4 mLightSpaceMatrix;
    std::shared_ptr<class Texture> mShadowMapTexture;
    
    
    //---------------------------------------------------------
    // Visual / SkyDome
    //---------------------------------------------------------
    
    std::vector<class VisualComponent*> mVisualComps;
    
    // SkyDome は Game 側で生成・所有し、生ポインタを保持
    class SkyDomeComponent* mSkyDomeComp;
    
    void DrawSky();
    void DrawVisualLayer(VisualLayer layer);
    
    
    //---------------------------------------------------------
    // デバッグ用カウンタ
    //---------------------------------------------------------
    
    // 1フレーム内で描画したオブジェクト数（Debug/Test用）
    unsigned int mCntDrawObject;
    
    
    //---------------------------------------------------------
    // DPI スケール
    //---------------------------------------------------------
    
    float mWindowDisplayScale;
};

} // namespace toy
