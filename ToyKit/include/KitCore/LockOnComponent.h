#pragma once
#include "Engine/Core/Component.h"

namespace toy::kit {



enum class LockOnViewMode
{
    CameraBased,   // カメラの向き・位置で視界判定
    PlayerBased,   // プレイヤー（Owner）の向き・位置で視界判定
    Hybrid,        // origin=カメラ, forward=プレイヤー みたいな混ぜ方も可能
};

class LockOnComponent : public toy::Component
{
    
    
};

} // namespace toy::kit
