#include "Engine/Render/Renderer.h"
#include "Engine/Render/LightingManager.h"
#include "Utils/JsonHelper.h"
#include <fstream>
#include <iostream>

namespace toy {

//=============================================================
// Renderer::LoadSettings
//   - Renderer_Settings.json などから描画関連の初期設定を読み込む
//   - ウィンドウタイトル／解像度／FOV／クリアカラー／フォグ／シャドウ等
//=============================================================
bool Renderer::LoadSettings(const std::string& filePath)
{
    //---------------------------------------------------------
    // ファイルオープン
    //---------------------------------------------------------
    std::ifstream file(filePath);
    if (!file.is_open())
    {
        std::cerr << "Failed to open Renderer settings file: "
                  << filePath.c_str() << std::endl;
        return false;
    }
    
    //---------------------------------------------------------
    // JSON パース
    //---------------------------------------------------------
    nlohmann::json data;
    try
    {
        file >> data;
    }
    catch (const std::exception& e)
    {
        std::cerr << "JSON parse error: " << e.what() << std::endl;
        return false;
    }
    
    //---------------------------------------------------------
    // シェーダーパス
    //   "shader_path": "ToyLib/Shaders/"
    //---------------------------------------------------------
    JsonHelper::GetString(data, "shader_path", mShaderPath);
    
    //---------------------------------------------------------
    // 仮想解像度、
    //   "screen": {
    //       "virtual_with":    1280
    //       "virtual_height":  768
    //  }
    //---------------------------------------------------------
    if (data.contains("screen"))
    {
        JsonHelper::GetFloat(data["screen"], "virtual_with",    mVirtualWidth);
        JsonHelper::GetFloat(data["screen"], "virtual_height",  mVirtualHeight);
    }
    
    //---------------------------------------------------------
    // 視野角（Perspective FOV, 単位は度）
    //   "perspectiveFOV": 45.0
    //---------------------------------------------------------
    JsonHelper::GetFloat(data, "perspectiveFOV", mPerspectiveFOV);
    
    //---------------------------------------------------------
    // デバッグモード
    //   "debug": { "enabled": true }
    //---------------------------------------------------------
    if (data.contains("debug"))
    {
        JsonHelper::GetBool(data["debug"], "enabled", mIsDebugMode);
    }
    
    //---------------------------------------------------------
    // クリアカラー（背景色）
    //   "clearColor": [0.2, 0.5, 0.8]
    //---------------------------------------------------------
    JsonHelper::GetVector3(data, "clearColor", mClearColor);
    
    //---------------------------------------------------------
    // ワイヤーフレームカラー
    //   "wireColor": [1.0, 1.0, 1.0]
    //---------------------------------------------------------
    JsonHelper::GetVector3(data, "wireColor", mWireColor);
    
    //---------------------------------------------------------
    // フォグ設定
    //   "fog": {
    //       "maxDist": 100.0,
    //       "minDist": 1.0,
    //       "color": [0.5, 0.5, 0.5]
    //   }
    //---------------------------------------------------------
    if (data.contains("fog"))
    {
        FogInfo fog;
        JsonHelper::GetFloat   (data["fog"], "maxDist", fog.MaxDist);
        JsonHelper::GetFloat   (data["fog"], "minDist", fog.MinDist);
        JsonHelper::GetVector3 (data["fog"], "color",   fog.Color);
        
        mLightingManager->SetFogInfo(fog);
    }
    
    //---------------------------------------------------------
    // シャドウ（影）設定
    //   "shadow": {
    //       "near": 10.0,
    //       "far":  100.0,
    //       "ortho_width":  100.0,
    //       "ortho_height": 100.0,
    //       "resolution_width":  4096,
    //       "resolution_height": 4096
    //   }
    //---------------------------------------------------------
    if (data.contains("shadow"))
    {
        JsonHelper::GetFloat(data["shadow"], "near",            mShadowNear);
        JsonHelper::GetFloat(data["shadow"], "far",             mShadowFar);
        JsonHelper::GetFloat(data["shadow"], "ortho_width",     mShadowOrthoWidth);
        JsonHelper::GetFloat(data["shadow"], "ortho_height",    mShadowOrthoHeight);
        JsonHelper::GetInt  (data["shadow"], "resolution_width",  mShadowFBOWidth);
        JsonHelper::GetInt  (data["shadow"], "resolution_height", mShadowFBOHeight);
    }
    
    std::cerr << "Loaded Renderer settings from "
              << filePath.c_str() << std::endl;
    return true;
}

} // namespace toy
