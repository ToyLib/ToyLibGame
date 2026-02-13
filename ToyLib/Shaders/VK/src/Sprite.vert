#version 450

layout(location = 0) in vec2 inPos;
layout(location = 1) in vec2 inUV;

layout(push_constant) uniform Push
{
    mat4 uMVP;
};

layout(location = 0) out vec2 vUV;

void main()
{
    vUV = inUV;
    gl_Position = uMVP * vec4(inPos, 0.0, 1.0);
}
