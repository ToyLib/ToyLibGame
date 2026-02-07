#pragma once

#include <map>


class PipelineManager
{
    std::unordered_map<std::string, std::shared_ptr<class Shader>> mShaders;
};
