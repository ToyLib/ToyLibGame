#version 450

layout(location = 0) in vec2 vUV;
layout(location = 1) in float vAlpha;

layout(location = 0) out vec4 outColor;

//============================================================
// Particle texture
// set=1, binding=0
//============================================================
layout(set = 1, binding = 0) uniform sampler2D uParticleTex;

void main()
{
    if (vAlpha <= 0.0)
    {
        discard;
    }

    vec4 tex = texture(uParticleTex, vUV);

    // 完全透明だけ落とす
    if (tex.a <= 0.001)
    {
        discard;
    }

    float alpha = tex.a * vAlpha;
    if (alpha <= 0.001)
    {
        discard;
    }

    outColor = vec4(tex.rgb, alpha);
}
