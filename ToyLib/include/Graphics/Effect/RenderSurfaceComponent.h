#pragma once

//==============================================================================
// RenderSurfaceComponent
//  - RenderTarget の結果などを「3Dの板ポリ（置物）」として表示する VisualComponent
//  - 基本は Object3D レイヤーで描画（鏡 / モニタ / スクリーン等の用途）
//==============================================================================

#include "Graphics/VisualComponent.h"
#include "Utils/MathUtil.h" // Vector3, Matrix4 など

#include <memory>

namespace toy {

enum class SurfaceMode
{
    Plain,
    Monitor,
    Mirror,
    Water
};

//==============================================================================
// RenderSurfaceComponent
//==============================================================================
class RenderSurfaceComponent : public VisualComponent
{
public:
    // 3D 置物の板ポリとして描画するので基本は Object3D レイヤー
    RenderSurfaceComponent(class Actor* owner, int drawOrder = 100);

    // VisualComponent
    void Draw() override;

    //==========================================================================
    // 演出パラメータ
    //==========================================================================
    void SetFlip(bool flipX, bool flipY) { mFlipX = flipX; mFlipY = flipY; }
    void SetOpacity(float a) { mOpacity = a; }
    void SetTint(const Vector3& tint) { mTint = tint; }
    
    void SetScale(const float scX, const float scY) { mScaleX = scX; mScaleY = scY; }

    bool    GetFlipX()   const { return mFlipX; }
    bool    GetFlipY()   const { return mFlipY; }
    float   GetOpacity() const { return mOpacity; }
    Vector3 GetTint()    const { return mTint; }
    
    void SetSurfaceMode(const SurfaceMode mode) { mMode = mode; }

private:
    //==========================================================================
    // パラメータ
    //==========================================================================
    bool    mFlipX;
    bool    mFlipY;
    
    float   mScaleX;
    float   mScaleY;

    float   mOpacity;
    Vector3 mTint;
    
    SurfaceMode mMode;
};

} // namespace toy
