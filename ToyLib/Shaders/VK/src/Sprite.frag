#version 450

layout(set = 0, binding = 0) uniform sampler2D uTex;

layout(location = 0) in vec2 vUV;
layout(location = 0) out vec4 outColor;

layout(push_constant) uniform Push
{
    mat4 uWorld;
    mat4 uViewProj;
    vec4 uColorAlpha; // rgb + alpha
} pc;

void main()
{
    vec4 tex = texture(uTex, vUV);
    outColor = vec4(tex.rgb * pc.uColorAlpha.rgb, tex.a * pc.uColorAlpha.a);
}
