#version 450
layout(set = 1, binding = 0) uniform sampler2D uBaseMap;

layout(location=0) in vec2 vTex;
layout(location=1) in vec4 vColorAlpha;

layout(location=0) out vec4 outColor;

void main()
{
    vec4 tex = texture(uBaseMap, vTex);
    outColor = vec4(tex.rgb * vColorAlpha.rgb, tex.a * vColorAlpha.a);
}
