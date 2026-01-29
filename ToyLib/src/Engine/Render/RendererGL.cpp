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

#include <iostream>
#include <string>
#include <vector>
#include <iomanip>

static const char* GLBoolStr(GLint v) { return (v == GL_TRUE) ? "TRUE" : "FALSE"; }

static void DumpSamplerUnits(GLuint program, const std::vector<const char*>& names)
{
    for (auto* n : names)
    {
        GLint loc = glGetUniformLocation(program, n);
        if (loc < 0)
        {
            std::cerr << "  [Sampler] " << n << " loc=-1\n";
            continue;
        }
        GLint unit = -999;
        glGetUniformiv(program, loc, &unit);
        std::cerr << "  [Sampler] " << n << " unit=" << unit << "\n";
    }
}

static void DumpProgramStatus(GLuint program)
{
    if (program == 0)
    {
        std::cerr << "[GL] Program=0 (no current program)\n";
        return;
    }

    GLint linked = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);

    GLint validated = 0;
    glGetProgramiv(program, GL_VALIDATE_STATUS, &validated);

    GLint activeUniforms = 0;
    glGetProgramiv(program, GL_ACTIVE_UNIFORMS, &activeUniforms);

    GLint activeAttribs = 0;
    glGetProgramiv(program, GL_ACTIVE_ATTRIBUTES, &activeAttribs);

    std::cerr << "[GL] Program=" << program
              << " LINK=" << GLBoolStr(linked)
              << " VALIDATE=" << GLBoolStr(validated)
              << " uniforms=" << activeUniforms
              << " attribs=" << activeAttribs
              << "\n";

    if (!linked)
    {
        GLint logLen = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logLen);
        if (logLen > 1)
        {
            std::string log;
            log.resize((size_t)logLen);
            GLsizei outLen = 0;
            glGetProgramInfoLog(program, logLen, &outLen, log.data());
            std::cerr << "[GL] Program Link Log:\n" << log << "\n";
        }
    }

    if (!validated)
    {
        GLint logLen = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logLen);
        if (logLen > 1)
        {
            std::string log;
            log.resize((size_t)logLen);
            GLsizei outLen = 0;
            glGetProgramInfoLog(program, logLen, &outLen, log.data());
            std::cerr << "[GL] Program Validate Log:\n" << log << "\n";
        }
    }
}

static void DumpVAOState(GLuint vao)
{
    if (vao == 0)
    {
        std::cerr << "[GL] VAO=0 (no VAO bound)\n";
        return;
    }

    GLint ebo = 0;
    glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &ebo);

    std::cerr << "[GL] VAO=" << vao
              << " EBO=" << ebo
              << "\n";

    // location 0..4 だけ見る（ToyLibの基本想定）
    for (int loc = 0; loc <= 4; ++loc)
    {
        GLint enabled = 0;
        glGetVertexAttribiv(loc, GL_VERTEX_ATTRIB_ARRAY_ENABLED, &enabled);

        GLint size = 0;
        glGetVertexAttribiv(loc, GL_VERTEX_ATTRIB_ARRAY_SIZE, &size);

        GLint type = 0;
        glGetVertexAttribiv(loc, GL_VERTEX_ATTRIB_ARRAY_TYPE, &type);

        GLint normalized = 0;
        glGetVertexAttribiv(loc, GL_VERTEX_ATTRIB_ARRAY_NORMALIZED, &normalized);

        GLint stride = 0;
        glGetVertexAttribiv(loc, GL_VERTEX_ATTRIB_ARRAY_STRIDE, &stride);

        GLint bufBinding = 0;
        glGetVertexAttribiv(loc, GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING, &bufBinding);

        void* ptr = nullptr;
        glGetVertexAttribPointerv(loc, GL_VERTEX_ATTRIB_ARRAY_POINTER, &ptr);

        std::cerr << "  [Attrib" << loc << "] enabled=" << enabled
                  << " size=" << size
                  << " type=0x" << std::hex << type << std::dec
                  << " norm=" << normalized
                  << " stride=" << stride
                  << " vbo=" << bufBinding
                  << " ptr=" << ptr
                  << "\n";
    }
}

static void DumpUniformLocations(GLuint program, const std::vector<const char*>& names)
{
    if (program == 0) return;

    for (auto* n : names)
    {
        GLint loc = glGetUniformLocation(program, n);
        std::cerr << "  [Uniform] " << n << " loc=" << loc << "\n";
    }
}

static void DumpDrawPrecheck(GLsizei indexCount)
{
    GLint prog = 0;
    GLint vao  = 0;
    GLint ebo  = 0;

    glGetIntegerv(GL_CURRENT_PROGRAM, &prog);
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vao);
    glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &ebo);

    std::cerr << "[PreDraw] Program=" << prog
              << " VAO=" << vao
              << " EBO=" << ebo
              << " indexCount=" << indexCount
              << "\n";

    // core profile: VAO must be bound for glDrawElements
    if (vao == 0)
        std::cerr << "  !! VAO is 0 (invalid for glDrawElements in core profile)\n";

    if (ebo == 0)
        std::cerr << "  !! EBO is 0 (indices missing)\n";
}

static void DumpGLError(const char* tag)
{
    for (;;)
    {
        GLenum err = glGetError();
        if (err == GL_NO_ERROR) break;
        std::cerr << "[GL ERROR] " << tag << " err=0x" << std::hex << err << std::dec << "\n";
    }
}



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

    // まずエラー掃除（前フレームの残骸を消す）
    DumpGLError("Before SetActive");

    it.shader.ptr->SetActive();

    // 現在のProgram確認
    GLint prog = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &prog);
    std::cerr << "[Draw] type=" << (int)it.type
              << " layer=" << (int)it.layer
              << " Prog=" << prog
              << " idxCount=" << it.indexCount
              << "\n";

    // Program状態（LINK/VALIDATE）も見る
    DumpProgramStatus((GLuint)prog);

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

            it.shader.ptr->SetBooleanUniform("uUseToon", false);

            if (it.material.ptr)
            {
                it.material.ptr->BindToShader(it.shader.ptr, 0);
            }
            break;
        }
        case RenderItemType::SkinnedMesh:
            return; // 未対応なら描かない（事故防止）
        case RenderItemType::Debug:
            return;
    }

    // VAO bind
    it.geometry.ptr->SetActive();

    DumpSamplerUnits((GLuint)prog, {
        "uTexture",
        "uShadowMap0",
        "uShadowMap1"
    });

    glDrawElements(GL_TRIANGLES, (GLsizei)it.indexCount, GL_UNSIGNED_INT, nullptr);

    DumpGLError("After glDrawElements");

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
