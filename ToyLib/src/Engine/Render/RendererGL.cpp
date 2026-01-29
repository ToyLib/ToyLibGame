#include "Engine/Render/Renderer.h"
#include "Engine/Render/RenderQueue.h"
#include "Engine/Render/RenderItem.h"
#include "Asset/Geometry/VertexArray.h"
#include "Asset/Material/Texture.h"
#include "Engine/Render/Shader.h"

#include "glad/glad.h"

namespace toy {

void Renderer::DrawRenderQueue(const RenderQueue& queue)
{
    for (const auto& it : queue.Items())
    {
        DrawItem_GL(it);
    }
}

void Renderer::ApplyState_GL(const RenderItem& it)
{
    // depth
    if (it.depthTest)
        glEnable(GL_DEPTH_TEST);
    else
        glDisable(GL_DEPTH_TEST);
    
    
    glDepthMask(it.depthWrite ? GL_TRUE : GL_FALSE);

    // blend
    if (it.blend == BlendMode::Opaque)
    {
        glDisable(GL_BLEND);
    }
    else
    {
        glEnable(GL_BLEND);
        if (it.blend == BlendMode::Additive)
            glBlendFunc(GL_ONE, GL_ONE);
        else
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }

    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

    // topology は今は Triangles 固定でOK
}

void Renderer::DrawItem_GL(const RenderItem& it)
{
    if (!it.geometry.ptr || it.indexCount <= 0)
        return;

    ApplyState_GL(it);

    // shader
    if (it.shader.ptr)
    {
        it.shader.ptr->SetActive();

        // matrices
        it.shader.ptr->SetMatrixUniform("uViewProj", it.viewProj);
        it.shader.ptr->SetMatrixUniform("uWorldTransform", it.world);

        // sprite uniforms（今はSprite想定）
        it.shader.ptr->SetVectorUniform("uSpriteColor", it.color);
        it.shader.ptr->SetFloatUniform("uSpriteAlpha", it.alpha);

        // texture
        if (it.texture.ptr)
        {
            it.texture.ptr->SetActive(it.textureUnit);
            it.shader.ptr->SetTextureUniform("uTexture", it.textureUnit);
        }
    }

    it.geometry.ptr->SetActive();
    glDrawElements(GL_TRIANGLES, it.indexCount, GL_UNSIGNED_INT, nullptr);

    AddDrawCall();
    AddDrawObject();
}

GeometryHandle Renderer::GetSpriteQuadHandle() const
{
    GeometryHandle h;
    h.ptr = mSpriteVerts.get();       // 既存の共通SpriteVertsを使う
    return h;
}

ShaderHandle Renderer::GetShaderHandle(const std::string& name)
{
    ShaderHandle h;
    auto sp = GetShader(name);        // 既存：shared_ptr<Shader>
    h.ptr = sp.get();
    return h;
}

TextureHandle Renderer::ToHandle(const std::shared_ptr<Texture>& tex) const
{
    TextureHandle h;
    h.ptr = tex.get();
    return h;
}

} // namespace toy
