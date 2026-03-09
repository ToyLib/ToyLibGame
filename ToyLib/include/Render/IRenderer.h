#pragma once

//==============================================================================
// IRenderer.h
//  - Renderer core interface
//  - Frame collection (BuildFrameQueues) + bucketed passes
//  - Shadow / World / Overlay / UI / Post / Fade
//==============================================================================

// Engine / Utils
#include "Utils/MathUtil.h"

// Render
#include "Render/PostEffect.h"
#include "Render/RenderQueue.h"
#include "Render/VisualLayer.h"


// SDL
#include <SDL3/SDL.h>

// Std
#include <algorithm>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace toy
{

//==============================================================================
// ScreenProjectResult
//  - WorldToScreen() の結果
//==============================================================================
struct ScreenProjectResult
{
    bool    visible;        // 画面内に投影されているか
    Vector2 screen;         // スクリーン座標（0〜width / 0〜height）
    Vector2 virtualScreen;  // 仮想スクリーン座標
    float   depth;          // 深度（0〜1）
};

//==============================================================================
// UIScaleInfo
//  - 論理(仮想)解像度→実スクリーンへのスケーリング情報
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
// SceneCaptureRequest
//  - Draw() 前に積んでおき、RenderTarget へ描画する要求
//==============================================================================
struct SceneCaptureRequest
{
    std::shared_ptr<class IRenderTarget> rt;

    Matrix4 view { Matrix4::Identity };
    Matrix4 proj { Matrix4::Identity };

    bool drawUI { false };

    // 将来拡張
    bool drawSky     { true };
    bool drawWorld   { true };
    bool drawOverlay { false }; // まず false 推奨
};

//==============================================================================
// FrameBuckets
//  - mFrame.items の index を用途別に分類したもの
//==============================================================================
struct FrameBuckets
{
    // World
    std::vector<uint32_t> worldOpaque;
    std::vector<uint32_t> worldTransparent;
    std::vector<uint32_t> effectPre;
    std::vector<uint32_t> effectOverlay;

    // Other passes
    std::vector<uint32_t> sky;
    std::vector<uint32_t> overlayScreen;
    std::vector<uint32_t> ui;

    // Shadow caster (RenderPass::Shadow)
    std::vector<uint32_t> shadowCaster;

    void Clear()
    {
        worldOpaque.clear();
        worldTransparent.clear();
        effectPre.clear();
        effectOverlay.clear();
        sky.clear();
        overlayScreen.clear();
        ui.clear();
        shadowCaster.clear();
    }
};

//==============================================================================
// DebugInfo
//==============================================================================
struct DebugInfo
{
    unsigned int drawCallCount {};
};

//==============================================================================
// Renderer
//==============================================================================
class IRenderer
{
public:
    IRenderer();
    virtual ~IRenderer();

    //--------------------------------------------------------------------------
    // Initialize / Shutdown
    //--------------------------------------------------------------------------

    virtual bool Initialize(const class Application* app) { return false; }
    virtual void Shutdown() {};
    virtual void WaitIdle() {};

    //--------------------------------------------------------------------------
    // Main draw
    //--------------------------------------------------------------------------

    void Draw();
    virtual void DrawToRenderTarget(const struct SceneCaptureRequest& req) {};

    //--------------------------------------------------------------------------
    // Clear / Debug
    //--------------------------------------------------------------------------

    virtual void SetClearColor(const Vector3& color);
    const Vector3& GetClearColor() const { return mClearColor; }

    void SetWireColor(const Vector3& color) { mWireColor = color; }
    const Vector3& GetWireColor() const { return mWireColor; }

    void AddDrawCall() { ++mDebugActiveScreen->drawCallCount; }
    unsigned int GetDrawCallCount() const { return mDebugOnScreen.drawCallCount; }
    unsigned int GetRTTDrawCallCount() const { return mDebugRTT.drawCallCount; }

    //--------------------------------------------------------------------------
    // Camera / View
    //--------------------------------------------------------------------------

    void SetViewMatrix(const Matrix4& view)
    {
        mViewMatrix = view;
        mInvView    = view;
        mInvView.Invert();
        mViewProjMatrix = mViewMatrix * mProjectionMatrix;
    }

    Matrix4 GetViewMatrix() const { return mViewMatrix; }
    Matrix4 GetInvViewMatrix() const { return mInvView; }

    Matrix4 GetProjectionMatrix() const { return mProjectionMatrix; }

    void SetProjectionMatrix(const Matrix4& mat)
    {
        mProjectionMatrix = mat;
        mViewProjMatrix   = mViewMatrix * mProjectionMatrix;
    }

    Matrix4 GetViewProjMatrix() const { return mViewProjMatrix; }

    float GetPerspectiveFov() const { return mPerspectiveFOV; }
    void SetPerspectiveFov(float f) { mPerspectiveFOV = f; }

    Vector3 GetCameraPosition() const
    {
        return mInvView.GetTranslation();
    }
    
    const std::string GetShaderPath() const { return mShaderPath; }

    //--------------------------------------------------------------------------
    // Screen / UI
    //--------------------------------------------------------------------------

    float GetScreenWidth()  const { return mScreenWidth; }
    float GetScreenHeight() const { return mScreenHeight; }

    float GetVirtualWidth()  const { return mVirtualWidth; }
    float GetVirtualHeight() const { return mVirtualHeight; }

    void SetVirtualResolution(float w, float h);
    UIScaleInfo GetUIScaleInfo() const;

    float GetWindowDisplayScale() const { return mWindowDisplayScale; }
    virtual void OnWindowResized(int pixelW, int pixelH) {};

    //--------------------------------------------------------------------------
    // Scene capture request
    //--------------------------------------------------------------------------

    virtual std::shared_ptr<class IRenderTarget>  CreateRenderTarget() { return nullptr; }
    void RequestSceneCapture(const SceneCaptureRequest& req);

    //--------------------------------------------------------------------------
    // VisualComponent management
    //--------------------------------------------------------------------------

    void AddVisualComp(class VisualComponent* comp);
    void RemoveVisualComp(class VisualComponent* comp);

    //--------------------------------------------------------------------------
    // Resources / helpers
    //--------------------------------------------------------------------------

    virtual void UnloadData() {};

    std::shared_ptr<class LightingManager> GetLightingManager() const
    {
        return mLightingManager;
    }

    //--------------------------------------------------------------------------
    // Shadow mapping (CSM)
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
    
    float GetShadowBias() const { return mShadowBias; }
    void SetShadowBias(float f) { mShadowBias = f; }

    //--------------------------------------------------------------------------
    // Common geometry
    //--------------------------------------------------------------------------

    std::shared_ptr<class VertexArray> GetSpriteQuad() const
    {
        return mSpriteQuad;
    }

    std::shared_ptr<class VertexArray> GetFullScreenQuad() const
    {
        return mFullScreenQuad;
    }

    std::shared_ptr<class VertexArray> GetSurfaceQuad() const
    {
        return mSurfaceQuad;
    }

    //--------------------------------------------------------------------------
    // Text / 2D helper
    //--------------------------------------------------------------------------

    std::shared_ptr<class Texture> CreateTextTexture(
        const std::string& text,
        const Vector3& color,
        std::shared_ptr<class TextFont> font);

    ScreenProjectResult WorldToScreen(const Vector3& worldPos) const;

    //--------------------------------------------------------------------------
    // Post effect
    //--------------------------------------------------------------------------

    void SetPostEffect(const PostEffectDesc& desc) { mPost = desc; }
    const PostEffectDesc& GetPostEffect() const { return mPost; }

    //--------------------------------------------------------------------------
    // Fade
    //--------------------------------------------------------------------------

    void SetFade(float alpha, const Vector3& color = Vector3(0,0,0))
    {
        mFadeAlpha  = std::clamp(alpha, 0.0f, 1.0f);
        mFadeColor  = color;
        mEnableFade = (mFadeAlpha > 0.0f);
    }

    //--------------------------------------------------------------------------
    // Handle conversion
    //--------------------------------------------------------------------------

    GeometryHandle GetSpriteQuadHandle() const;
    GeometryHandle GetSurfaceQuadHandle() const;

    virtual PipelineHandle GetPipelineHandle(const std::string& name) = 0;
    TextureHandle  ToHandle(const std::shared_ptr<Texture>& tex) const;
    MaterialHandle ToHandle(const std::shared_ptr<Material>& mat) const;
    
    
    const SpritePayload& GetSpritePayload(uint32_t idx) const
    {
        return mRenderQueue.GetSpritePayload(idx);
    }
    const UnlitQuadPayload& GetUnlitQuadPayload(uint32_t idx) const
    {
        return mRenderQueue.GetUnlitQuadPayload(idx);
    }
    const DebugPayload& GetDebugPayload(uint32_t idx) const
    {
        return mRenderQueue.GetDebugPayload(idx);
    }
    const SurfacePayload& GetSurfacePayload(uint32_t idx) const
    {
        return mRenderQueue.GetSurfacePayload(idx);
    }
    const ParticlePayload& GetParticlePayload(uint32_t idx) const
    {
        return mRenderQueue.GetParticlePayload(idx);
    }
    const OverlayPayload& GetOverlayPayload(uint32_t idx) const
    {
        return mRenderQueue.GetOverlayPayload(idx);
    }
    const SkyDomePayload& GetSkyDomePayload(uint32_t idx) const
    {
        return mRenderQueue.GetSkyDomePayload(idx);
    }
    const SkinnedMeshPayload& GetSkinnedMeshPayload(uint32_t idx) const
    {
        return mRenderQueue.GetSkinnedMeshPayload(idx);
    }
    const MeshPayload& GetMeshPayload(uint32_t idx) const
    {
        return mRenderQueue.GetMeshPayload(idx);
    }

    
protected:
    SDL_Window*   mWindow             { nullptr };

    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------


    float         mWindowDisplayScale { 1.0f };

    //--------------------------------------------------------------------------
    // Screen / Camera
    //--------------------------------------------------------------------------

    float   mScreenWidth   {};
    float   mScreenHeight  {};
    float   mVirtualWidth  {};
    float   mVirtualHeight {};

    float   mPerspectiveFOV { 45.0f };

    Matrix4 mViewMatrix       {};
    Matrix4 mInvView          {};
    Matrix4 mProjectionMatrix {};
    Matrix4 mViewProjMatrix   {};

    //--------------------------------------------------------------------------
    // Debug / Colors
    //--------------------------------------------------------------------------

    Vector3 mClearColor { Vector3(0.2f, 0.5f, 0.8f) };
    Vector3 mWireColor  { Vector3(1.0f, 1.0f, 1.0f) };

    DebugInfo  mDebugOnScreen {};
    DebugInfo  mDebugRTT {};
    DebugInfo* mDebugActiveScreen = &mDebugOnScreen;

    void ChangeDebugOnScreen() { mDebugActiveScreen = &mDebugOnScreen; }
    void ChangeDebugRTT()      { mDebugActiveScreen = &mDebugRTT; }
    void ResetDebugCounter()   { mDebugOnScreen = mDebugRTT = {}; }

    //--------------------------------------------------------------------------
    // Scene capture queue
    //--------------------------------------------------------------------------

    std::vector<SceneCaptureRequest> mSceneCaptureQueue {};

    //--------------------------------------------------------------------------
    // Lighting / Shadow
    //--------------------------------------------------------------------------

    std::shared_ptr<class LightingManager> mLightingManager;

    float mShadowNear        { 10.0f };
    float mShadowFar         { 100.0f };
    float mShadowOrthoWidth  { 100.0f };
    float mShadowOrthoHeight { 100.0f };
    float mShadowBias        { 0.0015f };

    int   mShadowFBOWidth    { 4096 };
    int   mShadowFBOHeight   { 4096 };

    static constexpr int kShadowCascadeCount = 2;

    uint32_t  mShadowFBO[kShadowCascadeCount]  {};
    Matrix4 mLightSpaceMatrix[kShadowCascadeCount] {};
    std::shared_ptr<class Texture> mShadowMapTexture[kShadowCascadeCount];

    float mCascadeSplit0 { 25.0f };
    float mCascadeBlend  { 6.0f };

    //--------------------------------------------------------------------------
    // Post effect
    //--------------------------------------------------------------------------

    // メインシーン（ポスト用）RenderTarget
    std::shared_ptr<IRenderTarget> mSceneRT;

    // ポスト設定
    PostEffectDesc mPost {};

    //--------------------------------------------------------------------------
    // Fade
    //--------------------------------------------------------------------------

    float   mFadeAlpha  { 0.0f }; // 0=表示, 1=完全暗転
    Vector3 mFadeColor  { 0, 0, 0 };
    bool    mEnableFade { false };

    //--------------------------------------------------------------------------
    // Geometry
    //--------------------------------------------------------------------------

    std::shared_ptr<class VertexArray> mFullScreenQuad;
    std::shared_ptr<class VertexArray> mSpriteQuad;
    std::shared_ptr<class VertexArray> mSurfaceQuad;

    void CreateFullScreenQuad();
    void CreateSpriteVerts();
    void CreateSurfaceQuad();

    //--------------------------------------------------------------------------
    // Shaders
    //--------------------------------------------------------------------------

    std::string mShaderPath { "ToyLib/Shaders/" };


    //--------------------------------------------------------------------------
    // Visual components
    //--------------------------------------------------------------------------

    std::vector<class VisualComponent*> mVisualComps;

    //--------------------------------------------------------------------------
    // Internal init
    //--------------------------------------------------------------------------

    bool LoadSettings(const std::string& filePath);
    virtual bool InitializeShadowMapping() { return false; }

    //--------------------------------------------------------------------------
    // OpenGL dispatch helpers (DrawPass layer)
    //--------------------------------------------------------------------------

    void DrawBucket_World(const std::vector<uint32_t>& bucket);
    void DrawBucket_Shadow(const std::vector<uint32_t>& bucket, int cascadeIndex);

    virtual void ApplyState(const RenderItem& it) {};
    virtual void DrawItem(const RenderItem& it, RenderPass pass, int cascadeIndex) {};

    //--------------------------------------------------------------------------
    // DrawPass
    //--------------------------------------------------------------------------

    RenderQueue     mRenderQueue;
    FrameBuckets    mBuckets;

    void BuildFrameQueues();
    void SortBucket(std::vector<uint32_t>& bucket);
    void SortBucket_Shadow(std::vector<uint32_t>& bucket);

    
    virtual bool BeginFrame() = 0;
    
    virtual void DrawShadowPass() = 0;
    virtual void RestoreAfterShadowPass() = 0;

    virtual void DrawSkyPass() = 0;
    virtual void DrawWorldPass() = 0;
    virtual void DrawOverlayScreenPass() = 0;
    virtual void DrawFadePass() = 0;
    virtual void DrawPostEffectPass() = 0;
    virtual void DrawUIPass() = 0;

    virtual void EndFrame() = 0;

    //--------------------------------------------------------------------------
    // Camera stack
    //--------------------------------------------------------------------------

    struct CameraState
    {
        Matrix4 view;
        Matrix4 proj;
        Matrix4 viewProj;
        Matrix4 invView;
    };

    std::vector<CameraState> mCameraStack;

    void PushCameraState();
    void PopCameraState();
    void SetCameraState(const CameraState& s);
    
};

} // namespace toy
