#pragma once

#include "Graphics/VisualComponent.h"
#include <memory>

namespace toy {

//======================================================================
// BillboardComponent
//
// ・3D空間に配置される看板型スプライト
// ・常にカメラの方向を向く（カメラ正面に正対する）描画を行う
// ・木、エフェクト、看板、キャラの簡易LOD などに使用
//
// VisualComponent を継承しているため、Renderer が管理する通常の
// Drawパイプラインで描画される。
//======================================================================
class BillboardComponent : public VisualComponent
{
public:
    // drawOrder で描画順を指定（通常 Object3D レイヤー）
    BillboardComponent(class Actor* a, int drawOrder);
    ~BillboardComponent();
    
    // Billboard の描画処理
    // カメラ方向に回転した板ポリを描画する
    void Draw() override;
    
    // サイズ変更
    void SetScale(float scale) { mScale = scale; }
    float GetScale() const { return mScale; }
    
private:
    // スプライトのスケール（Texture のサイズに掛ける倍率）
    float mScale;
};

} // namespace toy
