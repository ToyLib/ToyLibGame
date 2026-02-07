// Graphics/Effect/WireframeComponent.h
#pragma once

#include "Graphics/VisualComponent.h"
#include "Render/RenderQueue.h"

#include <memory>

namespace toy {

//------------------------------------------------------------
// WireframeComponent
//   ・デバッグ用ワイヤーフレーム描画
//   ・RenderQueue 経由で GL_LINES 描画
//------------------------------------------------------------
class WireframeComponent : public VisualComponent
{
public:
    WireframeComponent(class Actor* owner,
                       int drawOrder,
                       VisualLayer layer = VisualLayer::Object3D);

    //--------------------------------------------------------
    // RenderQueue 用
    //--------------------------------------------------------
    void GatherRenderItems(RenderQueue& out) override;


    //--------------------------------------------------------
    // 設定
    //--------------------------------------------------------
    void SetVertexArray(std::shared_ptr<class VertexArray> vertex)
    {
        mVertexArray = std::move(vertex);
    }

    void SetColor(const Vector3& color) { mColor = color; }
    void SetEnableLight(bool b) { mEnableLight = b; }

private:
    std::shared_ptr<class VertexArray> mVertexArray;
    Vector3 mColor { 1.0f, 1.0f, 1.0f };
    bool    mEnableLight { false };
};

} // namespace toy
