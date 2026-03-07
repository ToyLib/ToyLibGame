/*
#version 450

// Fullscreen quad
layout(location = 0) in vec2 aPos;

layout(location = 0) out vec2 vTex;

void main()
{
    gl_Position = vec4(aPos, 0.0, 1.0);

    // clip [-1..1] -> uv [0..1]
    vTex = aPos * 0.5 + 0.5;
}
*/
#version 450

layout(location = 0) in vec2 aPos;
layout(location = 0) out vec2 vTex;

void main()
{
    gl_Position = vec4(aPos, 0.0, 1.0);
    vTex = aPos * 0.5 + 0.5;
}
