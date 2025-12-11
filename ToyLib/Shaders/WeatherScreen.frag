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
    color *= 0.25;

    return color;
}

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

/*
#version 410 core

//==================================================
// WeatherOverlay.frag
// 画面全体に重ねる「前景エフェクト」用シェーダ
// - 雨スジ
// - 雪
// - もやっとした前景フォグ
// - レンズフレア（ゴースト）
//==================================================

out vec4 FragColor;

// 頂点シェーダ側が vUV を出している前提:
//   layout(location=1) out vec2 vUV;
in vec2 vUV;

//------------------------------
// Uniforms
//------------------------------
uniform float uTime;          // 経過時間（アニメーション用）
uniform vec2  uResolution;    // 画面サイズ (pixels)
uniform float uRainAmount;    // 雨の強さ  0.0〜1.0
uniform float uSnowAmount;    // 雪の強さ  0.0〜1.0
uniform float uFogAmount;     // フォグの強さ 0.0〜1.0

// ★ レンズフレア用
uniform vec2  uSunPos;        // 画面上の太陽位置 (0〜1, 左上原点)
uniform float uFlareIntensity;// フレアの強さ 0.0〜1.0
uniform vec3  uFlareColor;    // フレアのベース色（太陽色）

// 雪の粒の数
const int SNOW_COUNT = 80;

//==================================================
// 共通：ハッシュ＆ノイズ
//==================================================
float hash(float x)
{
    return fract(sin(x) * 43758.5453123);
}

float hash(vec2 p)
{
    return fract(sin(dot(p, vec2(27.619, 57.583))) * 43758.5453);
}

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
// レンズフレア用マスク
//==================================================

// ドーナツ状マスク（半径 radius の輪っか）
float ringMask(float d, float radius, float thickness, float blur)
{
    float inner = radius - thickness * 0.5;
    float outer = radius + thickness * 0.5;

    float innerEdge = smoothstep(inner - blur, inner + blur, d); // 0 → 1
    float outerEdge = smoothstep(outer - blur, outer + blur, d); // 0 → 1

    // inner より外 ＆ outer より内
    float ring = innerEdge * (1.0 - outerEdge);
    return ring;
}

// 内側のソフトなグラデーション
float innerFill(float d, float radius, float softness)
{
    // 0 〜 radius で 1 → 0 に落ちる
    float t = smoothstep(0.0, radius, d);
    return pow(1.0 - t, softness);
}

//==================================================
// レンズフレア（ゴーストたち）
//==================================================
vec3 computeLensFlare(vec2 uv)
{
    if (uFlareIntensity <= 0.001)
        return vec3(0.0);

    // 画面中心（uv 空間）
    vec2 screenCenter = vec2(0.5, 0.5);
    vec2 dir          = screenCenter - uSunPos;   // 太陽 → 画面中心
    float dirLen      = length(dir);
    if (dirLen < 1e-4)
        return vec3(0.0);

    vec2 dirN = dir / dirLen;

    // ピクセル系で円を描くためのスケール
    float normScale = 1.0 / min(uResolution.x, uResolution.y);

    // ゴーストのパラメータ
    const int GHOST_COUNT = 6;
    float posFactor[GHOST_COUNT] = float[](
        -0.5,  // 太陽の外側
         0.2,
         0.6,
         1.0,
         1.4,
         1.8   // 画面の向こう側
    );
    float radius[GHOST_COUNT] = float[](
        0.22,
        0.18,
        0.15,
        0.20,
        0.17,
        0.12
    );
    float thickness[GHOST_COUNT] = float[](
        0.04,
        0.035,
        0.03,
        0.04,
        0.035,
        0.03
    );

    vec3 ghostColor[GHOST_COUNT] = vec3[](
        vec3(1.0, 0.65, 0.45),
        vec3(0.7, 1.0, 0.6),
        vec3(0.6, 0.8, 1.0),
        vec3(1.0, 0.9, 0.6),
        vec3(0.8, 0.6, 1.0),
        vec3(0.6, 1.0, 0.9)
    );

    vec3 col = vec3(0.0);

    // 各ゴーストを線上に並べる
    for (int i = 0; i < GHOST_COUNT; ++i)
    {
        vec2 ghostUv = uSunPos + dirN * dirLen * posFactor[i];

        // UV → ピクセル座標へ
        vec2 ghostPx = ghostUv * uResolution;

        // 現フラグメントとの距離（ピクセル）
        vec2 deltaPx = gl_FragCoord.xy - ghostPx;
        float d = length(deltaPx) * normScale;  // 0〜? に正規化

        // ドーナツ
        float ring = ringMask(d, radius[i], thickness[i], 0.02);

        // 中のぼんやりグラデーション（半径は少し小さく）
        float fill = innerFill(d, radius[i] - thickness[i] * 0.4, 2.0);

        // 縁はハッキリ＋内側はぼんやり
        float intensity = ring * 1.4 + fill * 0.5;

        col += ghostColor[i] * intensity;
    }

    // 太陽が画面端寄りのときは少し弱める
    float edgeFade = 1.0 - smoothstep(0.4, 0.9, dirLen);
    col *= uFlareIntensity * edgeFade * 0.9;

    return col;
}

//==================================================
// メイン
//==================================================
void main()
{
    vec2 uv = gl_FragCoord.xy / uResolution;

    //----------------------------------------
    //  まず背景を全部真っ黒にクリア（重要）
    //----------------------------------------
    vec3 base = vec3(0.0);
    float baseA = 0.0;

    //----------------------------------------
    // ★ テスト用：太陽を赤丸で強制描画
    //----------------------------------------
    // 太陽の位置 (0〜1)
    vec2 sun = uSunPos;

    // ピクセル座標
    vec2 sunPx = sun * uResolution;

    // 距離（正規化）
    vec2 d = gl_FragCoord.xy - sunPx;
    float dist = length(d) / min(uResolution.x, uResolution.y);

    float circle = smoothstep(0.25, 0.0, dist);

    vec3 flareColor = vec3(1.0, 0.2, 0.2) * circle;
    float flareAlpha = circle;

    //----------------------------------------
    // ★ この時点で絶対に見える（赤丸）
    //----------------------------------------
    vec3 finalColor = base + flareColor;
    float finalAlpha = clamp(baseA + flareAlpha, 0.0, 1.0);

    FragColor = vec4(finalColor, finalAlpha);
}
 */
 
 /*
#version 410 core

//==================================================
// WeatherScreen.frag
// 画面全体に重ねる「前景エフェクト」用シェーダ
// - 雨スジ
// - 雪
// - もやっとした前景フォグ
// - レンズフレア（太陽）
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
uniform float uFlareIntensity;// フレアの強さ 0.0〜1.0（C++側で遮蔽など加味）
uniform vec3  uFlareColor;    // フレアの基本色（太陽色）

// 雪の粒の数
const int SNOW_COUNT = 80;

//==================================================
// 共通：ハッシュ＆ノイズ
//==================================================

float hash(float x)
{
    return fract(sin(x) * 43758.5453123);
}

float hash(vec2 p)
{
    return fract(sin(dot(p, vec2(27.619, 57.583))) * 43758.5453);
}

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
// レンズフレア（太陽）
//==================================================

// ★ アスペクト比補正付きの丸マスク
float circleMask(vec2 uv, vec2 center, float radius, float softness)
{
    // uv空間(0〜1)を高さ基準に揃えるために x を補正
    vec2 p = uv - center;
    float aspect = uResolution.x / uResolution.y;
    p.x *= aspect;

    float d = length(p);
    float x = clamp(d / radius, 0.0, 1.0);

    return pow(1.0 - smoothstep(0.0, 1.0, x), softness);
}

// ----------------------------------------
// レンズフレア（ゴースト＋スジ）
// ----------------------------------------
vec3 computeLensFlare(vec2 uv)
{
    if (uFlareIntensity <= 0.0)
        return vec3(0.0);

    vec2 screenCenter = vec2(0.5, 0.5);
    vec2 dir          = screenCenter - uSunPos;
    float distCenter  = length(dir);

    // 軸だけ取り出す（太陽 → 画面中心）
    vec2 axis = normalize(dir + 1e-6);

    // ゴーストを並べるスパン（範囲）
    //   - 太陽〜中心距離をベースに
    //   - 最低 0.3 は確保
    //   - 全体を 2.0 倍くらいに伸ばす
    float flareSpan = max(distCenter, 0.3) * 2.0;

    // 太陽が画面端に近づくほどフレアを弱くする
    float edgeFade = 1.0 - smoothstep(0.4, 0.85, distCenter);
    float baseInt  = uFlareIntensity * edgeFade;

    vec3 color = vec3(0.0);

    //--------------------------------------------------
    // ★ ゴースト：数を増やし、半径も大きめに
    //--------------------------------------------------
    const int GHOST_COUNT = 16;

    float posFactor[GHOST_COUNT] = float[](
        -1.8, -1.4, -1.0, -0.7,
        -0.4, -0.2,  0.0,  0.2,
         0.4,  0.7,  1.0,  1.3,
         1.6,  1.9,  2.2,  2.5
    );

    float sizeFactor[GHOST_COUNT] = float[](
        0.07, 0.06, 0.055, 0.05,
        0.045,0.04, 0.035, 0.04,
        0.045,0.05, 0.055, 0.06,
        0.065,0.07, 0.075, 0.08
    );

    vec3 ghostColor[GHOST_COUNT] = vec3[](
        vec3(1.0, 0.85, 0.7),
        vec3(0.8, 1.0, 0.7),
        vec3(0.7, 0.9, 1.0),
        vec3(1.0, 0.75, 0.9),
        vec3(1.0, 0.9, 0.7),
        vec3(0.8, 0.9, 1.0),
        vec3(0.9, 1.0, 0.8),
        vec3(1.0, 0.8, 0.6),
        vec3(0.7, 0.85, 1.0),
        vec3(1.0, 0.7, 0.6),
        vec3(0.9, 1.0, 0.9),
        vec3(1.0, 0.9, 0.95),
        vec3(0.8, 0.95, 1.0),
        vec3(1.0, 0.8, 0.8),
        vec3(0.9, 0.95, 0.7),
        vec3(0.95,0.95,1.0)
    );

    // ゴースト本体
    for (int i = 0; i < GHOST_COUNT; ++i)
    {
        vec2 gPos = uSunPos + axis * flareSpan * posFactor[i];
        float r    = sizeFactor[i];

        // ふわっと大きめの丸
        float m = circleMask(uv, gPos, r, 2.0);

        // ★ 強めの“加算風”ゲイン
        float ghostGain = 1.3;
        color += ghostColor[i] * m * baseInt * ghostGain;
    }

    //--------------------------------------------------
    // スジ（streak）も少し強めに
    //--------------------------------------------------
    {
        vec2 rel  = uv - screenCenter;
        float along  = dot(rel, axis);
        float normal = dot(rel, vec2(-axis.y, axis.x));

        float line = exp(-pow(abs(normal) * 18.0, 2.0));
        float streakFade =
            smoothstep(-1.0, 0.2, along) *
            smoothstep( 1.0, -0.2, along);

        color += uFlareColor * line * streakFade * baseInt * 0.35;
    }

    //--------------------------------------------------
    // ★ 加算っぽく見せるための“疑似 HDR カーブ”
    //--------------------------------------------------
    // 明るいところほど強く、暗いところは控えめに
    color = 1.0 - exp(-color * 2.5);

    return color;
}
//==================================================
// メイン
//==================================================
void main()
{
    // 0〜1 に正規化した画面座標
    vec2 uv = gl_FragCoord.xy / uResolution;

    // アルファの蓄積用（雨・雪・フォグ）
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

    // 前景エフェクトは白
    vec3 overlay = vec3(1.0) * clamp(alpha, 0.0, 1.0);

    // レンズフレア色を計算（ここをちょっと強めにしてもOK）
    vec3 flare = computeLensFlare(uv) * 1.5;  // ★ゴースト強調用に 1.5 倍

    vec3 finalColor = overlay + flare;

    // --- ここが重要：フレアからもアルファを作る ---
    float flareAlpha = 0.0;
    if (uFlareIntensity > 0.01)
    {
        // フレアの明るさから α を作る（ざっくり最大成分）
        float maxComp = max(max(flare.r, flare.g), flare.b);
        flareAlpha = clamp(maxComp, 0.0, 1.0);
    }

    float finalAlpha = clamp(alpha + flareAlpha, 0.0, 1.0);

    FragColor = vec4(finalColor, finalAlpha);
}
*/
/*
#version 410 core

//==================================================
// WeatherScreen.frag
// 画面全体に重ねる「前景エフェクト」用シェーダ
// - 雨スジ
// - 雪
// - もやっとした前景フォグ
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
//
// 縦方向に伸びた「雨スジ」をランダムな x 位置に配置。
// y 方向に時間でスクロールさせて落ちているように見せる。
//--------------------------------------------------
float rainPattern(vec2 uv)
{
    // 横方向の密度を上げ（300）、縦だけ時間でスクロール
    uv *= vec2(300.0, 1.0);
    uv.y += uTime * 8.0;

    // 各スジごとの ID とオフセット
    float id     = floor(uv.x);
    float offset = hash(vec2(id, 0.0));

    // 0〜1 の範囲で繰り返す縦位置
    float y = fract(uv.y + offset);

    // 細くて尻尾のある形状
    float shape = smoothstep(0.0, 0.01, y) * (1.0 - y);

    return shape;
}

//==================================================
// 雪エフェクト
//==================================================
//
// SNOW_COUNT 個の粒をランダム配置し、時間で y を下に流す。
// 1 粒ごとにランダムな大きさ・速度を付与。
//--------------------------------------------------
float snowPattern(vec2 uv)
{
    float brightness = 0.0;

    for (int i = 0; i < SNOW_COUNT; i++)
    {
        float fi = float(i);

        // x 位置（わずかに左右に揺らす）
        float x = hash(fi * 1.3) + sin(uTime * 0.2 + fi) * 0.01;
        
        // 落下速度
        float speed = 0.1 + hash(fi * 3.2) * 0.5;

        // y は時間で下方向へスクロール（fract でループ）
        float y = fract(hash(fi * 2.1) - uTime * speed);

        vec2 snowPos = vec2(x, y);

        // uv からの距離で丸い粒にする
        float dist = length(uv - snowPos);

        // ランダムサイズ
        float size = 0.01 + hash(fi * 4.0) * 0.01;

        // 中心ほど明るい丸い粒
        brightness += smoothstep(size, 0.0, dist);
    }

    return brightness;
}

//==================================================
// 前景フォグエフェクト
//==================================================
//
// 画面中央を基準に、ノイズで「もやっ」とした濃淡を作る。
//--------------------------------------------------
float fogPattern(vec2 uv)
{
    // 画面中央原点・縦幅基準で正規化
    vec2 centeredUV = (gl_FragCoord.xy - 0.5 * uResolution) / uResolution.y;

    // ノイズ用のスケール
    vec2 noiseUV = centeredUV * 1.5;

    // ゆっくり流れる fbm ノイズ
    float n = fbm(noiseUV + vec2(0.0, uTime * 0.02));

    // ノイズ値をフォグ濃度にマッピング
    return smoothstep(0.3, 1.0, n);
}

//==================================================
// メイン
//==================================================
void main()
{
    // 0〜1 に正規化した画面座標
    vec2 uv = gl_FragCoord.xy / uResolution;

    // アルファの蓄積用
    float alpha = 0.0;

    // 雨の重ね合わせ
    if (uRainAmount > 0.01)
    {
        float rain = rainPattern(uv);
        alpha += rain * uRainAmount * 0.25; // 雨はやや控えめ
    }

    // 雪の重ね合わせ
    if (uSnowAmount > 0.01)
    {
        float snow = snowPattern(uv);
        alpha += snow * uSnowAmount * 1.2; // 雪は少し強め
    }

    // フォグの重ね合わせ
    if (uFogAmount > 0.01)
    {
        float fog = fogPattern(uv);
        alpha += fog * uFogAmount * 0.9;
    }

    // すべて白い前景エフェクトとして合成（色は vec3(1.0)）
    FragColor = vec4(vec3(1.0), clamp(alpha, 0.0, 1.0));
}
*/
