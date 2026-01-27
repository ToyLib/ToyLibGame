#pragma once

#include "Graphics/VisualComponent.h"
#include <memory>

namespace toy {

//------------------------------------------------------------
// WireframeComponent
//   ・メッシュのワイヤーフレームだけを描画するデバッグ用コンポーネント
//   ・主に当たり判定の可視化、メッシュ形状確認に使用
//   ・VertexArray が外部で作成されたものを流用して描画する
//------------------------------------------------------------
class WireframeComponent : public VisualComponent
{
public:
    // owner     : 所属する Actor
    // drawOrder : 描画順（デフォルトは上位に描画したい時に利用）
    // layer     : Object3D（3Dオブジェクトとして扱う）
    WireframeComponent(class Actor* owner,
                       int drawOrder,
                       VisualLayer layer = VisualLayer::Object3D);
    
    //--------------------------------------------------------
    // ワイヤーフレームを描画
    //   ・使用するシェーダは VisualComponent 側で指定されているもの
    //   ・mVertexArray のインデックスを GL_LINES で描画する
    //--------------------------------------------------------
    void Draw() override;
    
    //--------------------------------------------------------
    // ワイヤーフレーム表示に使う頂点配列を設定
    //--------------------------------------------------------
    void SetVertexArray(std::shared_ptr<class VertexArray> vertex) { mVertexArray = vertex; }
    
    //--------------------------------------------------------
    // 線の色を指定
    //--------------------------------------------------------
    void SetColor(const Vector3& color) { mColor = color; }
    
    void SetEnableLight(const bool b) { mEnableLight = b; };
    
private:
    Vector3 mColor    { 1.0f, 1.0f, 1.0f };   // ワイヤーフレームの線の色
    bool mEnableLight { false };
};

} // namespace toy
