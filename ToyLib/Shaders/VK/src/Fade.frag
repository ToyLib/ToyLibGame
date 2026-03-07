#version 450

layout(location = 0) in vec2 vUV;
layout(location = 0) out vec4 outColor;

layout(push_constant) uniform FadePC
{
    vec4 colorAlpha; // rgb = color, a = alpha
} pc;

void main()
{
    outColor = vec4(pc.colorAlpha.rgb, pc.colorAlpha.a);
}
