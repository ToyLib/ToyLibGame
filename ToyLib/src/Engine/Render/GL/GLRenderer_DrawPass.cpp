#include "Engine/Render/GL/GLRenderer.h"
#include "Engine/Render/RenderTarget.h"


namespace toy {

//==============================================================================
// Frame begin/end
//==============================================================================

void GLRenderer::BeginFrame()
{
    const bool usePost = (mPost.type != PostEffectType::None);

    if (usePost)
    {
        if (!mSceneRT ||
            mSceneRT->GetWidth()  != (int)mScreenWidth ||
            mSceneRT->GetHeight() != (int)mScreenHeight)
        {
            mSceneRT = std::make_shared<RenderTarget>();
            mSceneRT->Create((int)mScreenWidth, (int)mScreenHeight);
        }

        mSceneRT->Bind();
    }
    else
    {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, (GLsizei)mScreenWidth, (GLsizei)mScreenHeight);
    }

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void GLRenderer::EndFrame()
{
    SDL_GL_SwapWindow(mWindow);
}




} // namespace toy
