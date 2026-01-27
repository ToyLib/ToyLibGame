#pragma once

#include "Engine/Core/Component.h"
#include "Utils/MathUtil.h"

namespace toy {

class LightingManager;

//======================================================================
// PointLightComponent
//----------------------------------------------------------------------
// Actor に「点光源（Point Light）」を付与するコンポーネント。
//
// ・CreateComponent<PointLightComponent>() で Actor に追加する
// ・生成時に LightingManager へ自動登録
// ・破棄時に LightingManager から自動解除
// ・ライト位置は Actor のワールド座標に追従する
//
// 想定用途：
// ・松明 / ランプ / 魔法エフェクト / 室内照明 など
//
// 注意：
// ・LightingManager は所有しない（生ポインタ）
// ・LightingManager → Component の寿命順が保証されている前提
//======================================================================
class PointLightComponent : public Component
{
public:
    //==============================================================
    // コンストラクタ / デストラクタ
    //
    // updateOrder:
    // ・通常は描画より前で問題ないため 100 をデフォルトとする
    //==============================================================
    PointLightComponent(class Actor* owner, int updateOrder = 100);
    ~PointLightComponent() override;

    //==============================================================
    // Component interface
    //==============================================================
    void Update(float deltaTime) override;

    // ライトのワールド座標を取得
    // （Actor のワールド行列の平行移動成分）
    Vector3 GetPosition() const override;

    //==============================================================
    // プロパティ設定
    //==============================================================

    // ライトカラー（RGB、HDR 想定）
    void SetColor(const Vector3& c)      { mColor = c; }

    // 明るさスケール（シェーダ側で color に乗算される想定）
    void SetIntensity(float i)           { mIntensity = i; }

    // 距離減衰パラメータ（OpenGL で一般的な形式）
    // attenuation = 1.0 / (constant + linear*d + quadratic*d*d)
    void SetConstant(float c)            { mConstant = c; }
    void SetLinear(float l)              { mLinear = l; }
    void SetQuadratic(float q)           { mQuadratic = q; }

    // 影響半径（最適化・見た目用のカットオフ距離）
    void SetRadius(float r)              { mRadius = r; }

    // 有効 / 無効切り替え（登録は維持したまま ON/OFF）
    void SetEnabled(bool e)              { mIsEnabled = e; }

    //==============================================================
    // プロパティ取得
    //==============================================================
    const Vector3& GetColor()     const  { return mColor; }
    float          GetIntensity() const  { return mIntensity; }
    float          GetConstant()  const  { return mConstant; }
    float          GetLinear()    const  { return mLinear; }
    float          GetQuadratic() const  { return mQuadratic; }
    float          GetRadius()    const  { return mRadius; }
    bool           IsEnabled()    const  { return mIsEnabled; }

private:
    //==============================================================
    // 内部状態
    //==============================================================

    // LightingManager への参照（所有しない）
    // コンストラクタで登録、デストラクタで解除する
    LightingManager* mLighting { nullptr };

    // ライトパラメータ
    Vector3 mColor     { 1.0f, 1.0f, 1.0f };        // ライト色
    float   mIntensity { 3.0f };    // 明るさスケール（シェーダ側で乗算される想定）

    // 距離減衰係数
    float   mConstant  { 1.0f };     // 定数項
    float   mLinear    { 0.09f };    // 一次項
    float   mQuadratic { 0.032f };   // 二次項

    float   mRadius    { 28.0f };    // 影響半径
    bool    mIsEnabled { true };     // 有効フラグ
};

} // namespace toy
