//==============================================================================
// Renderer_Shader.cpp
//  - LoadShaders
//  - GetShader
//==============================================================================

#include "Render/IRenderer.h"

#include "Render/Shader.h"
#include "glad/glad.h"

#include <memory>
#include <string>

namespace toy {

bool IRenderer::LoadShaders()
{
    std::string vShaderName;
    std::string fShaderName;

    // Weather overlay
    vShaderName = mShaderPath + "WeatherScreen.vert";
    fShaderName = mShaderPath + "WeatherScreen.frag";
    mShaders["WeatherOverlay"] = std::make_shared<Shader>();
    if (!mShaders["WeatherOverlay"]->Load(vShaderName.c_str(), fShaderName.c_str()))
        return false;

    // PostEffect
    vShaderName = mShaderPath + "PostEffect.vert";
    fShaderName = mShaderPath + "PostEffect.frag";
    mShaders["PostEffect"] = std::make_shared<Shader>();
    if (!mShaders["PostEffect"]->Load(vShaderName.c_str(), fShaderName.c_str()))
        return false;

    // Fade
    vShaderName = mShaderPath + "PostEffect.vert";
    fShaderName = mShaderPath + "Fade.frag";
    mShaders["Fade"] = std::make_shared<Shader>();
    if (!mShaders["Fade"]->Load(vShaderName.c_str(), fShaderName.c_str()))
        return false;

    // Mesh (Phong)
    vShaderName = mShaderPath + "Phong.vert";
    fShaderName = mShaderPath + "Phong.frag";
    mShaders["Mesh"] = std::make_shared<Shader>();
    if (!mShaders["Mesh"]->Load(vShaderName.c_str(), fShaderName.c_str()))
        return false;

    // Skinned
    vShaderName = mShaderPath + "Skinned.vert";
    fShaderName = mShaderPath + "Phong.frag";
    mShaders["Skinned"] = std::make_shared<Shader>();
    if (!mShaders["Skinned"]->Load(vShaderName.c_str(), fShaderName.c_str()))
        return false;

    // Unlit
    vShaderName = mShaderPath + "Unlit.vert";
    fShaderName = mShaderPath + "Unlit.frag";
    mShaders["Unlit"] = std::make_shared<Shader>();
    if (!mShaders["Unlit"]->Load(vShaderName.c_str(), fShaderName.c_str()))
        return false;

    // Sprite
    vShaderName = mShaderPath + "Sprite.vert";
    fShaderName = mShaderPath + "Sprite.frag";
    mShaders["Sprite"] = std::make_shared<Shader>();
    if (!mShaders["Sprite"]->Load(vShaderName.c_str(), fShaderName.c_str()))
        return false;

    // Sprite initial 2D ViewProj
    mShaders["Sprite"]->SetActive();
    {
        Matrix4 viewProj = Matrix4::CreateSimpleViewProj(mScreenWidth, mScreenHeight);
        mShaders["Sprite"]->SetMatrixUniform("uViewProj", viewProj);
    }

    // GPU particle update (Transform Feedback)
    vShaderName = mShaderPath + "ParticleUpdate.vert";
    {
        auto update = std::make_shared<Shader>();
        update->LoadWithTransformFeedback(
            vShaderName,
            "", // fragなし
            { "tfPos", "tfVel", "tfLife" },
            GL_INTERLEAVED_ATTRIBS
        );
        mShaders["ParticleUpdate"] = update;
    }

    // Particle render
    vShaderName = mShaderPath + "Particle.vert";
    fShaderName = mShaderPath + "Particle.frag";
    {
        auto render = std::make_shared<Shader>();
        render->Load(vShaderName, fShaderName);
        mShaders["Particle"] = render;
    }

    // SolidColor
    vShaderName = mShaderPath + "BasicMesh.vert";
    fShaderName = mShaderPath + "SolidColor.frag";
    mShaders["Solid"] = std::make_shared<Shader>();
    if (!mShaders["Solid"]->Load(vShaderName.c_str(), fShaderName.c_str()))
        return false;

    // Shadow (Skinned)
    vShaderName = mShaderPath + "ShadowMapping_Skinned.vert";
    fShaderName = mShaderPath + "ShadowMapping.frag";
    mShaders["ShadowSkinned"] = std::make_shared<Shader>();
    if (!mShaders["ShadowSkinned"]->Load(vShaderName.c_str(), fShaderName.c_str()))
        return false;

    // Shadow (Mesh)
    vShaderName = mShaderPath + "ShadowMapping_Mesh.vert";
    fShaderName = mShaderPath + "ShadowMapping.frag";
    mShaders["ShadowMesh"] = std::make_shared<Shader>();
    if (!mShaders["ShadowMesh"]->Load(vShaderName.c_str(), fShaderName.c_str()))
        return false;

    // RenderSurface
    vShaderName = mShaderPath + "RenderSurface.vert";
    fShaderName = mShaderPath + "RenderSurface.frag";
    mShaders["RenderSurface"] = std::make_shared<Shader>();
    if (!mShaders["RenderSurface"]->Load(vShaderName.c_str(), fShaderName.c_str()))
        return false;

    // SkyDome
    vShaderName = mShaderPath + "WeatherDome.vert";
    fShaderName = mShaderPath + "WeatherDome.frag";
    mShaders["SkyDome"] = std::make_shared<Shader>();
    if (!mShaders["SkyDome"]->Load(vShaderName.c_str(), fShaderName.c_str()))
        return false;

    // Default view/proj
    mViewMatrix = Matrix4::CreateLookAt(
        Vector3(0, 0.5f, -3),
        Vector3(0, 0, 10),
        Vector3::UnitY
    );
    mProjectionMatrix = Matrix4::CreatePerspectiveFOV(
        Math::ToRadians(mPerspectiveFOV),
        mScreenWidth,
        mScreenHeight,
        1.0f,
        2000.0f
    );

    return true;
}

std::shared_ptr<Shader> IRenderer::GetShader(const std::string& name)
{
    auto itr = mShaders.find(name);
    return (itr != mShaders.end()) ? itr->second : nullptr;
}

} // namespace toy
