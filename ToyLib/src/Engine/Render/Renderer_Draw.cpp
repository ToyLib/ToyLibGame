#include "Engine/Render/Renderer.h"
#include "Engine/Render/RenderQueue.h"
#include "Engine/Render/RenderItem.h"
#include "Asset/Geometry/VertexArray.h"
#include "Asset/Material/Texture.h"
#include "Engine/Render/Shader.h"
#include "Asset/Material/Material.h"
#include "Engine/Render/LightingManager.h"

#include "glad/glad.h"

namespace toy {


void Renderer::DrawRenderQueue_World(const RenderQueue& queue)
{
    for (const auto& it : queue.Items())
    {
        SDL_assert(it.dispatch && "RenderItem.dispatch must be set");
        DrawItem_GL(it, RenderPass::World, -1);
    }
}

void Renderer::DrawRenderQueue_Shadow(const RenderQueue& queue, int cascadeIndex)
{
    for (const auto& it : queue.Items())
    {
        SDL_assert(it.dispatch && "RenderItem.dispatch must be set");
        DrawItem_GL(it, RenderPass::Shadow, cascadeIndex);
    }
}


void Renderer::ApplyState_GL(const RenderItem& it)
{
    // depth
    if (it.depthTest) glEnable(GL_DEPTH_TEST);
    else             glDisable(GL_DEPTH_TEST);

    // ★追加：SkyDome は LEQUAL（遠方Z=1.0でも描ける）
    if (it.type == RenderItemType::SkyDome)
        glDepthFunc(GL_LEQUAL);
    else
        glDepthFunc(GL_LESS);

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

    // cull
    if (it.cull == CullMode::None)
    {
        glDisable(GL_CULL_FACE);
    }
    else
    {
        glEnable(GL_CULL_FACE);
        glCullFace(it.cull == CullMode::Back ? GL_BACK : GL_FRONT);
    }

    // front face
    glFrontFace(it.frontFace == FrontFace::CCW ? GL_CCW : GL_CW);

    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
}

GeometryHandle Renderer::GetSpriteQuadHandle() const
{
    GeometryHandle h;
    h.ptr = mSpriteQuad.get();       // 既存の共通SpriteVertsを使う
    return h;
}
GeometryHandle Renderer::GetSurfaceQuadHandle() const
{
    GeometryHandle h;
    h.ptr = mSurfaceQuad.get();   // 既に CreateSurfaceQuad() で用意してる前提
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
MaterialHandle Renderer::ToHandle(const std::shared_ptr<Material>& mat) const
{
    MaterialHandle h;
    h.ptr = mat.get();
    return h;
}



// ----------------------------------------
// 共通：geometry / count の検証
// ----------------------------------------
inline bool ValidateGeometryForDraw(const RenderItem& it)
{
    const bool isGPUParticle = (it.type == RenderItemType::Particle);

    if (!isGPUParticle)
    {
        if (!it.geometry.ptr) return false;

        const bool hasElements = (it.indexCount  > 0);
        const bool hasArrays   = (it.vertexCount > 0);
        if (!hasElements && !hasArrays) return false;
    }
    else
    {
        if (it.gpuVAO == 0 || it.instanceCount <= 0 || it.indexCount <= 0)
            return false;
    }
    return true;
}

// ----------------------------------------
// 共通：World/UI/Shadow の共通Uniform
// ----------------------------------------
inline void SetCommonUniforms(Renderer& r,
                             const RenderItem& it,
                             RenderPass pass,
                             int cascadeIndex)
{
    auto* sh = it.shader.ptr;
    if (!sh) return;

    if (pass == RenderPass::World || pass == RenderPass::UI)
    {
        sh->SetMatrixUniform("uViewProj",       it.viewProj);
        sh->SetMatrixUniform("uWorldTransform", it.world);
    }
    else if (pass == RenderPass::Shadow)
    {
        sh->SetMatrixUniform("uWorldTransform",  it.world);
        sh->SetMatrixUniform("uLightSpaceMatrix", r.GetLightSpaceMatrix(cascadeIndex));
    }
}

// ----------------------------------------
// 共通：通常Draw（geometry/Topology/Index/Array）
// ----------------------------------------
inline void DrawDefaultGeometry_GL(Renderer& r, const RenderItem& it)
{
    if (it.type == RenderItemType::Particle)
    {
        glBindVertexArray(it.gpuVAO);
        glDrawElementsInstanced(GL_TRIANGLES,
                                it.indexCount,
                                GL_UNSIGNED_INT,
                                nullptr,
                                it.instanceCount);
        glBindVertexArray(0);
        r.AddDrawCall();
        r.AddDrawObject();
        return;
    }

    it.geometry.ptr->SetActive();

    GLenum mode = GL_TRIANGLES;
    switch (it.topology)
    {
        case PrimitiveTopology::Triangles: mode = GL_TRIANGLES; break;
        case PrimitiveTopology::Lines:     mode = GL_LINES;     break;
        default:                           mode = GL_TRIANGLES; break;
    }

    if (it.indexCount > 0)
        glDrawElements(mode, it.indexCount, GL_UNSIGNED_INT, nullptr);
    else
        glDrawArrays(mode, 0, it.vertexCount);

    r.AddDrawCall();
    r.AddDrawObject();
}


void Renderer::DrawItem_GL(const RenderItem& it, RenderPass pass, int cascadeIndex)
{
    // 0) Validate
    if (!ValidateGeometryForDraw(it))
        return;

    // 1) State
    ApplyState_GL(it);

    // 2) Shader
    if (!it.shader.ptr)
        return;
    it.shader.ptr->SetActive();

    // 3) Common uniforms
    SetCommonUniforms(*this, it, pass, cascadeIndex);

    // 4) Dispatch（Type別）
    if (it.dispatch)
    {
        const bool alreadyDrawn = it.dispatch(*this, it, pass, cascadeIndex);
        if (alreadyDrawn)
            return;
    }

    // 5) Default draw
    DrawDefaultGeometry_GL(*this, it);
}

} // namespace toy
