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
    
    void SetClearColor(const Vector3& color) override;
    
    std::shared_ptr<class IRenderTarget> CreateRenderTarget() override;
protected:
    void ApplyState(const RenderItem& it) override;
    void DrawItem(const RenderItem& it, RenderPass pass, int cascadeIndex) override;
    
    bool InitializeShadowMapping() override;
    
    void DrawToRenderTarget(const struct SceneCaptureRequest& req) override;
    
    bool BeginFrame() override;
    void EndFrame() override;
    
    void DrawShadowPass() override;
    void RestoreAfterShadowPass() override;
    
    void DrawSkyPass() override;
    void DrawWorldPass() override;
    void DrawOverlayScreenPass() override;
    void DrawFadePass() override;
    void DrawPostEffectPass() override;
    void DrawUIPass() override;

private:
    SDL_Window*   mWindow             { nullptr };
    SDL_GLContext mGLContext          { nullptr };
    
    std::unordered_map<std::string, std::shared_ptr<class GLShader>> mShaders;
    bool LoadShaders();

};

} // namespace toy
