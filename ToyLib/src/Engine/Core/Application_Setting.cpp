#include "Engine/Core/Application.h"
#include "Utils/JsonHelper.h"
#include "Render/RenderBackendState.h"
#include <fstream>
#include <iostream>

namespace toy {

//=============================================================
// Application::LoadSettings
//   - ウィンドウタイトル
//   - デフォルトのウィンドウサイズ
//=============================================================
bool Application::LoadSettings(const std::string& filePath)
{
    //---------------------------------------------------------
    // ファイルオープン
    //---------------------------------------------------------
    std::ifstream file(filePath);
    if (!file.is_open())
    {
        std::cerr << "Failed to open Application settings file: "
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
    // タイトル
    //   "title": "ToyLib App"
    //---------------------------------------------------------
    JsonHelper::GetString(data, "title", mApplicationTitle);
    
    //---------------------------------------------------------
    // アセットパス
    //   "title": "ToyLib App"
    //---------------------------------------------------------
    JsonHelper::GetString(data, "asset_path", mSystemAssetPath);
    
    //---------------------------------------------------------
    // アセットパス
    //   "renderer_backend": "GL" "VK"
    //---------------------------------------------------------
    if (RenderBackendState::Get().Type() == RenderBackendType::Unknown)
    {
        std::string backend;
        JsonHelper::GetString(data, "renderer_backend", backend);
        if (backend == "VK" || backend == "Vulkan" || backend == "VULKAN")
        {
            RenderBackendState::Get().Set(RenderBackendType::Vulkan);
        }
        else
        {
            RenderBackendState::Get().Set(RenderBackendType::OpenGL);
        }
    }

    //---------------------------------------------------------
    // ウィンドウサイズ、
    //   "screen": {
    //       "screen_width":    1280
    //       "screen_height":  768
    //  }
    //---------------------------------------------------------
    if (data.contains("screen"))
    {
        JsonHelper::GetInt(data["screen"], "screen_width",  mScreenWidth);
        JsonHelper::GetInt(data["screen"], "screen_height", mScreenHeight);
        JsonHelper::GetBool(data["screen"], "fullscreen", mIsFullScreen);
        JsonHelper::GetBool(data["screen"], "fullscreen_use_setting_resolution",
                             mFullscreenUseSettingResolution);

        // フルスクリーン時に狙う解像度も同じ設定値を使う
        mFullscreenWidth  = mScreenWidth;
        mFullscreenHeight = mScreenHeight;
    }
    
    //---------------------------------------------------------
    // ターゲットFPS
    //   "target_fps": 60 (0=Unlimited)
    //---------------------------------------------------------
    JsonHelper::GetInt(data, "target_fps", mTargetFPS);
    
    //---------------------------------------------------------
    // デバッグ
    //   "debug": true/false
    //---------------------------------------------------------
    JsonHelper::GetBool(data["debug"], "enabled", mEnableDebug);

    
    std::cerr << "Loaded Application settings from "
              << filePath.c_str() << std::endl;
    return true;
}

} // namespace toy
