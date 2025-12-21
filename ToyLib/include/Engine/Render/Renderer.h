#pragma once

#include "Utils/MathUtil.h"
#include "glad/glad.h"

#include <SDL3/SDL.h>

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace toy {

//==============================================================================
// VisualLayer
//==============================================================================
enum class VisualLayer
{
    Background2D,
    Effect3D,
    Object3D,
    OverlayScreen,
    UI,
};

//==============================================================================
// ScreenProjectResult
//==============================================================================
struct ScreenProjectResult
{
    bool    visible; // 画面内に投影されているか
    Vector2 screen;  // スクリーン座標（0〜width / 0〜height）
    float   depth;   // 深度（0〜1）
};

//==============================================================================
// UIScaleInfo
//==============================================================================
struct UIScaleInfo
{
    float screenW  = 0.0f;
    float screenH  = 0.0f;

    float virtualW = 0.0f;
    float virtualH = 0.0f;

    float scaleX   = 1.0f;
    float scaleY   = 1.0f;
    float scale    = 1.0f;
    float offsetX  = 0.0f;
    float offsetY  = 0.0f;
};

//==============================================================================
// Capture Request
//==============================================================================
struct SceneCaptureRequest
{
    std::shared_ptr<class RenderTarget> rt;
    Matrix4 view;
    Matrix4 proj;
    bool drawUI = false;
    const class Texture* skipTexture = nullptr; // フィードバック防止用
};

//==============================================================================
// Renderer
//==============================================================================
class Renderer
{
public:
    Renderer();
    virtual ~Renderer();

    //--------------------------------------------------------------------------
    // 初期化 / 終了
    //--------------------------------------------------------------------------

    bool Initialize(SDL_Window* window);
    void Shutdown();

    SDL_Window* GetSDLWindow() const { return mWindow; }

    //--------------------------------------------------------------------------
    // 描画
    //--------------------------------------------------------------------------

    void Draw();
    void DrawPass(bool drawUI);

    void DrawToRenderTarget(std::shared_ptr<class RenderTarget> rt,
                            const Matrix4& view,
                            const Matrix4& proj,
                            bool drawUI = false);

    //--------------------------------------------------------------------------
    // クリア / デバッグ
    //--------------------------------------------------------------------------

    void SetClearColor(const Vector3& color);
    const Vector3& GetClearColor() const { return mClearColor; }

    void SetWireColor(const Vector3& color) { mWireColor = color; }
    const Vector3& GetWireColor() const { return mWireColor; }

    void SetDebugMode(bool b) { mIsDebugMode = b; }
    bool GetDebugMode() const { return mIsDebugMode; }

    void SetDebugWireVisible(bool b) { mIsDebugWireVisible = b; }
    bool GetDebugWireVisible() const { return mIsDebugWireVisible; }

    void AddDrawCall() { ++mDrawCallCount; }
    unsigned int GetDrawCallCount() const { return mDrawCallCount; }

    void AddDrawObject() { ++mDrawObjectCount; }
    unsigned int GetDrawObjectCount() const { return mDrawObjectCount; }

    //--------------------------------------------------------------------------
    // カメラ / ビュー
    //--------------------------------------------------------------------------

    void SetViewMatrix(const Matrix4& view)
    {
        mViewMatrix = view;
        mInvView    = view;
        mInvView.Invert();
    }

    Matrix4 GetViewMatrix() const { return mViewMatrix; }
    Matrix4 GetInvViewMatrix() const { return mInvView; }

    Matrix4 GetProjectionMatrix() const { return mProjectionMatrix; }
    void SetProjectionMatrix(const Matrix4& mat) { mProjectionMatrix = mat; }

    Matrix4 GetViewProjMatrix() const
    {
        return mViewMatrix * mProjectionMatrix;
    }

    float GetPerspectiveFov() const { return mPerspectiveFOV; }
    void SetPerspectiveFov(float f) { mPerspectiveFOV = f; }

    Vector3 GetCameraPosition() const
    {
        return mInvView.GetTranslation();
    }

    //--------------------------------------------------------------------------
    // シーンキャプチャーリクエスト
    //--------------------------------------------------------------------------
    
    void RequestSceneCapture(const SceneCaptureRequest& req);
    void FlushSceneCaptures();
    
    //--------------------------------------------------------------------------
    // スクリーン / UI
    //--------------------------------------------------------------------------

    float GetScreenWidth() const { return mScreenWidth; }
    float GetScreenHeight() const { return mScreenHeight; }

    float GetVirtualWidth() const { return mVirtualWidth; }
    float GetVirtualHeight() const { return mVirtualHeight; }

    void SetVirtualResolution(float w, float h);
    UIScaleInfo GetUIScaleInfo() const;

    float GetWindowDisplayScale() const { return mWindowDisplayScale; }
    void OnWindowResized(int pixelW, int pixelH);

    //--------------------------------------------------------------------------
    // VisualComponent 管理
    //--------------------------------------------------------------------------

    void AddVisualComp(class VisualComponent* comp);
    void RemoveVisualComp(class VisualComponent* comp);

    //--------------------------------------------------------------------------
    // リソース / 補助
    //--------------------------------------------------------------------------

    void UnloadData();

    void RegisterSkyDome(class SkyDomeComponent* sky);
    class SkyDomeComponent* GetSkyDome() const { return mSkyDomeComp; }

    std::shared_ptr<class LightingManager> GetLightingManager() const
    {
        return mLightingManager;
    }

    std::shared_ptr<class Shader> GetShader(const std::string& name);

    //--------------------------------------------------------------------------
    // シャドウマップ（CSM）
    //--------------------------------------------------------------------------

    Matrix4 GetLightSpaceMatrix(int cascadeIndex) const
    {
        return mLightSpaceMatrix[cascadeIndex];
    }

    std::shared_ptr<class Texture> GetShadowMapTexture(int cascadeIndex) const
    {
        return mShadowMapTexture[cascadeIndex];
    }

    float GetCascadeSplit0() const { return mCascadeSplit0; }
    void SetCascadeSplit0(float f) { mCascadeSplit0 = f; }

    float GetCascadeBlend() const { return mCascadeBlend; }
    void SetCascadeBlend(float f) { mCascadeBlend = f; }

    //--------------------------------------------------------------------------
    // 共通ジオメトリ
    //--------------------------------------------------------------------------

    std::shared_ptr<class VertexArray> GetSpriteVerts() const
    {
        return mSpriteVerts;
    }

    std::shared_ptr<class VertexArray> GetFullScreenQuad() const
    {
        return mFullScreenQuad;
    }

    std::shared_ptr<class VertexArray> GetParticleQuad() const
    {
        return mSpriteVerts;
    }

    std::shared_ptr<class VertexArray> GetSurfaceQuad() const
    {
        return mSurfaceQuad;
    }

    //--------------------------------------------------------------------------
    // テキスト / 2D補助
    //--------------------------------------------------------------------------

    std::shared_ptr<class Texture> CreateTextTexture(
        const std::string& text,
        const Vector3& color,
        std::shared_ptr<class TextFont> font);

    ScreenProjectResult WorldToScreen(const Vector3& worldPos) const;

private:
    //--------------------------------------------------------------------------
    // SDL / OpenGL
    //--------------------------------------------------------------------------

    SDL_Window*   mWindow          = nullptr;
    SDL_GLContext mGLContext       = nullptr;
    float         mWindowDisplayScale = 1.0f;

    //--------------------------------------------------------------------------
    // スクリーン / カメラ
    //--------------------------------------------------------------------------

    float   mScreenWidth  = 0.0f;
    float   mScreenHeight = 0.0f;
    float   mVirtualWidth = 0.0f;
    float   mVirtualHeight = 0.0f;

    float   mPerspectiveFOV = 45.0f;

    Matrix4 mViewMatrix;
    Matrix4 mInvView;
    Matrix4 mProjectionMatrix;
    
    //--------------------------------------------------------------------------
    // キャプチャーリクエストのキュー
    //--------------------------------------------------------------------------

    std::vector<SceneCaptureRequest> mSceneCaptureQueue;

    //--------------------------------------------------------------------------
    // 描画状態 / デバッグ
    //--------------------------------------------------------------------------

    Vector3 mClearColor;
    Vector3 mWireColor;

    bool mIsDebugWireVisible = false;
    bool mIsDebugMode        = false;

    unsigned int mDrawObjectCount = 0;
    unsigned int mDrawCallCount   = 0;

    //--------------------------------------------------------------------------
    // ライティング / シャドウ
    //--------------------------------------------------------------------------

    std::shared_ptr<class LightingManager> mLightingManager;

    float mShadowNear;
    float mShadowFar;
    float mShadowOrthoWidth;
    float mShadowOrthoHeight;
    int   mShadowFBOWidth;
    int   mShadowFBOHeight;

    static constexpr int kShadowCascadeCount = 2;

    GLuint  mShadowFBO[kShadowCascadeCount]{};
    Matrix4 mLightSpaceMatrix[kShadowCascadeCount]{};
    std::shared_ptr<class Texture> mShadowMapTexture[kShadowCascadeCount]{};

    float mCascadeSplit0;
    float mCascadeBlend;

    //--------------------------------------------------------------------------
    // ジオメトリ
    //--------------------------------------------------------------------------

    std::shared_ptr<class VertexArray> mFullScreenQuad;
    std::shared_ptr<class VertexArray> mSpriteVerts;
    std::shared_ptr<class VertexArray> mSurfaceQuad;

    void CreateFullScreenQuad();
    void CreateSpriteVerts();
    void CreateSurfaceQuad();

    //--------------------------------------------------------------------------
    // シェーダ
    //--------------------------------------------------------------------------

    std::string mShaderPath;
    std::unordered_map<std::string, std::shared_ptr<class Shader>> mShaders;

    bool LoadShaders();

    //--------------------------------------------------------------------------
    // Visual / Sky
    //--------------------------------------------------------------------------

    std::vector<class VisualComponent*> mVisualComps;
    class SkyDomeComponent* mSkyDomeComp = nullptr;

    void DrawSky();
    void DrawVisualLayer(VisualLayer layer,
                         const std::shared_ptr<class Texture>& skipTex = nullptr);

    //--------------------------------------------------------------------------
    // 内部初期化
    //--------------------------------------------------------------------------

    bool LoadSettings(const std::string& filePath);
    bool InitializeShadowMapping();
    void RenderShadowMap();
};

} // namespace toy
