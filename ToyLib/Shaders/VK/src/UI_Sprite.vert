#version 450

layout(location=0) in vec2 aPos;
layout(location=1) in vec2 aUV;
layout(push_constant) uniform PC { mat4 uMVP; vec4 uColor; } pc;
layout(location=0) out vec2 vUV;
layout(location=1) out vec4 vColor;

void main()
{
    vUV=aUV;
    vColor=pc.uColor;
    gl_Position = pc.uMVP * vec4(aPos,0,1);
}
