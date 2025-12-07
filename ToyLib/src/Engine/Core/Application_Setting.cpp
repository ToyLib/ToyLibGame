#include "Engine/Core/Application.h"
#include "Utils/JsonHelper.h"
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
    }
    
    std::cerr << "Loaded Application settings from "
    << filePath.c_str() << std::endl;
    return true;
}

} // namespace toy
