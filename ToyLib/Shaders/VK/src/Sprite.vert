#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;   // unused
layout(location = 2) in vec2 inTexCoord;

layout(push_constant) uniform Push
{
    mat4 uMVP;
    vec4 uColor; // rgb + alpha
} pc;

layout(location = 0) out vec2 vUV;
layout(location = 1) out vec4 vColor;

void main()
{
    vUV = inTexCoord;
    vColor = pc.uColor;

    // GL Sprite.vert と同じ: pos * World * ViewProj という “row-vector” の流儀
    gl_Position = vec4(inPosition, 1.0) * pc.uMVP;

    // ★上下を直すならまずここを試す（確実に効く）
    // gl_Position.y = -gl_Position.y;
}
