#version 450

layout(location=0) in vec2 vUV;
layout(location=0) out vec4 outColor;

layout(set=1, binding=0) uniform sampler2D uBaseMap;

layout(push_constant) uniform PushFrag
{
    vec4 colorAlpha;
} pc;

void main()
{
    vec4 tex = texture(uBaseMap, vUV);
    outColor = vec4(tex.rgb * pc.colorAlpha.rgb,
                    tex.a  * pc.colorAlpha.a);
}
