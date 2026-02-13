#version 450

layout(set = 0, binding = 0) uniform sampler2D uTex;

layout(location = 0) in vec2 vUV;
layout(location = 1) in vec4 vColor;

layout(location = 0) out vec4 outColor;

void main()
{
    vec4 tex = texture(uTex, vUV);
    outColor = vec4(tex.rgb * vColor.rgb, tex.a * vColor.a);

    // デバッグ：常に白ならこれ
    // outColor = vec4(1.0);
}
