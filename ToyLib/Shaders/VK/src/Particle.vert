#version 450

layout(set = 0, binding = 0, std140, row_major) uniform SceneUBO
{
    mat4 viewProj;
    vec4 cameraPos;
    vec4 ambientLight;
    vec4 dirDir;
    vec4 dirDiffuse;
    vec4 dirSpecular;
} uScene;

layout(push_constant, row_major) uniform ParticlePC
{
    vec4 cameraRight;
    vec4 cameraUp;
    vec4 params; // x=size, y=lifeMax
} pc;

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal; // unused
layout(location = 2) in vec2 aUV;

layout(location = 3) in vec3 iPos;
layout(location = 4) in float iLife;

layout(location = 0) out vec2 vUV;
layout(location = 1) out float vAlpha;

void main()
{
    float size    = pc.params.x;
    float lifeMax = pc.params.y;

    vec3 camRight = normalize(pc.cameraRight.xyz);
    vec3 camUp    = normalize(pc.cameraUp.xyz);

    vec3 posWS =
        iPos +
        camRight * (aPos.x * size) +
        camUp    * (aPos.y * size);

    vec4 worldPos = vec4(posWS, 1.0);
    gl_Position = worldPos * uScene.viewProj;

    vUV = aUV;

    float alpha = 1.0;

    // dead 粒は描かない
    if (iLife >= lifeMax)
    {
        alpha = 0.0;
    }
    else if (lifeMax > 0.0001)
    {
        float t = clamp(iLife / lifeMax, 0.0, 1.0);
        alpha = 1.0 - t;
    }

    vAlpha = alpha;
}
