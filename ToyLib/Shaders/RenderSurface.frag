#version 410 core

//==============================================================
// RenderSurface.frag
//  - テクスチャをそのまま映す（鏡/モニタ両対応）
//  - FlipX/FlipY は UV を反転して実現
//  - Opacity/Tint で演出に対応
//==============================================================

in vec2 vUV;
out vec4 outColor;

uniform sampler2D uSurfaceTex;

uniform bool  uFlipX;
uniform bool  uFlipY;

uniform float uOpacity;  // 0..1
uniform vec3  uTint;     // 乗算色

void main()
{
    vec2 uv = vUV;

    if (uFlipX) uv.x = 1.0 - uv.x;
    if (uFlipY) uv.y = 1.0 - uv.y;

    vec4 c = texture(uSurfaceTex, uv);

    c.rgb *= uTint;
    c.a   *= uOpacity;

    // 必要なら完全透明を捨てて最適化
    // if (c.a <= 0.001) discard;

    outColor = c;
}
