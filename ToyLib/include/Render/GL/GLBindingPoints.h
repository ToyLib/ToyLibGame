#pragma once

//======================================================================
//  GLBindingPoints.h
//  ・GL UBO バインディングポイント定数の一元管理
//  ・GLShader::Load() や GLRenderer で使用
//======================================================================

#include "glad/glad.h"

namespace toy::gl
{
    constexpr GLuint kBonePaletteBinding = 0;   // BonePalette UBO (既存)
    constexpr GLuint kSceneUBOBinding    = 1;   // SceneUBO     (新規)
    constexpr GLuint kShadowUBOBinding   = 2;   // ShadowSceneUBO (新規、シャドウ深度パス専用)
}
