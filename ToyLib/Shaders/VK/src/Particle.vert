#version 450

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec2 aUV;

// instance data
layout(location = 3) in vec3 iPos;
layout(location = 4) in float iLife;

layout(location = 0) out vec2 vUV;
layout(location = 1) out float vAlpha;

//============================================================
// Scene UBO
// set=0, binding=0
//============================================================
layout(set = 0, binding = 0) uniform SceneUBO
{
    mat4 uViewProj;
} uScene;

//============================================================
// Push Constants
//============================================================
layout(push_constant) uniform ParticlePC
{
    vec4 uCameraRight; // xyz used
    vec4 uCameraUp;    // xyz used
    vec4 uParams;      // x=size, y=lifeMax, z/w=reserved
} pc;

void main()
{
    float size    = pc.uParams.x;
    float lifeMax = pc.uParams.y;

    vec3 camRight = pc.uCameraRight.xyz;
    vec3 camUp    = pc.uCameraUp.xyz;

    vec3 worldPos =
        iPos +
        camRight * (aPos.x * size) +
        camUp    * (aPos.y * size);

    gl_Position = uScene.uViewProj * vec4(worldPos, 1.0);

    vUV = aUV;

    float alpha = 1.0;
    if (lifeMax > 0.0001)
    {
        alpha = clamp(1.0 - (iLife / lifeMax), 0.0, 1.0);
    }
    vAlpha = alpha;
}
