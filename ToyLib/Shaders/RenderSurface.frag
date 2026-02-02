#version 410 core

in vec2 vUV;
out vec4 outColor;

uniform sampler2D uSurfaceTex;

uniform bool  uFlipX;
uniform bool  uFlipY;

uniform float uOpacity;  // 0..1
uniform vec3  uTint;     // 乗算色

// 0: Plain  1: Monitor  2: Mirror  3: Water
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
    // 0) Plain（味付けなし：Flip + Sample + Tint/Opacityのみ）
    // ============================================================
    if (uMode == 0)
    {
        vec4 c = SampleSurface(uv);
        c.rgb *= uTint;
        c.a   *= uOpacity;
        outColor = c;
        return;
    }

    // ============================================================
    // 1) Monitor
    // ============================================================
    if (uMode == 1)
 /*   {
        vec4 c = SampleSurface(uv);

        float sl = sin((uv.y + uTime * 0.4) * 800.0) * 0.5 + 0.5;
        float k  = mix(1.0 - uScanlineStrength, 1.0, sl);
        c.rgb *= k;

        c.rgb *= uTint;
        c.a   *= uOpacity;
        outColor = c;
        return;
    }
    */
    {
        // ---- (A) 横方向の歪み（強め）
        vec2 uvM = uv;
        uvM.x += sin(uvM.y * 3.0 + uTime * 0.9) * 0.006;   // ←強め
        uvM.y += sin(uvM.x * 2.0 - uTime * 0.6) * 0.002;

        vec4 c = SampleSurface(uvM);

        // ---- (B) 走査線：細線 + 太ムラ（極端）
        float slFine  = sin((uvM.y + uTime * 0.55) * 1400.0) * 0.5 + 0.5;
        float slThick = sin((uvM.y - uTime * 0.18) * 260.0)  * 0.5 + 0.5;

        // 細線を「暗線」に寄せる（かなり落とす）
        // 0..1 -> 0.35..1.0 くらい
        float line = 0.35 + 0.65 * slFine;

        // 太ムラ：0.75..1.15 くらい
        float blot = 0.75 + 0.40 * slThick;

        // ---- (C) 微フリッカー（輝度の脈動）
        float flicker = 0.92 + 0.08 * (sin(uTime * 9.0) * 0.5 + 0.5);

        float k = line * blot * flicker;

        // uScanlineStrength で適用量を調整（0..1）
        c.rgb *= mix(1.0, k, clamp(uScanlineStrength, 0.0, 1.0));

        // ---- (D) 色調を落とす（彩度↓ + ちょい色かぶせ）
        float luma = dot(c.rgb, vec3(0.299, 0.587, 0.114));

        // 彩度を強めに落とす（0.0=完全グレイ、1.0=元色）
        vec3 desat = mix(vec3(luma), c.rgb, 0.25);

        // モニタっぽい色かぶせ（緑がかる/セピア寄りどっちでも）
        // ここは好み：グリーン寄り
        vec3 castCol = vec3(0.90, 1.00, 0.92);

        // さらに黒を締める（コントラスト）
        desat = pow(desat, vec3(1.20));

        c.rgb = desat * castCol;

        // ---- (E) 既存反映
        c.rgb *= uTint;
        c.a   *= uOpacity;
        outColor = c;
        return;
    }
    
/*    {
        vec2 uvM = uv;

        // 強度（既存 uniform を流用）
        float I = clamp(uScanlineStrength, 0.0, 1.0);

        // ベース色
        vec4 c = SampleSurface(uvM);

        // ------------------------------------------------------------
        // (A) 走査線（控えめ〜中）
        // ------------------------------------------------------------
        float scan = 0.90 + 0.10 * sin((uvM.y * 2.0 - 1.0) * 900.0);
        c.rgb *= mix(1.0, scan, 0.65 * I);

        // ------------------------------------------------------------
        // (B) “ローリング黒帯”：上に流れる太い帯
        //   ・phase が 0..1 をループ
        //   ・center に近いほど暗く
        // ------------------------------------------------------------
        float t = uTime;

        // 上に流す（速度）
        float speed = 0.18;          // 0.10〜0.35で調整
        float phase = fract(uvM.y + t * speed);

        // 帯の中心位置（0..1）と幅
        float center = 0.55;         // 位置（上寄り/下寄り）
        float halfW  = 0.10;         // 帯の“半幅”（0.06〜0.18くらい）

        // center からの距離（0..）
        float d = abs(phase - center);

        // 帯の形：中心が濃く、外側がフェード（ガウスっぽく）
        // d=0 -> 1, dが大きい -> 0
        float bar = exp(- (d * d) / (halfW * halfW));

        // さらに “帯の縁が少し見える” 感（好みで）
        // ※縁が強すぎると違和感なので弱め
        float edge = smoothstep(0.0, halfW, d) * (1.0 - smoothstep(halfW, halfW * 1.6, d));
        bar = clamp(bar + edge * 0.35, 0.0, 1.0);

        // 暗くする量（I で強度）
        float dark = 1.0 - (0.55 * I) * bar;   // 0.35〜0.75あたりで調整
        c.rgb *= dark;

        // ------------------------------------------------------------
        // (C) 色調を落とす（それっぽい “くすみ”）
        // ------------------------------------------------------------
        float luma = dot(c.rgb, vec3(0.299, 0.587, 0.114));
        c.rgb = mix(vec3(luma), c.rgb, mix(1.0, 0.70, I)); // 彩度落とす
        c.rgb *= mix(1.0, 0.92, I);                         // ちょい暗め

        // ------------------------------------------------------------
        // (D) 既存反映
        // ------------------------------------------------------------
        c.rgb *= uTint;
        c.a   *= uOpacity;

        outColor = c;
        return;
    }
    */
    // ============================================================
    // 2) Mirror
    // ============================================================
    if (uMode == 2)
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
    // 3) Water（空反射・ぼやけ重視）
    // ============================================================
    if (uMode == 3)
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
        vec2 uv2 = ApplyFlip(vUV);

        uv2 += vec2(
            sin(t * 0.3 + uv2.y * 2.0),
            cos(t * 0.25 + uv2.x * 1.8)
        ) * 0.412;

        // --------------------------------------------
        // (C) 歪み（極端）
        // --------------------------------------------
        vec2 blurDistort = vec2(
            swell - 0.5,
            (1.0 - swell) - 0.5
        );
        uv2 += blurDistort * 0.115;

        // --------------------------------------------
        // (C') 破綻防止：UVをループさせる（マンガ向き）
        // --------------------------------------------
        uv2 = fract(uv2);

        // --------------------------------------------
        // (D) 擬似ぼかし（極端だけど制御）
        // --------------------------------------------
        vec2 ofs = vec2(0.206, 0.206) * (0.7 + 0.6 * swell);

        vec4 c = texture(uSurfaceTex, uv2);
        c += texture(uSurfaceTex, fract(uv2 + ofs));
        c += texture(uSurfaceTex, fract(uv2 - ofs));
        c += texture(uSurfaceTex, fract(uv2 + vec2(-ofs.x, ofs.y)));
        c += texture(uSurfaceTex, fract(uv2 + vec2(ofs.x, -ofs.y)));
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
