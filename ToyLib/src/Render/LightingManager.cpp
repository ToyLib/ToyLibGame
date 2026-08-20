#include "Render/LightingManager.h"
#include "Graphics/Light/PointLightComponent.h"

#include <algorithm>
#include <string>

namespace toy {

//-------------------------------------------------------------
// BuildLightData()
//  バックエンド非依存の POD 収集。GL / VK 両方から利用可能。
//-------------------------------------------------------------
SceneLightData LightingManager::BuildLightData(const Matrix4& viewMatrix) const
{
    SceneLightData d;

    // Camera position（view の逆行列から取得）
    Matrix4 invView = viewMatrix;
    invView.Invert();
    d.cameraPos = invView.GetTranslation();

    // Ambient / Sun
    d.ambientColor = mAmbientColor;
    d.sunIntensity = mSunIntensity;

    // Directional light
    d.dirDirection = mDirLight.GetDirection();
    d.dirDiffuse   = mDirLight.DiffuseColor;
    d.dirSpecular  = mDirLight.SpecColor;

    // Point lights
    const int numAll    = static_cast<int>(mPointLights.size());
    const int maxLights = kMaxScenePointLights;
    int       num       = 0;

    for (int i = 0; i < numAll && num < maxLights; ++i)
    {
        auto* comp = mPointLights[i];
        if (!comp || !comp->IsEnabled()) continue;

        PointLightData& pl = d.pointLights[num++];
        pl.position  = comp->GetPosition();
        pl.color     = comp->GetColor();
        pl.intensity = comp->GetIntensity();
        pl.constant  = comp->GetConstant();
        pl.linear    = comp->GetLinear();
        pl.quadratic = comp->GetQuadratic();
        pl.radius    = comp->GetRadius();
    }
    d.numPointLights = num;

    // Fog
    d.fogMaxDist = mFog.MaxDist;
    d.fogMinDist = mFog.MinDist;
    d.fogColor   = mFog.Color;

    return d;
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
