#include "Render/LightingManager.h"
#include "Render/GL/Shader.h"
#include "Graphics/Light/PointLightComponent.h"

#include <algorithm>

namespace toy {

//-------------------------------------------------------------
// ApplyToShader()
// ・現在のライティング関連パラメーターを GLSL シェーダーに送る
// ・Renderer → 各 VisualComponent 描画時に呼ばれる想定
//-------------------------------------------------------------
// LightingManager.cpp
void LightingManager::ApplyToShader(std::shared_ptr<Shader> shader,
                                    const Matrix4& viewMatrix)
{
    ApplyToShader(shader.get(), viewMatrix);
}

void LightingManager::ApplyToShader(Shader* shader,
                                    const Matrix4& viewMatrix)
{
    if (!shader) return;

    Matrix4 invView = viewMatrix;
    invView.Invert();
    shader->SetVectorUniform("uCameraPos", invView.GetTranslation());

    shader->SetVectorUniform("uAmbientLight", mAmbientColor);
    shader->SetFloatUniform ("uSunIntensity", mSunIntensity);

    shader->SetVectorUniform("uDirLight.mDirection",    mDirLight.GetDirection());
    shader->SetVectorUniform("uDirLight.mDiffuseColor", mDirLight.DiffuseColor);
    shader->SetVectorUniform("uDirLight.mSpecColor",    mDirLight.SpecColor);

    const int maxPointLights = 8;
    int numAll = (int)mPointLights.size();
    if (numAll > maxPointLights) numAll = maxPointLights;

    int num = 0;
    for (int i = 0; i < numAll; ++i)
    {
        auto* comp = mPointLights[i];
        if (!comp || !comp->IsEnabled()) continue;

        int idx = num++;
        std::string base = "uPointLights[" + std::to_string(idx) + "].";

        shader->SetVectorUniform((base + "position").c_str(),  comp->GetPosition());
        shader->SetVectorUniform((base + "color").c_str(),     comp->GetColor());
        shader->SetFloatUniform ((base + "intensity").c_str(), comp->GetIntensity());
        shader->SetFloatUniform ((base + "constant").c_str(),  comp->GetConstant());
        shader->SetFloatUniform ((base + "linear").c_str(),    comp->GetLinear());
        shader->SetFloatUniform ((base + "quadratic").c_str(), comp->GetQuadratic());
        shader->SetFloatUniform ((base + "radius").c_str(),    comp->GetRadius());
    }
    shader->SetIntUniform("uNumPointLights", num);

    shader->SetFloatUniform ("uFoginfo.maxDist", mFog.MaxDist);
    shader->SetFloatUniform ("uFoginfo.minDist", mFog.MinDist);
    shader->SetVectorUniform("uFoginfo.color",   mFog.Color);
}


void LightingManager::RegisterPointLight(PointLightComponent* light)
{
    if (!light)
    {
        return;
    }
    auto it = std::find(mPointLights.begin(), mPointLights.end(), light);
    if (it == mPointLights.end())
    {
        mPointLights.emplace_back(light);
    }
}

void LightingManager::UnregisterPointLight(PointLightComponent* light)
{
    if (!light)
    {
        return;
    }
    auto it = std::find(mPointLights.begin(), mPointLights.end(), light);
    if (it != mPointLights.end())
    {
        mPointLights.erase(it);
    }
}

} // namespace toy
