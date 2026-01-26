#pragma once
#include "Utils/MathUtil.h"

namespace toy::kit {

struct MoveIntent
{
    Vector3 moveDir   = Vector3::Zero; // 正規化済み（XZ想定）
    float   moveSpeed = 0.0f;

    bool    jump      = false;
    bool    dash      = false;
};

} // namespace toy::kit
