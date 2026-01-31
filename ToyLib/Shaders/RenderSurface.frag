#version 410 core

// ------------------------------------------------------------
// Inputs / Outputs（現状に合わせる）
// ------------------------------------------------------------
in vec2 vUV;
out vec4 outColor;

// ------------------------------------------------------------
// Textures
// ------------------------------------------------------------
uniform sampler2D uSurfaceTex;

// ------------------------------------------------------------
// Existing params（現状そのまま）
// ------------------------------------------------------------
uniform bool  uFlipX;
uniform bool  uFlipY;

uniform float uOpacity;  // 0..1
uniform vec3  uTint;     // 乗算色

// ------------------------------------------------------------
// New params（追加）
// ------------------------------------------------------------
// 0: Monitor  1: Mirror  2: Water
uniform int   uMode;

// time（波/走査線で使用）
uniform float uTime;

// 共通：UV歪み強さ（0.0〜0.05くらいから）
uniform float uDistortStrength;

// Monitor：走査線強度（0..1）
uniform float uScanlineStrength;

// Mirror：フレネル（0..1）を C++ から渡せるなら使う（未設定なら 0 でOK）
uniform float uFresnel;
uniform float uFresnelPow;

// Water：波速度
uniform float uWaveSpeed;

uniform float uSwayStrength; // 0..0.02 くらい

uniform float uSparkleStrength;

// ------------------------------------------------------------

vec2 ApplyFlip(vec2 uv)
{
    if (uFlipX) uv.x = 1.0 - uv.x;
    if (uFlipY) uv.y = 1.0 - uv.y;
    return uv;
}

// wave: 0..1（あなたの w1*w2 でもOK）
float softHighlight(vec2 uv, float t, float wave)
{
    // 波の山だけを拾う（帯になる）
    float crest = smoothstep(0.55, 0.95, wave);

    // UV方向にゆっくり動く細い帯（“水面の筋”）
    float band = sin(uv.x * 6.0 + t * 0.6) * 0.5 + 0.5;
    band *= sin(uv.y * 5.0 - t * 0.5) * 0.5 + 0.5; // 0..1

    // 急峻すぎると粒状になるので滑らかに
    band = smoothstep(0.45, 0.85, band);

    // fwidthで解像度に応じて“ぼかし幅”を増やす（チラつき抑制）
    float fw = fwidth(band);
    band = smoothstep(0.45 - fw, 0.85 + fw, band);

    // crest と band を掛けて、波の山にだけ上品に乗せる
    return crest * band;
}
void main()
{
    
    vec2 uv = ApplyFlip(vUV);
    

    // ============================================================
    // 0) Monitor（そのまま + うっすら走査線）
    // ============================================================
    if (uMode == 0)
    {
        vec4 c = texture(uSurfaceTex, uv);

        // scanline：雑に y 方向へ縞（強さは uScanlineStrength）
        float sl = sin((uv.y + uTime * 0.4) * 800.0) * 0.5 + 0.5; // 0..1
        float k  = mix(1.0 - uScanlineStrength, 1.0, sl);
        c.rgb *= k;

        c.rgb *= uTint;
        c.a   *= uOpacity;
        outColor = c;
        return;
    }

    // ============================================================
    // 1) Mirror（微小歪み + フレネルで縁強調）
    // ============================================================
    if (uMode == 1)
    {
        // 微小な歪み（鏡面のゆらぎ）
        float n = sin(uv.x * 30.0 + uTime * 1.2) * sin(uv.y * 25.0 - uTime * 1.0);
        vec2 duv = uv + vec2(n, -n) * (uDistortStrength * 0.3);

        vec4 c = texture(uSurfaceTex, duv);

        // Fresnel：C++から渡してるならそれを使う（0..1）
        float f = clamp(uFresnel, 0.0, 1.0);
        float edge = pow(f, max(uFresnelPow, 0.0001));

        // 端が少し強く・中心は少し落とす、くらいの控えめ調整
        c.rgb = mix(c.rgb * 0.85, c.rgb * 1.15, edge);

        c.rgb *= uTint;
        c.a   *= uOpacity;
        outColor = c;
        return;
    }

    // ============================================================
    // 2) Water（しっかり波歪み + ちょいキラ）
    // ============================================================
    if (uMode == 2)
    {
        float t = uTime * uWaveSpeed;

        // 先に “ゆらゆら” で uv を動かす
        float swayX = sin(t * 0.7 + uv.y * 6.0) * (uSwayStrength * 0.6);
        float swayY = cos(t * 0.6 + uv.x * 5.0) * (uSwayStrength * 0.4);
        uv += vec2(swayX, swayY);

        // sway 後の uv で波を作る（模様も一緒に漂う）
        float w1 = sin(uv.x * 25.0 + t) * 0.5 + 0.5;
        float w2 = sin(uv.y * 18.0 - t * 1.2) * 0.5 + 0.5;
        float wave = w1 * w2; // 0..1

        vec2 duv = uv + vec2(wave - 0.5, (1.0 - wave) - 0.5) * uDistortStrength;

        vec4 c = texture(uSurfaceTex, duv);

        float h = softHighlight(uv, t, wave);
        float viewFactor = smoothstep(0.2, 0.8, abs(uv.y - 0.5) * 2.0);

        // またはもっとシンプルに
        // float viewFactor = pow(1.0 - abs(uv.y - 0.5) * 2.0, 2.0);

        h *= viewFactor;
        c.rgb += h * uSparkleStrength; // 0.05〜0.18くらい推奨（上品）
        //c.rgb *= (1.0 + h * uSparkleStrength);
        float sparkle = pow(wave, 8.0) * 0.25;
        c.rgb += sparkle;

        c.rgb *= uTint;
        c.a   *= uOpacity;
        outColor = c;
        return;
    }

    // fallback（現状と同じ）
    vec4 c = texture(uSurfaceTex, uv);
    c.rgb *= uTint;
    c.a   *= uOpacity;
    outColor = c;
}
