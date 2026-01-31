#pragma once

#include <memory>

namespace toy {

class VertexArray;

namespace SkyDomeMeshGenerator {

std::unique_ptr<VertexArray> CreateSkyDomeVAO(
    int slices = 32,
    int stacks = 16,
    float radius = 1.0f
);

} // namespace SkyDomeMeshGenerator
} // namespace toy
