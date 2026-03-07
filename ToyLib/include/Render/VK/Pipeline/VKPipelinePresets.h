#pragma once

#include "Render/VK/Pipeline/VKPipeline.h"
#include <string>

namespace toy
{

namespace VKPipelinePresets
{
    VKPipelineDesc MakeSprite(const std::string& base);
    VKPipelineDesc MakeUnlitQuad(const std::string& base);
    VKPipelineDesc MakeUnlitWire(const std::string& base);

    VKPipelineDesc MakeSkyDome(const std::string& base);

    VKPipelineDesc MakeMesh(const std::string& base);
    VKPipelineDesc MakeSkinnedMesh(const std::string& base);

    VKPipelineDesc MakeShadowMesh(const std::string& base);
    VKPipelineDesc MakeShadowSkinnedMesh(const std::string& base);

    VKPipelineDesc MakeWeatherOverlay(const std::string& base);
    VKPipelineDesc MakeWeatherOverlayAdd(const std::string& base);

    VKPipelineDesc MakeFade(const std::string& base);

    VKPipelineDesc MakeRenderSurface(const std::string& base);
}

} // namespace toy
