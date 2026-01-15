#pragma once
#include "Graphics/Sprite/FootSpriteComponent.h"

namespace toy {

class ShadowSpriteComponent : public FootSpriteComponent
{
public:
    ShadowSpriteComponent(class Actor* owner, int drawOrder = 10);
    ~ShadowSpriteComponent() override = default;

    // 影の“潰れ”や“伸び”の調整
    void SetStretch(float s) { mStretch = s; }          // 例：3.0f
    void SetAutoRotateByLight(bool on) { mAutoRotateByLight = on; }

protected:
    Matrix4 BuildWorldMatrix() const override;

private:
    float mStretch = 3.0f;
    bool  mAutoRotateByLight = true;
};

} // namespace toy
