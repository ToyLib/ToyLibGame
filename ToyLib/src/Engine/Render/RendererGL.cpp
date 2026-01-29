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
    if (it.depthTest) glEnable(GL_DEPTH_TEST);
    else             glDisable(GL_DEPTH_TEST);

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



void Renderer::DrawItem_GL(const RenderItem& it)
{
    if (!it.geometry.ptr || it.indexCount <= 0)
        return;

    ApplyState_GL(it);

    if (!it.shader.ptr)
        return;

    it.shader.ptr->SetActive();

    // matrices
    it.shader.ptr->SetMatrixUniform("uViewProj", it.viewProj);
    it.shader.ptr->SetMatrixUniform("uWorldTransform", it.world);

    switch (it.type)
    {
        case RenderItemType::Sprite:
        {
            it.shader.ptr->SetVectorUniform("uSpriteColor", it.color);
            it.shader.ptr->SetFloatUniform("uSpriteAlpha", it.alpha);

            if (it.texture.ptr)
            {
                it.texture.ptr->SetActive(it.textureUnit);
                it.shader.ptr->SetTextureUniform("uTexture", it.textureUnit);
            }
            break;
        }
        case RenderItemType::Mesh:
        {
            // lighting
            if (mLightingManager)
            {
                Matrix4 view = GetViewMatrix();
                mLightingManager->ApplyToShader(it.shader.ptr, view);
            }

            // ★ shadow (Phong系が参照してるなら必須)
            {
                auto sm0 = GetShadowMapTexture(0);
                auto sm1 = GetShadowMapTexture(1);

                if (sm0) sm0->SetActive(6);
                if (sm1) sm1->SetActive(7);

                it.shader.ptr->SetTextureUniform("uShadowMap0", 6);
                it.shader.ptr->SetTextureUniform("uShadowMap1", 7);

                it.shader.ptr->SetMatrixUniform("uLightViewProj0", GetLightSpaceMatrix(0));
                it.shader.ptr->SetMatrixUniform("uLightViewProj1", GetLightSpaceMatrix(1));

                it.shader.ptr->SetFloatUniform("uCascadeSplit0", GetCascadeSplit0());
                it.shader.ptr->SetFloatUniform("uCascadeBlend",  GetCascadeBlend());
                it.shader.ptr->SetFloatUniform("uShadowBias",    0.005f);
            }

            it.shader.ptr->SetBooleanUniform("uUseToon", it.toon);

            if (it.material.ptr)
            {
                it.material.ptr->BindToShader(it.shader.ptr, 0);
            }
            break;
        }
        case RenderItemType::SkinnedMesh:
        {
            if (mLightingManager)
            {
                Matrix4 view = GetViewMatrix();
                mLightingManager->ApplyToShader(it.shader.ptr, view);
            }
            
            // ★ shadow (Phong系が参照してるなら必須)
            {
                auto sm0 = GetShadowMapTexture(0);
                auto sm1 = GetShadowMapTexture(1);

                if (sm0) sm0->SetActive(6);
                if (sm1) sm1->SetActive(7);

                it.shader.ptr->SetTextureUniform("uShadowMap0", 6);
                it.shader.ptr->SetTextureUniform("uShadowMap1", 7);

                it.shader.ptr->SetMatrixUniform("uLightViewProj0", GetLightSpaceMatrix(0));
                it.shader.ptr->SetMatrixUniform("uLightViewProj1", GetLightSpaceMatrix(1));

                it.shader.ptr->SetFloatUniform("uCascadeSplit0", GetCascadeSplit0());
                it.shader.ptr->SetFloatUniform("uCascadeBlend",  GetCascadeBlend());
                it.shader.ptr->SetFloatUniform("uShadowBias",    0.005f);
            }

            it.shader.ptr->SetBooleanUniform("uUseToon", it.toon);

            if (it.material.ptr)
                it.material.ptr->BindToShader(it.shader.ptr, 0);

            // ★ここが本体
            if (it.matrixPalette && it.paletteCount > 0)
            {
                it.shader.ptr->SetMatrixUniforms(
                    "uMatrixPalette",
                    it.matrixPalette,
                    (unsigned int)it.paletteCount
                );
            }
            break;
        }
        case RenderItemType::Debug:
            return;
    }

    // VAO bind
    it.geometry.ptr->SetActive();

    glDrawElements(GL_TRIANGLES, (GLsizei)it.indexCount, GL_UNSIGNED_INT, nullptr);

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
MaterialHandle Renderer::ToHandle(const std::shared_ptr<Material>& mat) const
{
    MaterialHandle h;
    h.ptr = mat.get();
    return h;
}
} // namespace toy
