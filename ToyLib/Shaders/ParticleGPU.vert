#version 410 core

//============================================================
// ParticleGPU.vert
//  - Instanced billboard quad rendering
//  - Per-vertex : aQuadPos, aUV
//  - Per-instance: iPos, iLife
//============================================================

// quad geometry
layout(location = 0) in vec3 aQuadPos;  // -0.5..0.5 quad local
layout(location = 1) in vec2 aUV;

// instance data (from particle buffer)
layout(location = 3) in vec3  iPos;     // particle position (world)
layout(location = 4) in float iLife;    // particle life (sec)

// varyings to fragment
out vec2  vUV;
out float vLife;

// uniforms
uniform mat4  uViewProj;
uniform vec3  uCameraRight;  // normalized
uniform vec3  uCameraUp;     // normalized
uniform float uSize;         // particle size (world)

void main()
{
    vUV   = aUV;
    vLife = iLife;

    // billboard in world space
    vec3 worldPos =
        iPos +
        (uCameraRight * aQuadPos.x + uCameraUp * aQuadPos.y) * uSize;

    // ToyLib (DirectX-like) order: vec4 * mat4
    gl_Position = vec4(worldPos, 1.0) * uViewProj;
}
