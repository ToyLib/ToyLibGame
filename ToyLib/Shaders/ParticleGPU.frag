#version 410 core

//============================================================
// ParticleGPU.frag
//  - Render pass (instanced billboard quads)
//  - life >= uLifeMax の粒子は一旦消す（discard）
//============================================================

// from vertex shader
in vec2  vUV;
in float vLife;

// output
out vec4 outColor;

// uniforms
uniform sampler2D uTexture;
uniform float     uLifeMax;

void main()
{
    // life が寿命に到達したら描画しない（＝一旦消える）
    if (vLife >= uLifeMax)
    {
        discard;
    }

    vec4 tex = texture(uTexture, vUV);

    // 透明ピクセルは捨てる（オーバードロー軽減）
    // ※必要なら閾値調整
    if (tex.a <= 0.001)
    {
        discard;
    }

    outColor = tex;
}
