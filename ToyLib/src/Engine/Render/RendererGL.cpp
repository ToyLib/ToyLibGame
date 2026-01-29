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

    // shader must exist for now
    if (!it.shader.ptr)
        return;

    it.shader.ptr->SetActive();

    // matrices
    it.shader.ptr->SetMatrixUniform("uViewProj", it.viewProj);
    it.shader.ptr->SetMatrixUniform("uWorldTransform", it.world);

    // item-type specific
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
            // lighting（Rendererが持つ view を使う）
            if (mLightingManager)
            {
                Matrix4 view = GetViewMatrix();
                mLightingManager->ApplyToShader(it.shader.ptr, view);
            }
            
            // toon（今は RenderItem に無いので “常にfalse” にしておくか、
            // いったん MeshComponent をトゥーン無し運用にする）
            it.shader.ptr->SetBooleanUniform("uUseToon", false);
            
            // material bind
            if (it.material.ptr)
            {
                it.material.ptr->BindToShader(it.shader.ptr, 0);
            }
            break;
        }
        case RenderItemType::SkinnedMesh:
        /*{
            if (mLightingManager)
            {
                Matrix4 view = GetViewMatrix();
                mLightingManager->ApplyToShader(it.shader.ptr, view);
            }

            it.shader.ptr->SetBooleanUniform("uUseToon", false);

            // ★ボーンパレット
            if (it.matrixPalette && it.paletteCount > 0)
            {
                it.shader.ptr->SetMatrixUniforms(
                    "uMatrixPalette",
                    it.matrixPalette,
                    static_cast<unsigned int>(it.paletteCount)
                );
            }

            if (it.material.ptr)
            {
                it.material.ptr->BindToShader(it.shader.ptr, 0);
            }
            break;
        }*/
        case RenderItemType::Debug:
            break;
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
MaterialHandle Renderer::ToHandle(const std::shared_ptr<Material>& mat) const
{
    MaterialHandle h;
    h.ptr = mat.get();
    return h;
}
} // namespace toy
