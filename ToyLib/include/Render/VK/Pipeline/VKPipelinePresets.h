//======================================================================
// Render/VK/Pipeline/VKPipelinePresets.h
//======================================================================
#pragma once

#include "Render/VK/Pipeline/VKPipeline.h" // VKPipelineDesc
#include <string>

namespace toy
{

namespace VKPipelinePresets
{

    // base: ".../Shaders/VK/spv/"

    // 2D / UI sprite
    VKPipelineDesc MakeSprite(const std::string& base);

    // Unlit textured quad (Billboard / FootSprite / etc)
    VKPipelineDesc MakeUnlitQuad(const std::string& base);

    // ★追加：Wireframe / Debug lines
    VKPipelineDesc MakeUnlitWire(const std::string& base);

    // Standard mesh
    VKPipelineDesc MakeMesh(const std::string& base);

    // Skinned mesh
    VKPipelineDesc MakeSkinnedMesh(const std::string& base);

    // Shadow(depth-only)
    VKPipelineDesc MakeShadowMesh(const std::string& base);
    VKPipelineDesc MakeShadowSkinnedMesh(const std::string& base);

}

} // namespace toy
