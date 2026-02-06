#pragma once

#include "Engine/Render/IRenderer.h"
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

    
protected:
    bool InitializeShadowMapping() override;
    
    void BeginFrame() override;
    void EndFrame() override;
    
private:
    SDL_Window*   mWindow             { nullptr };
    SDL_GLContext mGLContext          { nullptr };
};

} // namespace toy
