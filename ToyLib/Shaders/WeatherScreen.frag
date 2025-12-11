#version 410 core

//==================================================
// WeatherScreen.frag
// 画面全体に重ねる「前景エフェクト」用シェーダ
// - 雨スジ
// - 雪
// - もやっとした前景フォグ
// - レンズフレア（ゴースト）
//==================================================

out vec4 FragColor;

//------------------------------
// Uniforms
//------------------------------
uniform float uTime;          // 経過時間（アニメーション用）
uniform vec2  uResolution;    // 画面サイズ
uniform float uRainAmount;    // 雨の強さ  0.0〜1.0
uniform float uSnowAmount;    // 雪の強さ  0.0〜1.0
uniform float uFogAmount;     // フォグの強さ 0.0〜1.0

// ★ レンズフレア用
uniform vec2  uSunPos;        // 画面上の太陽位置 (0〜1, 左上原点)
uniform float uFlareIntensity;// フレアの強さ 0.0〜1.0（遮蔽などで外から制御）
uniform vec3  uFlareColor;    // 太陽の基本色

// 雪の粒の数
const int SNOW_COUNT = 80;

//==================================================
// 共通：ハッシュ＆ノイズ
//==================================================

//--- ハッシュ関数（float 版） ---
float hash(float x)
{
    return fract(sin(x) * 43758.5453123);
}

//--- ハッシュ関数（vec2 版） ---
float hash(vec2 p)
{
    return fract(sin(dot(p, vec2(27.619, 57.583))) * 43758.5453);
}

//--- 2D value noise ---
float noise(vec2 p)
{
    vec2 i = floor(p);
    vec2 f = fract(p);
    
    float a = hash(i);
    float b = hash(i + vec2(1.0, 0.0));
    float c = hash(i + vec2(0.0, 1.0));
    float d = hash(i + vec2(1.0, 1.0));
    
    vec2 u = f * f * (3.0 - 2.0 * f);
    
    return mix(mix(a, b, u.x),
               mix(c, d, u.x), u.y);
}

//--- fbm（Fractal Brownian Motion） ---
float fbm(vec2 p)
{
    float value = 0.0;
    float amp   = 0.5;
    
    for (int i = 0; i < 4; i++)
    {
        value += amp * noise(p);
        p     *= 2.0;
        amp   *= 0.5;
    }
    return value;
}

//==================================================
// 雨エフェクト
//==================================================
float rainPattern(vec2 uv)
{
    uv *= vec2(300.0, 1.0);
    uv.y += uTime * 8.0;

    float id     = floor(uv.x);
    float offset = hash(vec2(id, 0.0));

    float y = fract(uv.y + offset);

    float shape = smoothstep(0.0, 0.01, y) * (1.0 - y);

    return shape;
}

//==================================================
// 雪エフェクト
//==================================================
float snowPattern(vec2 uv)
{
    float brightness = 0.0;

    for (int i = 0; i < SNOW_COUNT; i++)
    {
        float fi = float(i);

        float x = hash(fi * 1.3) + sin(uTime * 0.2 + fi) * 0.01;
        float speed = 0.1 + hash(fi * 3.2) * 0.5;

        float y = fract(hash(fi * 2.1) - uTime * speed);

        vec2 snowPos = vec2(x, y);

        float dist = length(uv - snowPos);
        float size = 0.01 + hash(fi * 4.0) * 0.01;

        brightness += smoothstep(size, 0.0, dist);
    }

    return brightness;
}

//==================================================
// 前景フォグエフェクト
//==================================================
float fogPattern(vec2 uv)
{
    vec2 centeredUV = (gl_FragCoord.xy - 0.5 * uResolution) / uResolution.y;

    vec2 noiseUV = centeredUV * 1.5;

    float n = fbm(noiseUV + vec2(0.0, uTime * 0.02));

    return smoothstep(0.3, 1.0, n);
}

//==================================================
// レンズフレア（ゴースト）
//==================================================

// ピクセル座標ベースの円盤マスク
//  centerPx : 円の中心（ピクセル）
//  radius   : 半径（画面の短辺に対する 0〜1）
//  soft     : ふちのボケ量
float discMaskPx(vec2 centerPx, float radius, float soft)
{
    float minDim = min(uResolution.x, uResolution.y);
    float dist   = length(gl_FragCoord.xy - centerPx) / minDim;

    // dist = 0   → ほぼ1
    // dist > r+s → 0 にフェードアウト
    float m = 1.0 - smoothstep(radius - soft, radius + soft, dist);
    return clamp(m, 0.0, 1.0);
}

// ゴースト + うっすら円のレンズフレア合成
vec3 computeLensFlare()
{
    if (uFlareIntensity <= 0.0)
        return vec3(0.0);

    // 画面中心と太陽位置（ピクセル）
    vec2 centerUV = vec2(0.5, 0.5);
    vec2 centerPx = centerUV * uResolution;
    vec2 sunPx    = uSunPos * uResolution;

    // 太陽 → 画面中心 への軸
    vec2 axis     = centerPx - sunPx;
    float axisLen = length(axis);
    if (axisLen < 1e-4) axisLen = 1e-4;
    vec2 dir = axis / axisLen;

    // 太陽が画面の端に寄ったら全体フェードアウト
    float distCenter = axisLen / length(vec2(uResolution.x, uResolution.y));
    float edgeFade   = 1.0 - smoothstep(0.3, 0.9, distCenter);
    if (edgeFade <= 0.0)
        return vec3(0.0);

    float minDim = min(uResolution.x, uResolution.y);
    vec3  color  = vec3(0.0);

    // -----------------------------
    // 太陽まわりのハロー（かなり暗め）
    // -----------------------------
    {
        vec2  d    = gl_FragCoord.xy - sunPx;
        float dist = length(d) / minDim;

        float outerGlow = smoothstep(0.5, 0.0, dist);
        float innerGlow = smoothstep(0.20, 0.0, dist);

        float halo = outerGlow * 0.4 + innerGlow * 0.2;

        color += uFlareColor * halo * 0.25 * uFlareIntensity * edgeFade;
    }

    // ------------------------------------------------
    // 軸上に並ぶゴースト（30個の半透明円盤）
    // 画面の対角線方向に画面全域を使って配置
    // ------------------------------------------------
    const int GHOST_COUNT = 30;

    // ベース半径（太陽に近いほど小さく、遠いほど大きく）
    float baseRadius[GHOST_COUNT] = float[](
        0.02, 0.022, 0.025, 0.028, 0.030,
        0.033, 0.036, 0.040, 0.043, 0.046,
        0.050, 0.053, 0.057, 0.060, 0.064,
        0.067, 0.071, 0.074, 0.078, 0.082,
        0.086, 0.090, 0.095, 0.10,  0.11,
        0.12, 0.13,  0.14,  0.15,  0.16
    );

    // カラフルなテーブル（虹っぽい）
    vec3 ghostColor[GHOST_COUNT] = vec3[](
        vec3(1.00, 0.35, 0.35), // 0
        vec3(1.00, 0.50, 0.30),
        vec3(1.00, 0.65, 0.25),
        vec3(1.00, 0.80, 0.30),
        vec3(0.95, 0.95, 0.35),
        vec3(0.80, 1.00, 0.40),
        vec3(0.60, 1.00, 0.50),
        vec3(0.45, 1.00, 0.65),
        vec3(0.35, 1.00, 0.80),
        vec3(0.30, 1.00, 1.00),
        vec3(0.30, 0.80, 1.00),
        vec3(0.35, 0.60, 1.00),
        vec3(0.45, 0.45, 1.00),
        vec3(0.60, 0.35, 1.00),
        vec3(0.80, 0.30, 1.00),
        vec3(1.00, 0.30, 1.00),
        vec3(1.00, 0.30, 0.80),
        vec3(1.00, 0.30, 0.60),
        vec3(1.00, 0.30, 0.45),
        vec3(1.00, 0.40, 0.40),
        vec3(1.00, 0.55, 0.30),
        vec3(1.00, 0.70, 0.25),
        vec3(1.00, 0.85, 0.30),
        vec3(0.95, 0.95, 0.35),
        vec3(0.75, 1.00, 0.45),
        vec3(0.55, 1.00, 0.60),
        vec3(0.40, 1.00, 0.80),
        vec3(0.30, 0.90, 1.00),
        vec3(0.35, 0.60, 1.00),
        vec3(0.45, 0.45, 1.00)
    );

    // 画面の対角線長
    float span = length(uResolution);

    for (int i = 0; i < GHOST_COUNT; ++i)
    {
        // 0〜1 のインデックス位置
        float t = float(i) / float(GHOST_COUNT - 1);

        // --- サイズに「波」を乗せる ---------------------
        float baseR = baseRadius[i];

        // ゆっくりした波（freq を変えると大きい区間の個数が変わる）
        float freq = 1.5;  // 1.0〜2.0 くらいで調整
        float wave = 0.5 + 0.5 * sin(6.28318 * (t * freq + 0.25));

        // 波の山の部分だけを持ち上げるマスク
        float bigMask = smoothstep(0.6, 0.9, wave);

        float smallScale = 0.7;  // 山じゃないところ
        float bigScale   = 1.6;  // 山のところ（まとめて大きい区間）
        float sizeFactor = mix(smallScale, bigScale, bigMask);

        float r = baseR * sizeFactor;
        // ---------------------------------------------

        // ★ 画面全域を使うように、対角線方向に配置
        //   t = 0 → 画面の片端、t = 1 → 反対側
        float s = mix(-span * 0.6, span * 0.6, t); // 0.6 を 0.5〜0.8 くらいで好み調整

        // ゴースト中心
        vec2 gCenterPx = centerPx + dir * s;

        // ピクセル距離（正規化）
        vec2  d    = gl_FragCoord.xy - gCenterPx;
        float dist = length(d) / minDim;

        // 単色の円盤マスク
        float soft  = r * 0.6;
        float edge0 = max(r - soft, 0.0);
        float edge1 = r;

        float disk = 1.0 - smoothstep(edge0, edge1, dist);
        disk = pow(disk, 1.4); // 中心を少し強めに

        // そのゴーストの色
        vec3 gCol = ghostColor[i] * disk
                    * uFlareIntensity * edgeFade * 0.5;

        color += gCol;
    }

    // 全体を少し抑えめに
    color *= 0.3;

    return color;
}
/*
vec3 computeLensFlare()
{
    if (uFlareIntensity <= 0.0)
        return vec3(0.0);

    vec2 centerUV = vec2(0.5, 0.5);
    vec2 centerPx = centerUV * uResolution;
    vec2 sunPx    = uSunPos * uResolution;

    vec2 axis     = centerPx - sunPx;
    float axisLen = length(axis);
    if (axisLen < 1e-4) axisLen = 1e-4;
    vec2 dir = axis / axisLen;

    float distCenter = axisLen / length(vec2(uResolution.x, uResolution.y));
    float edgeFade   = 1.0 - smoothstep(0.3, 0.9, distCenter);
    if (edgeFade <= 0.0)
        return vec3(0.0);

    float minDim = min(uResolution.x, uResolution.y);
    vec3  color  = vec3(0.0);

    // -----------------------------
    // 太陽まわりのハロー（かなり暗め）
    // -----------------------------
    {
        vec2  d    = gl_FragCoord.xy - sunPx;
        float dist = length(d) / minDim;

        float outerGlow = smoothstep(0.5, 0.0, dist);
        float innerGlow = smoothstep(0.20, 0.0, dist);

        float halo = outerGlow * 0.4 + innerGlow * 0.2;

        color += uFlareColor * halo * 0.25 * uFlareIntensity * edgeFade;
    }

    // ------------------------------------------------
    // 軸上に並ぶゴースト（半透明円盤・色とサイズを t で決定）
    // ------------------------------------------------

    const int GHOST_COUNT = 30;

    // 位置だけは今までのテーブルを使用
    float posFactor[GHOST_COUNT] = float[](
        -2.5, -2.2, -1.9, -1.6, -1.3,
        -1.1, -0.9, -0.7, -0.5, -0.3,
        -0.1,  0.1,  0.3,  0.5,  0.7,
         0.9,  1.1,  1.4,  1.7,  2.0,
         2.4,  2.8,  3.2,  3.6,  4.0,
         4.5,  5.0,  5.5,  6.0,  6.5
    );

    // 半径のレンジ
    float minRadius = 0.02;
    float maxRadius = 0.16;

    for (int i = 0; i < GHOST_COUNT; ++i)
    {
        // 0〜1 の位置インデックス
        float t = float(i) / float(GHOST_COUNT - 1);

        // ② 太陽に近いほど小さく、遠いほど大きく（指数的）
        float r = mix(minRadius, maxRadius, pow(t, 1.5));

        // ゴースト中心
        vec2 gCenterPx = centerPx + axis * posFactor[i];

        // ピクセル距離（正規化）
        vec2  d    = gl_FragCoord.xy - gCenterPx;
        float dist = length(d) / minDim;

        // 単色円盤マスク
        float soft  = r * 0.6;
        float edge0 = max(r - soft, 0.0);
        float edge1 = r;

        float disk = 1.0 - smoothstep(edge0, edge1, dist);
        disk = pow(disk, 1.4);   // 中心を強く・外周を弱める

        // ① t からレインボー色を生成（位相をずらした sin）
        float angle = 6.2831853 * t; // 2πt
        vec3 rainbow;
        rainbow.r = 0.5 + 0.5 * sin(angle + 0.0);
        rainbow.g = 0.5 + 0.5 * sin(angle + 2.0943951); // +120°
        rainbow.b = 0.5 + 0.5 * sin(angle + 4.1887902); // +240°

        vec3 gCol = rainbow * disk
                    * uFlareIntensity * edgeFade * 0.5;

        color += gCol;
    }


    // 全体の明るさを少し抑える
    color *= 0.3;

    return color;
}
*/
//==================================================
// メイン
//==================================================
void main()
{
    // 0〜1 に正規化した画面座標
    vec2 uv = gl_FragCoord.xy / uResolution;

    //----------------------------------------
    // 1) 雨・雪・フォグのアルファマスク
    //----------------------------------------
    float alpha = 0.0;

    // 雨
    if (uRainAmount > 0.01)
    {
        float rain = rainPattern(uv);
        alpha += rain * uRainAmount * 0.25;
    }

    // 雪
    if (uSnowAmount > 0.01)
    {
        float snow = snowPattern(uv);
        alpha += snow * uSnowAmount * 1.2;
    }

    // フォグ
    if (uFogAmount > 0.01)
    {
        float fog = fogPattern(uv);
        alpha += fog * uFogAmount * 0.9;
    }

    alpha = clamp(alpha, 0.0, 1.0);

    // 前景エフェクトは白マスクとして乗せる
    vec3 overlay = vec3(1.0) * alpha;

    //----------------------------------------
    // 2) レンズフレア（ゴースト）色
    //----------------------------------------
    vec3 flare = computeLensFlare();

    // フレアの明るさから「追加のアルファ」を作る
    float flareLuma = max(flare.r, max(flare.g, flare.b)); // 0〜∞
    flareLuma = clamp(flareLuma, 0.0, 1.0);

    // フレア由来のアルファ（強すぎたら 0.5 とかに落として調整）
    float flareAlpha = flareLuma * 0.7;

    // 最終アルファ：天候アルファ + フレアアルファ
    float finalAlpha = clamp(alpha + flareAlpha, 0.0, 1.0);

    // 色はそのまま overlay + flare を使う
    vec3 finalColor = overlay + flare;

    FragColor = vec4(finalColor, finalAlpha);
}
