#pragma once

#include "Render/IRenderer.h"
#include <SDL3/SDL.h>

namespace toy
{

class GLRenderer : public IRenderer
{
public:
    GLRenderer();
    virtual ~GLRenderer();
    
    
    //--------------------------------------------------------------------------
    // Initialize / Shutdown
    //--------------------------------------------------------------------------
    bool Initialize(const class Application* app) override;
    void Shutdown() override;
    void UnloadData() override;
    
    void OnWindowResized(int pixelW, int pixelH) override;

    std::shared_ptr<class GLShader> GetShader(const std::string& name);

    PipelineHandle GetPipelineHandle(const std::string& name) override;

    std::shared_ptr<class Texture> GetShadowMapTexture(int cascadeIndex) const override;
    Matrix4 GetLightSpaceMatrix(int cascadeIndex) const override;
    
    void SetClearColor(const Vector3& color) override;
    
    std::shared_ptr<class IRenderTarget> CreateRenderTarget() override;
    
protected:
    void     ApplyState(const RenderItem& it) override;
    void     DrawItem(const RenderItem& it, RenderPass pass, int cascadeIndex) override;
    uint64_t GetPipelineSortKey(const PipelineHandle& h) const override;

    bool InitializeShadowMapping() override;
    
    void DrawToRenderTarget(const struct SceneCaptureRequest& req) override;
    
    bool BeginFrame() override;
    void EndFrame() override;

    void UpdateShadowLightMatrices() override;
    void DrawShadowPass() override;
    void RestoreAfterShadowPass() override;
    
    void DrawSkyPass() override;
    void DrawWorldPass() override;
    void DrawOverlayScreenPass() override;
    void DrawFadePass() override;
    void DrawPostEffectPass() override;
    void DrawUIPass() override;

private:
    SDL_GLContext mGLContext        { nullptr };
    bool          mIsDrawingCapture { false };

    // Shadow mapping リソース（GL 固有 — IRenderer には置かない）
    uint32_t mShadowFBO[kShadowCascadeCount] {};
    std::shared_ptr<class Texture> mShadowMapTexture[kShadowCascadeCount];
    Matrix4  mLightSpaceMatrix[kShadowCascadeCount] {};

    // Scene UBO（フレーム単位のシーンデータを一括転送）
    uint32_t mSceneUBO { 0 };
    void   CreateSceneUBO();
    void   UploadSceneUBO();

    // Shadow Scene UBO（カスケードごとの lightVP。VK の ShadowSceneUBO に相当）
    uint32_t mShadowUBO { 0 };
    void   CreateShadowUBO();
    void   UploadShadowUBO(const Matrix4& lightVP);

    std::unordered_map<std::string, std::shared_ptr<class GLShader>> mShaders;
    bool LoadShaders();
    
};

} // namespace toy
