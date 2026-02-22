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

    // 次に増やす用（Step4以降で実装）
    // VKPipelineDesc MakeSkinned(const std::string& base);
    // VKPipelineDesc MakeShadow(const std::string& base);
}

} // namespace toy//
