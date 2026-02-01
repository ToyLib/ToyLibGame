#version 410 core

in vec2 vUV;
out vec4 outColor;

uniform sampler2D uSurfaceTex;

uniform bool  uFlipX;
uniform bool  uFlipY;

uniform float uOpacity;  // 0..1
uniform vec3  uTint;     // 乗算色

// 0: Monitor  1: Mirror  2: Water
uniform int   uMode;

uniform float uTime;

uniform float uDistortStrength;   // 0.0..0.05
uniform float uScanlineStrength;  // 0..1

uniform float uFresnel;
uniform float uFresnelPow;

uniform float uWaveSpeed;

uniform float uSwayStrength;      // 0..0.02
uniform float uSparkleStrength;

vec2 ApplyFlip(vec2 uv)
{
    if (uFlipX) uv.x = 1.0 - uv.x;
    if (uFlipY) uv.y = 1.0 - uv.y;
    return uv;
}

// wave: 0..1
float softHighlight(vec2 uv, float t, float wave)
{
    float crest = smoothstep(0.55, 0.95, wave);

    float band = sin(uv.x * 6.0 + t * 0.6) * 0.5 + 0.5;
    band *= sin(uv.y * 5.0 - t * 0.5) * 0.5 + 0.5;

    band = smoothstep(0.45, 0.85, band);

    float fw = fwidth(band);
    band = smoothstep(0.45 - fw, 0.85 + fw, band);

    return crest * band;
}

// 小さなヘルパー：サンプルを安全に（境界付近のチラつき抑制）
vec4 SampleSurface(vec2 uv)
{
    // 端の黒/クリア混入が気になるなら clamp を強める
    uv = clamp(uv, vec2(0.001), vec2(0.999));
    return texture(uSurfaceTex, uv);
}

void main()
{
    vec2 uv = ApplyFlip(vUV);

    // ============================================================
    // 0) Monitor
    // ============================================================
    if (uMode == 0)
    {
        vec4 c = SampleSurface(uv);

        float sl = sin((uv.y + uTime * 0.4) * 800.0) * 0.5 + 0.5;
        float k  = mix(1.0 - uScanlineStrength, 1.0, sl);
        c.rgb *= k;

        c.rgb *= uTint;
        c.a   *= uOpacity;
        outColor = c;
        return;
    }

    // ============================================================
    // 1) Mirror
    // ============================================================
    if (uMode == 1)
    {
        float n = sin(uv.x * 30.0 + uTime * 1.2) * sin(uv.y * 25.0 - uTime * 1.0);
        vec2 duv = uv + vec2(n, -n) * (uDistortStrength * 0.3);

        vec4 c = SampleSurface(duv);

        float f = clamp(uFresnel, 0.0, 1.0);
        float edge = pow(f, max(uFresnelPow, 0.0001));

        c.rgb = mix(c.rgb * 0.85, c.rgb * 1.15, edge);

        c.rgb *= uTint;
        c.a   *= uOpacity;
        outColor = c;
        return;
    }


    // ============================================================
    // 2) Water（うねり＋さざ波＋軽いキラ）
    // ============================================================
    // ============================================================
    // 2) Water（空反射・ぼやけ重視）
    // ============================================================
    if (uMode == 2)
    {
        float t = uTime * 0.5;

        // --------------------------------------------
        // (A) 大きなうねり（0..1）
        // --------------------------------------------
        float swell =
            sin(vUV.x * 1.5 + t * 0.4) +
            cos(vUV.y * 1.2 - t * 0.3);
        swell = swell * 0.5 + 0.5;

        // --------------------------------------------
        // (B) かなり大きく流す（極端）
        // --------------------------------------------
        vec2 uv = ApplyFlip(vUV);

        uv += vec2(
            sin(t * 0.3 + uv.y * 2.0),
            cos(t * 0.25 + uv.x * 1.8)
        ) * 0.412;

        // --------------------------------------------
        // (C) 歪み（極端）
        // --------------------------------------------
        vec2 blurDistort = vec2(
            swell - 0.5,
            (1.0 - swell) - 0.5
        );
        uv += blurDistort * 0.115;

        // --------------------------------------------
        // (C') 破綻防止：UVをループさせる（マンガ向き）
        // --------------------------------------------
        uv = fract(uv);

        // --------------------------------------------
        // (D) 擬似ぼかし（極端だけど制御）
        //     ※オフセットを一定ではなく、少しだけ波で変える
        // --------------------------------------------
        vec2 ofs = vec2(0.206, 0.206) * (0.7 + 0.6 * swell);

        vec4 c = texture(uSurfaceTex, uv);
        c += texture(uSurfaceTex, fract(uv + ofs));
        c += texture(uSurfaceTex, fract(uv - ofs));
        c += texture(uSurfaceTex, fract(uv + vec2(-ofs.x, ofs.y)));
        c += texture(uSurfaceTex, fract(uv + vec2(ofs.x, -ofs.y)));
        c *= 0.2;

        // --------------------------------------------
        // (E) 呼吸（控えめ）
        // --------------------------------------------
        float lightWave = smoothstep(0.25, 0.85, swell);
        c.rgb *= mix(0.95, 1.05, lightWave);

        // --------------------------------------------
        // (F) 水色に溶かす
        // --------------------------------------------
        vec3 waterTint = vec3(0.85, 0.95, 1.05);
        c.rgb = mix(c.rgb, c.rgb * waterTint, 0.35);

        // --------------------------------------------
        // (G) 既存反映
        // --------------------------------------------
        c.rgb *= uTint;
        c.a   *= uOpacity;

        outColor = c;
        return;
    }

    // fallback
    vec4 c = SampleSurface(uv);
    c.rgb *= uTint;
    c.a   *= uOpacity;
    outColor = c;
}
