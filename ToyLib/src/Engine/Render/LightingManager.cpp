#include "Engine/Render/LightingManager.h"
#include "Engine/Render/Shader.h"
#include "Graphics/Light/PointLightComponent.h"

namespace toy {

//-------------------------------------------------------------
// コンストラクタ
// ・太陽光の強さはデフォルト 1.0
//-------------------------------------------------------------
LightingManager::LightingManager()
: mSunIntensity(1.0f)
{
}

LightingManager::~LightingManager()
{
}


//-------------------------------------------------------------
// ApplyToShader()
// ・現在のライティング関連パラメーターを GLSL シェーダーに送る
// ・Renderer → 各 VisualComponent 描画時に呼ばれる想定
//-------------------------------------------------------------
void LightingManager::ApplyToShader(std::shared_ptr<Shader> shader,
                                    const Matrix4& viewMatrix)
{
    //---------------------------------------------------------
    // カメラ位置（シェーダーで Specular 計算等に利用）
    // ・View 行列の逆行列からカメラのワールド位置を取得
    //---------------------------------------------------------
    Matrix4 invView = viewMatrix;
    invView.Invert();
    shader->SetVectorUniform("uCameraPos", invView.GetTranslation());
    
    //---------------------------------------------------------
    // アンビエントライト（全体のベースカラー）
    //---------------------------------------------------------
    shader->SetVectorUniform("uAmbientLight", mAmbientColor);
    
    //---------------------------------------------------------
    // 太陽光の強さ（スカイドーム・シーン全体の明るさ調整）
    //---------------------------------------------------------
    shader->SetFloatUniform("uSunIntensity", mSunIntensity);
    
    //---------------------------------------------------------
    // ディレクショナルライト
    // ・mDirection は常に Normalize(Target - Position)
    //---------------------------------------------------------
    shader->SetVectorUniform("uDirLight.mDirection",     mDirLight.GetDirection());
    shader->SetVectorUniform("uDirLight.mDiffuseColor",  mDirLight.DiffuseColor);
    shader->SetVectorUniform("uDirLight.mSpecColor",     mDirLight.SpecColor);
    
    // --- Point Light 適用 ---
    const int maxPointLights = 8;
    int numAll = static_cast<int>(mPointLights.size());
    if (numAll > maxPointLights) numAll = maxPointLights;

    int num = 0;
    for (int i = 0; i < numAll; ++i)
    {
        auto* comp = mPointLights[i];
        if (!comp || !comp->IsEnabled())
            continue;

        // 有効なライトだけを詰めていく
        int idx = num++;
        std::string base = "uPointLights[" + std::to_string(idx) + "].";
        
        std::string name;
        name = base + "position";
        shader->SetVectorUniform(name.c_str(), comp->GetPosition());
        name = base + "color";
        shader->SetVectorUniform(name.c_str(), comp->GetColor());
        name = base + "intensity";
        shader->SetFloatUniform(name.c_str(), comp->GetIntensity());
        name = base + "constant";
        shader->SetFloatUniform(name.c_str(), comp->GetConstant());
        name = base + "linear";
        shader->SetFloatUniform(name.c_str(), comp->GetLinear());
        name = base + "quadratic";
        shader->SetFloatUniform(name.c_str(), comp->GetQuadratic());
        name = base + "radius";
        shader->SetFloatUniform(name.c_str(), comp->GetRadius());
    }
    shader->SetIntUniform("uNumPointLights", num);
    
    //---------------------------------------------------------
    // フォグ設定
    // ・フォグ距離（min/max）と色
    //---------------------------------------------------------
    shader->SetFloatUniform ("uFoginfo.maxDist", mFog.MaxDist);
    shader->SetFloatUniform ("uFoginfo.minDist", mFog.MinDist);
    shader->SetVectorUniform("uFoginfo.color",   mFog.Color);
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
