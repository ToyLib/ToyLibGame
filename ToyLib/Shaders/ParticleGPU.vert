// shaders/ParticleGPU.vert
#version 410 core

layout(location = 0) in vec3 aQuadPos;
layout(location = 1) in vec2 aUV;

// instanced particle data
layout(location = 3) in vec3 iPos;
layout(location = 4) in float iLife;

uniform mat4 uViewProj;

// billboard basis (world)
uniform vec3 uCameraRight;
uniform vec3 uCameraUp;

uniform float uSize;
uniform float uLifeMax;

out vec2 vUV;
out float vAlpha;
out float vLife;

void main()
{
    vUV = aUV;
    vLife = iLife;

    // dead -> alpha 0 and move off-screen
    if (iLife >= uLifeMax)
    {
        vAlpha = 0.0;
        gl_Position = vec4(0.0, 0.0, 2.0, 1.0);
        return;
    }

    // fade out near end
    float t = clamp(iLife / uLifeMax, 0.0, 1.0);
    // smooth fade out (t->1)
    vAlpha = 1.0 - smoothstep(0.65, 1.0, t);

    // billboard quad in world
    vec3 worldPos = iPos
                  + uCameraRight * (aQuadPos.x * uSize)
                  + uCameraUp    * (aQuadPos.y * uSize);

    // ToyLib convention: vec4(worldPos,1) * uViewProj
    gl_Position = vec4(worldPos, 1.0) * uViewProj;
}
