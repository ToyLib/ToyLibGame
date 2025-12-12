// shaders/ParticleGPU.frag
#version 410 core

in vec2 vUV;
in float vAlpha;
in float vLife;

uniform sampler2D uTexture;

out vec4 outColor;

void main()
{
    // dead -> discard
    if (vAlpha <= 0.0)
    {
        discard;
    }

    vec4 tex = texture(uTexture, vUV);

    // if texture alpha is 0, discard early
    if (tex.a <= 0.001)
    {
        discard;
    }

    outColor = vec4(tex.rgb, tex.a * vAlpha);
}
