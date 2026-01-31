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
        DrawItem_GL(it, RenderPass::World, 0);
    }
}

void Renderer::DrawRenderQueue_Shadow(const RenderQueue& queue, int cascadeIndex)
{
    for (const auto& it : queue.Items())
        DrawItem_GL(it, RenderPass::Shadow, cascadeIndex);
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


void Renderer::DrawItem_GL(const RenderItem& it,
                           RenderPass pass,
                           int cascadeIndex /*Shadowの時だけ使う*/)
{
    //===========================================
    // 0) GPUParticle は geometry.ptr を使わない
    //===========================================
    const bool isGPUParticle = (it.type == RenderItemType::GPUParticle);

    if (!isGPUParticle)
    {
        if (!it.geometry.ptr)
            return;

        const bool hasElements = (it.indexCount  > 0);
        const bool hasArrays   = (it.vertexCount > 0);

        if (!hasElements && !hasArrays)
            return;
    }
    else
    {
        // GPUParticle は専用VAO必須
        if (it.gpuVAO == 0 || it.instanceCount <= 0 || it.indexCount <= 0)
            return;
    }

    //===========================
    // 1) State
    //===========================
    ApplyState_GL(it);

    //===========================
    // 2) Shader
    //===========================
    if (!it.shader.ptr)
        return;

    it.shader.ptr->SetActive();

    //===========================
    // 3) 共通 Uniform（World / Shadow）
    //===========================
    if (pass == RenderPass::World || pass == RenderPass::UI)
    {
        it.shader.ptr->SetMatrixUniform("uViewProj",       it.viewProj);
        it.shader.ptr->SetMatrixUniform("uWorldTransform", it.world);
    }
    else if (pass == RenderPass::Shadow)
    {
        it.shader.ptr->SetMatrixUniform("uWorldTransform",  it.world);
        it.shader.ptr->SetMatrixUniform("uLightSpaceMatrix", GetLightSpaceMatrix(cascadeIndex));
    }

    //===========================
    // 4) Type別 Uniform
    //===========================
    switch (it.type)
    {
        case RenderItemType::Sprite:
        {
            if (pass == RenderPass::Shadow)
                return;

            it.shader.ptr->SetVectorUniform("uSpriteColor", it.color);
            it.shader.ptr->SetFloatUniform ("uSpriteAlpha", it.alpha);

            if (it.texture.ptr)
            {
                it.texture.ptr->SetActive(it.textureUnit);
                it.shader.ptr->SetTextureUniform("uTexture", it.textureUnit);
                it.shader.ptr->SetBooleanUniform("uUseTexture", true);
            }
            else
            {
                it.shader.ptr->SetBooleanUniform("uUseTexture", false);
            }
            break;
        }

        case RenderItemType::Mesh:
        {
            if (pass == RenderPass::World)
            {
                if (mLightingManager)
                {
                    Matrix4 view = GetViewMatrix();
                    mLightingManager->ApplyToShader(it.shader.ptr, view);
                }

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
            }
            break;
        }

        case RenderItemType::SkinnedMesh:
        {
            if (pass == RenderPass::World)
            {
                if (mLightingManager)
                {
                    Matrix4 view = GetViewMatrix();
                    mLightingManager->ApplyToShader(it.shader.ptr, view);
                }

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
            }

            if (it.matrixPalette && it.paletteCount > 0)
            {
                it.shader.ptr->SetMatrixUniforms(
                    "uMatrixPalette",
                    it.matrixPalette,
                    static_cast<unsigned int>(it.paletteCount)
                );
            }
            break;
        }

        case RenderItemType::Billboard:
        {
            if (pass == RenderPass::Shadow)
                return;

            if (it.texture.ptr)
            {
                it.texture.ptr->SetActive(it.textureUnit);
                it.shader.ptr->SetTextureUniform("uTexture", it.textureUnit);
                it.shader.ptr->SetBooleanUniform("uUseTexture", true);
            }
            else
            {
                it.shader.ptr->SetBooleanUniform("uUseTexture", false);
            }
            break;
        }

        case RenderItemType::GPUParticle:
        {
            if (pass == RenderPass::Shadow)
                return;

            // ParticleGPU.vert に合わせた uniform
            it.shader.ptr->SetVectorUniform("uCameraRight", it.cameraRight);
            it.shader.ptr->SetVectorUniform("uCameraUp",    it.cameraUp);
            it.shader.ptr->SetFloatUniform ("uLifeMax",     it.particleLifeMax);
            it.shader.ptr->SetFloatUniform ("uSize",        it.particleSize);

            if (it.texture.ptr)
            {
                it.texture.ptr->SetActive(it.textureUnit);
                it.shader.ptr->SetTextureUniform("uTexture", it.textureUnit);
                it.shader.ptr->SetBooleanUniform("uUseTexture", true);
            }
            else
            {
                it.shader.ptr->SetBooleanUniform("uUseTexture", false);
            }
            break;
        }

        case RenderItemType::Debug:
        {
            if (pass == RenderPass::Shadow)
                return;

            it.shader.ptr->SetVectorUniform("uSolColor", it.color);
            break;
        }
    }

    //===========================
    // 5) Draw
    //===========================
    if (it.type == RenderItemType::GPUParticle)
    {
        // ★GPUParticle は専用VAOで Instanced draw
        glBindVertexArray(it.gpuVAO);
        glDrawElementsInstanced(GL_TRIANGLES,
                                it.indexCount,
                                GL_UNSIGNED_INT,
                                nullptr,
                                it.instanceCount);
        glBindVertexArray(0);
    }
    else
    {
        it.geometry.ptr->SetActive();

        GLenum mode = GL_TRIANGLES;
        switch (it.topology)
        {
            case PrimitiveTopology::Triangles: mode = GL_TRIANGLES; break;
            case PrimitiveTopology::Lines:     mode = GL_LINES;     break;
            default:                           mode = GL_TRIANGLES; break;
        }

        if (it.indexCount > 0)
        {
            glDrawElements(mode, it.indexCount, GL_UNSIGNED_INT, nullptr);
        }
        else
        {
            glDrawArrays(mode, 0, it.vertexCount);
        }
    }

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
