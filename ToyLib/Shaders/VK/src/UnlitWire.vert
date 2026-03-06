#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;

layout(location = 0) out vec3 vWorldPos;
layout(location = 1) out vec3 vNormal;

layout(set = 0, binding = 0, std140) uniform SceneUBO
{
    mat4 viewProj;

    vec4 cameraPos;
    vec4 ambient;

    vec4 dirDir;
    vec4 dirDiffuse;
    vec4 dirSpecular;

    vec4 fogColor;
    vec4 fogParams;

    int  numPointLights;
    int  _plPad0;
    int  _plPad1;
    int  _plPad2;
} uScene;

layout(push_constant) uniform DebugPC
{
    mat4 world;
    vec4 color;   // rgb=color, a=alpha
    vec4 params;  // x=useLight
} pc;

void main()
{
    vec4 localPos = vec4(inPosition, 1.0);
    vec4 worldPos = pc.world * localPos;

    vWorldPos = worldPos.xyz;
    vNormal   = normalize(mat3(pc.world) * inNormal);

    gl_Position = uScene.viewProj * worldPos;
}
