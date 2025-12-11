#pragma once
#include "Engine/Core/Component.h"
#include "Utils/MathUtil.h"

namespace toy {

class LightingManager;

//-------------------------------------------------------------
// PointLightComponent
// ・Actor にぶら下げるポイントライト
// ・CreateComponent<PointLightComponent>() で生成
//-------------------------------------------------------------
class PointLightComponent : public Component
{
public:
    PointLightComponent(class Actor* owner, int updateOrder = 100);
    ~PointLightComponent() override;

    void Update(float deltaTime) override;

    // ワールド座標の取得（Actor のワールド行列から）
    Vector3 GetPosition() const override;

    // ---- プロパティ設定 ----
    void SetColor(const Vector3& c)      { mColor = c; }
    void SetIntensity(float i)           { mIntensity = i; }
    void SetConstant(float c)            { mConstant = c; }
    void SetLinear(float l)              { mLinear = l; }
    void SetQuadratic(float q)           { mQuadratic = q; }
    void SetRadius(float r)              { mRadius = r; }
    void SetEnabled(bool e)              { mIsEnabled = e; }

    const Vector3& GetColor()     const  { return mColor; }
    float          GetIntensity() const  { return mIntensity; }
    float          GetConstant()  const  { return mConstant; }
    float          GetLinear()    const  { return mLinear; }
    float          GetQuadratic() const  { return mQuadratic; }
    float          GetRadius()    const  { return mRadius; }
    bool           IsEnabled()    const  { return mIsEnabled; }

private:
    LightingManager* mLighting;   // LightingManager への生ポインタ（所有しない）

    Vector3 mColor;
    float   mIntensity;

    float   mConstant;
    float   mLinear;
    float   mQuadratic;

    float   mRadius;
    bool    mIsEnabled;
};

} // namespace toy
