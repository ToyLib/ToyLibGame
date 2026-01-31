#pragma once

#include "Utils/MathUtil.h"
#include "Engine/Render/PostEffect.h"
#include "Engine/Render/VisualLayer.h"
#include "glad/glad.h"

#include "Engine/Render/RenderQueue.h"

#include <SDL3/SDL.h>

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <algorithm>

namespace toy {



//==============================================================================
// ScreenProjectResult
//==============================================================================
struct ScreenProjectResult
{
    bool    visible; // 画面内に投影されているか
    Vector2 screen;  // スクリーン座標（0〜width / 0〜height）
    Vector2 virtualScreen; // 仮想スクリーン座標
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
    Matrix4 view { Matrix4::Identity };
    Matrix4 proj { Matrix4::Identity };
    bool drawUI { false };
    const class Texture* skipTexture { nullptr }; // フィードバック防止用
};

//==============================================================================
// デバッグ用の情報
//==============================================================================
struct DebugInfo
{
    unsigned int drawObjectCount {};
    unsigned int drawCallCount   {};
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

    bool Initialize(SDL_Window* window, SDL_GLContext glContext);
    void Shutdown();

    SDL_Window* GetSDLWindow() const { return mWindow; }

    //--------------------------------------------------------------------------
    // 描画
    //--------------------------------------------------------------------------

    void Draw();
    //void DrawPass(bool drawUI);

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

    void AddDrawCall() { ++mDebugActiveScreen->drawCallCount; }
    unsigned int GetDrawCallCount() const { return mDebugOnScreen.drawCallCount; }
    unsigned int GetRTTDrawCallCount() const { return mDebugRTT.drawCallCount; }

    void AddDrawObject() { ++mDebugActiveScreen->drawObjectCount; }
    unsigned int GetDrawObjectCount() const { return mDebugOnScreen.drawObjectCount; }
    unsigned int GetDrawRTTObjectCount() const { return mDebugRTT.drawObjectCount; }

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

    //void RegisterSkyDome(class SkyDomeComponent* sky);
    //class SkyDomeComponent* GetSkyDome() const { return mSkyDomeComp; }

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
    
    //--------------------------------------------------------------------------
    // ポストエフェクト
    //--------------------------------------------------------------------------
    
    void SetPostEffect(const PostEffectDesc& desc) { mPost = desc; }
    const PostEffectDesc& GetPostEffect() const { return mPost; }
    
    //--------------------------------------------------------------------------
    // フェードパラメーター設定
    //--------------------------------------------------------------------------
    void SetFade(float alpha, const Vector3& color = Vector3(0,0,0))
    {
        mFadeAlpha  = std::clamp(alpha, 0.0f, 1.0f);
        mFadeColor  = color;
        mEnableFade = (mFadeAlpha > 0.0f);
    }

    
    
    GeometryHandle GetSpriteQuadHandle() const;

    ShaderHandle GetShaderHandle(const std::string& name);
    TextureHandle ToHandle(const std::shared_ptr<Texture>& tex) const;
    MaterialHandle ToHandle(const std::shared_ptr<Material>& mat) const;
    
private:
    //--------------------------------------------------------------------------
    // SDL / OpenGL
    //--------------------------------------------------------------------------

    SDL_Window*   mWindow             { nullptr };
    SDL_GLContext mGLContext          { nullptr };
    float         mWindowDisplayScale { 1.0f };

    //--------------------------------------------------------------------------
    // スクリーン / カメラ
    //--------------------------------------------------------------------------

    float   mScreenWidth {};
    float   mScreenHeight {};
    float   mVirtualWidth {};
    float   mVirtualHeight {};

    float   mPerspectiveFOV { 45.0f };

    Matrix4 mViewMatrix {};
    Matrix4 mInvView    {};
    Matrix4 mProjectionMatrix {};
    
    
    //--------------------------------------------------------------------------
    // キャプチャーリクエストのキュー
    //--------------------------------------------------------------------------

    std::vector<SceneCaptureRequest> mSceneCaptureQueue {};

    //--------------------------------------------------------------------------
    // 描画状態 / デバッグ
    //--------------------------------------------------------------------------

    Vector3 mClearColor { Vector3(0.2f, 0.5f, 0.8f) };
    Vector3 mWireColor  { Vector3(1.0f, 1.0f, 1.0f) };

    DebugInfo mDebugOnScreen {};
    DebugInfo mDebugRTT {};
    DebugInfo* mDebugActiveScreen = &mDebugOnScreen;
    void ChangeDebugOnScreen() { mDebugActiveScreen = &mDebugOnScreen; }
    void ChangeDebugRTT() { mDebugActiveScreen = &mDebugRTT; }
    void ResetDebugCounter() { mDebugOnScreen = mDebugRTT = {}; }

    //--------------------------------------------------------------------------
    // ライティング / シャドウ
    //--------------------------------------------------------------------------

    std::shared_ptr<class LightingManager> mLightingManager;

    float mShadowNear        { 10.0f };
    float mShadowFar         { 100.0f };
    float mShadowOrthoWidth  { 100.0f };
    float mShadowOrthoHeight { 100.0f };
    int   mShadowFBOWidth    { 4096 };
    int   mShadowFBOHeight   { 4096 };

    static constexpr int kShadowCascadeCount = 2;

    GLuint  mShadowFBO[kShadowCascadeCount]        {};
    Matrix4 mLightSpaceMatrix[kShadowCascadeCount] {};
    std::shared_ptr<class Texture> mShadowMapTexture[kShadowCascadeCount];

    float mCascadeSplit0 { 25.0f };
    float mCascadeBlend  { 6.0f };

    //--------------------------------------------------------------------------
    // ポストエフェクト
    //--------------------------------------------------------------------------
    // メインシーン（ポスト用）RenderTarget
    std::shared_ptr<RenderTarget> mSceneRT ;
    // ポスト設定
    PostEffectDesc mPost {};

    // ポスト描画
    void DrawPostFromSceneRT();
    //void DrawWorldPass_NoUI();  // DrawPass(false) の代替（クリア含む）
    //void DrawUIPass_Only();     // UIだけ（クリアなし）

    //--------------------------------------------------------------------------
    // フェード
    //--------------------------------------------------------------------------
    float   mFadeAlpha  { 0.0f };     // 0=表示, 1=完全暗転
    Vector3 mFadeColor  { 0, 0, 0 };
    bool    mEnableFade { false };


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

    std::string mShaderPath { "ToyLib/Shaders/" };
    std::unordered_map<std::string, std::shared_ptr<class Shader>> mShaders;

    bool LoadShaders();

    //--------------------------------------------------------------------------
    // Visual / Sky
    //--------------------------------------------------------------------------

    std::vector<class VisualComponent*> mVisualComps;
    //class SkyDomeComponent* mSkyDomeComp { nullptr };

    //void DrawSky();
    void DrawVisualLayer(VisualLayer layer,
                         const std::shared_ptr<class Texture>& skipTex = nullptr);

    //--------------------------------------------------------------------------
    // 内部初期化
    //--------------------------------------------------------------------------

    bool LoadSettings(const std::string& filePath);
    bool InitializeShadowMapping();

    
    // OpenGL 切り離し準備
    void DrawRenderQueue_World(const RenderQueue& items);
    void DrawRenderQueue_Shadow(const RenderQueue& queue, int cascadeIndex);
    void ApplyState_GL(const RenderItem& it);
    void DrawItem_GL(const RenderItem& it, RenderPass pass, int cascadeIndex);
    //void DrawObject3DPass_Only(const std::shared_ptr<Texture>& skipTex);
    
    
private:
    // 新DrawPass
    void BeginFrame();
    void RenderShadowPass();
    void RestoreAfterShadowPass();
    void DrawSkyPass();
    void DrawWorldPass();
    void DrawOverlayScreenPass();
    void DrawFadePass();
    void DrawUIPass();
    void EndFrame();

    
};

} // namespace toy
