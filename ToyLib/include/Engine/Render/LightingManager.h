#pragma once
#include "Utils/MathUtil.h"
#include <memory>
#include <vector>

namespace toy {

//-------------------------------------------------------------
// DirectionalLight
// ・太陽光（平行光源）を表す構造体
// ・Position → 光の発生点（見かけの位置）
// ・Target   → 光が向かう方向
// ・GetDirection = normalize(Target - Position)
//-------------------------------------------------------------
struct DirectionalLight
{
    Vector3 Target       = Vector3::Zero;     // 光の向く先
    Vector3 Position     = Vector3::UnitZ;    // 光源の見かけの位置
    Vector3 DiffuseColor = Vector3(0.5f, 0.5f, 0.5f); // 拡散光
    Vector3 SpecColor    = Vector3(0.5f, 0.5f, 0.5f); // 反射光（スペキュラー）

    // 光の方向（正規化済み）
    Vector3 GetDirection() const { return Vector3::Normalize(Target - Position); }

    // Position / Target をまとめて設定
    void LookAt(const Vector3& position, const Vector3& target)
    {
        Position = position;
        Target   = target;
    }
};


//-------------------------------------------------------------
// FogInfo
// ・霧（フォグ）表現のパラメータ
// ・MinDist〜MaxDist で徐々に Color に近づく
//-------------------------------------------------------------
struct FogInfo
{
    float MaxDist = 100.0f;          // 完全に霧に包まれる距離
    float MinDist = 0.00001f;        // 霧の開始距離
    Vector3 Color = Vector3(0.5f, 0.5f, 0.5f); // 霧の色
};


//-------------------------------------------------------------
// LightingManager
// ・Directional Light（太陽）
// ・Ambient Light（環境光）
// ・Fog（霧）
//   これらを一元管理し、Shader に渡す役割を持つクラス
//-------------------------------------------------------------
class LightingManager
{
public:
    LightingManager() = default;

    //---------------------------------------------------------
    // 一括設定（構造体丸ごと）
    //---------------------------------------------------------
    
    void SetDirectionalLight(const DirectionalLight& light) { mDirLight = light; }
    const DirectionalLight& GetDirectionalLight() const { return mDirLight; }
    
    void SetFogInfo(const FogInfo& fog) { mFog = fog; }
    const FogInfo& GetFogInfo() const { return mFog; }
    
    
    //---------------------------------------------------------
    // 個別設定（Directional Light）
    //---------------------------------------------------------
    
    void SetLightPosition(const Vector3& pos) { mDirLight.Position = pos; }
    void SetLightTarget(const Vector3& target) { mDirLight.Target = target; }
    
    // Position & Target 同時設定
    void SetLightDirection(const Vector3& pos, const Vector3& target)
    {
        mDirLight.LookAt(pos, target);
    }
    
    void SetLightDiffuseColor(const Vector3& color) { mDirLight.DiffuseColor = color; }
    void SetLightSpecColor(const Vector3& color)    { mDirLight.SpecColor    = color; }
    
    const Vector3& GetLightPosition() const { return mDirLight.Position; }
    
    // 正規化方向
    Vector3 GetLightDirection() const { return mDirLight.GetDirection(); }
    
    
    //---------------------------------------------------------
    // 個別設定（Ambient Light）
    //---------------------------------------------------------
    
    void SetAmbientColor(const Vector3& color) { mAmbientColor = color; }
    const Vector3& GetAmbientColor() { return mAmbientColor; }
    
    
    //---------------------------------------------------------
    // 個別設定（Fog）
    //---------------------------------------------------------
    
    void SetFogMinDist(float min) { mFog.MinDist = min; }
    void SetFogMaxDist(float max) { mFog.MaxDist = max; }
    void SetFogColor(const Vector3& color) { mFog.Color = color; }
    
    float GetFogMinDist() const { return mFog.MinDist; }
    float GetFogMaxDist() const { return mFog.MaxDist; }
    const Vector3& GetFogColor() const { return mFog.Color; }
    
    
    //---------------------------------------------------------
    // 太陽光の強さ（シーンの明度調整などに使用）
    //---------------------------------------------------------
    
    void  SetSunIntensity(const float i) { mSunIntensity = i; }
    float GetSunIntensity() const { return mSunIntensity; }
    
    
    //---------------------------------------------------------
    // Shader へ適用
    // ・Directional Light / Ambient Light / Fog などを一括反映
    // ・viewMatrixから LightDir を view space に変換して渡す
    //---------------------------------------------------------
    
    void ApplyToShader(std::shared_ptr<class Shader> shader,
                       const Matrix4& viewMatrix);
    
    void ApplyToShader(Shader* shader, const Matrix4& viewMatrix);
    
    //---------------------------------------------------------
    // Point Light 管理
    //---------------------------------------------------------
    void RegisterPointLight(class PointLightComponent* light);
    void UnregisterPointLight(class PointLightComponent* light);
    const std::vector<class PointLightComponent*>& GetPointLights() const
    {
        return mPointLights;
    }
    
private:
    //---------------------------------------------------------
    // ライト / フォグ / アンビエント
    //---------------------------------------------------------
    
    DirectionalLight mDirLight      { Vector3::Zero };
    FogInfo          mFog           {};
    Vector3          mAmbientColor  { 0.8f, 0.8f, 0.8f };
    
    float mSunIntensity { 1.0f }; // 太陽の強さ（時間帯／天候などで変化）
    
    // ポイントライト
    std::vector<class PointLightComponent*> mPointLights;
};

} // namespace toy
