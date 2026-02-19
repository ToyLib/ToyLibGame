#include "Render/LightingManager.h"
#include "Render/GL/GLShader.h"
#include "Render/GL/UniformNamesGL.h"
#include "Graphics/Light/PointLightComponent.h"

#include <algorithm>
#include <string>

namespace toy {

//-------------------------------------------------------------
// ApplyToShader()
//-------------------------------------------------------------
void LightingManager::ApplyToShader(std::shared_ptr<GLShader> shader,
                                    const Matrix4& viewMatrix)
{
    ApplyToShader(shader.get(), viewMatrix);
}

void LightingManager::ApplyToShader(GLShader* shader,
                                    const Matrix4& viewMatrix)
{
    if (!shader) return;

    using namespace toy::glsl;

    //---------------------------------------------------------
    // Camera
    //---------------------------------------------------------
    Matrix4 invView = viewMatrix;
    invView.Invert();

    shader->SetVectorUniform(Scene::CameraPos, invView.GetTranslation());

    //---------------------------------------------------------
    // Ambient / Sun
    //---------------------------------------------------------
    shader->SetVectorUniform(Scene::AmbientLight, mAmbientColor);
    shader->SetFloatUniform (Scene::SunIntensity, mSunIntensity);

    //---------------------------------------------------------
    // Directional light
    //---------------------------------------------------------
    shader->SetVectorUniform(Scene::Dir_Direction, mDirLight.GetDirection());
    shader->SetVectorUniform(Scene::Dir_Diffuse,   mDirLight.DiffuseColor);
    shader->SetVectorUniform(Scene::Dir_Specular,  mDirLight.SpecColor);

    //---------------------------------------------------------
    // Point lights
    //---------------------------------------------------------
    const int maxPointLights = 8;

    int numAll = static_cast<int>(mPointLights.size());
    if (numAll > maxPointLights)
    {
        numAll = maxPointLights;
    }

    int num = 0;

    for (int i = 0; i < numAll; ++i)
    {
        auto* comp = mPointLights[i];
        if (!comp || !comp->IsEnabled())
        {
            continue;
        }

        const int idx = num++;

        const std::string base =
            std::string(Scene::PointPrefix) + std::to_string(idx) + "].";

        shader->SetVectorUniform((base + "position").c_str(),  comp->GetPosition());
        shader->SetVectorUniform((base + "color").c_str(),     comp->GetColor());
        shader->SetFloatUniform ((base + "intensity").c_str(), comp->GetIntensity());
        shader->SetFloatUniform ((base + "constant").c_str(),  comp->GetConstant());
        shader->SetFloatUniform ((base + "linear").c_str(),    comp->GetLinear());
        shader->SetFloatUniform ((base + "quadratic").c_str(), comp->GetQuadratic());
        shader->SetFloatUniform ((base + "radius").c_str(),    comp->GetRadius());
    }

    shader->SetIntUniform(Scene::NumPointLights, num);

    //---------------------------------------------------------
    // Fog
    //---------------------------------------------------------
    shader->SetFloatUniform (Scene::Fog_MaxDist, mFog.MaxDist);
    shader->SetFloatUniform (Scene::Fog_MinDist, mFog.MinDist);
    shader->SetVectorUniform(Scene::Fog_Color,   mFog.Color);
}

void LightingManager::RegisterPointLight(PointLightComponent* light)
{
    if (!light) return;

    auto it = std::find(mPointLights.begin(), mPointLights.end(), light);
    if (it == mPointLights.end())
    {
        mPointLights.emplace_back(light);
    }
}

void LightingManager::UnregisterPointLight(PointLightComponent* light)
{
    if (!light) return;

    auto it = std::find(mPointLights.begin(), mPointLights.end(), light);
    if (it != mPointLights.end())
    {
        mPointLights.erase(it);
    }
}

} // namespace toy
