#include "Engine/Render/LightingManager.h"
#include "Engine/Render/Shader.h"

namespace toy {

//-------------------------------------------------------------
// コンストラクタ
// ・太陽光の強さはデフォルト 1.0
//-------------------------------------------------------------
LightingManager::LightingManager()
: mSunIntensity(1.f)
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
    //shader->SetFloatUniform("uSunIntensity", mSunIntensity);
    
    //---------------------------------------------------------
    // ディレクショナルライト
    // ・mDirection は常に Normalize(Target - Position)
    //---------------------------------------------------------
    shader->SetVectorUniform("uDirLight.mDirection",     mDirLight.GetDirection());
    shader->SetVectorUniform("uDirLight.mDiffuseColor",  mDirLight.DiffuseColor);
    shader->SetVectorUniform("uDirLight.mSpecColor",     mDirLight.SpecColor);
    
    //---------------------------------------------------------
    // フォグ設定
    // ・フォグ距離（min/max）と色
    //---------------------------------------------------------
    shader->SetFloatUniform ("uFoginfo.maxDist", mFog.MaxDist);
    shader->SetFloatUniform ("uFoginfo.minDist", mFog.MinDist);
    shader->SetVectorUniform("uFoginfo.color",   mFog.Color);
}

} // namespace toy
