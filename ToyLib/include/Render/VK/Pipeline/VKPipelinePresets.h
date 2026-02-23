// Render/VK/Pipeline/VKPipelinePresets.h
#pragma once

#include "Render/VK/Pipeline/VKPipeline.h" // VKPipelineDesc
#include <string>

namespace toy
{

namespace VKPipelinePresets
{
    // base: ".../Shaders/VK/spv/"
    VKPipelineDesc MakeSprite(const std::string& base);
    VKPipelineDesc MakeMesh(const std::string& base);
    VKPipelineDesc MakeSkinnedMesh(const std::string& base);
}

} // namespace toy//
