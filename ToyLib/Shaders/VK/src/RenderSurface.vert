#version 450

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUV;

layout(location = 0) out vec2 vUV;

layout(set = 0, binding = 0) uniform SceneUBO
{
    mat4 uViewProj;
    vec4 uCameraPos;
    vec4 uAmbientLight;
} uScene;

layout(push_constant) uniform SurfacePC
{
    mat4 world;
    vec4 tintOpacity;
    vec4 params0;
    vec4 params1;
    vec4 params2;
} pc;

void main()
{
    vUV = aUV;
    gl_Position = uScene.uViewProj * pc.world * vec4(aPos, 1.0);
}
