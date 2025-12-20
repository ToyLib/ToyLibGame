#pragma once
#include "Graphics/VisualComponent.h"
#include "Utils/MathUtil.h" // Vector3, Matrix4 など

#include <memory>

namespace toy {

class Texture;

class RenderSurfaceComponent : public VisualComponent
{
public:
    // 3D 置物の板ポリとして描画するので基本は Object3D レイヤー
    RenderSurfaceComponent(class Actor* owner, int drawOrder = 100);

    void Draw() override;

    // ---- 演出パラメータ ----
    void SetFlip(bool flipX, bool flipY) { mFlipX = flipX; mFlipY = flipY; }
    void SetOpacity(float a) { mOpacity = a; }
    void SetTint(const Vector3& tint) { mTint = tint; }

    bool GetFlipX() const { return mFlipX; }
    bool GetFlipY() const { return mFlipY; }
    float GetOpacity() const { return mOpacity; }
    Vector3 GetTint() const { return mTint; }
    
private:

    bool    mFlipX = true;
    bool    mFlipY = true;

    float   mOpacity = 1.0f;
    Vector3 mTint = Vector3(1.0f, 1.0f, 1.0f);
};

} // namespace toy
