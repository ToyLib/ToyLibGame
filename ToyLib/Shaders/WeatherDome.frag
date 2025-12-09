#version 410 core

//======================================================================
// WeatherDome.frag
//
// ・全天を覆うスカイドーム用フラグメントシェーダ
// ・時間帯・天候・太陽方向から、空／雲／太陽／星／月／天の川を表現
//
//  uWeatherType : 0 Clear, 1 Cloudy, 2 Rain, 3 Storm, 4 Snow
//  uTimeOfDay   : 0.0〜1.0（夜→昼→夜）
//  uSunDir      : 太陽のワールド方向ベクトル
//
//  構成：
//    - 2D/3D ハッシュ＆ノイズ
//    - fbm / fbm3（Fractal Brownian Motion）
//    - ベース空色決定（天候＋時間帯）
//    - 雲の密度・色・カバー率
//    - 雷（Storm）
//    - 雪天の色味調整（Snow）
//    - 夜間のみの星／月／天の川(Milky Way)
//    - 太陽ハイライト（サンディスク＆グロー）
//======================================================================

out vec4 FragColor;
in vec3 vWorldDir;

//-------------------------
// 共通 Uniform
//-------------------------
uniform float uTime;
uniform int  uWeatherType;     // 0: Clear, 1: Cloudy, 2: Rain, 3: Storm, 4: Snow
uniform float uTimeOfDay;      // 0.0〜1.0（夜→昼→夜）
uniform vec3 uSunDir;          // 太陽方向（ワールド空間）
uniform vec3 uMoonDir;         // 月方向

// C++ 側から渡される「素」の空色・雲色
uniform vec3 uRawSkyColor;
uniform vec3 uRawCloudColor;


//======================================================================
// ハッシュ / ノイズ（2D）
//======================================================================
float hash12(vec2 p)
{
    vec3 p3 = fract(vec3(p.x, p.y, p.x) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}

float vnoise(vec2 p)
{
    vec2 i = floor(p);
    vec2 f = fract(p);

    float a = hash12(i);
    float b = hash12(i + vec2(1.0, 0.0));
    float c = hash12(i + vec2(0.0, 1.0));
    float d = hash12(i + vec2(1.0, 1.0));

    vec2 u = f * f * (3.0 - 2.0 * f);

    return mix(mix(a, b, u.x),
               mix(c, d, u.x), u.y);
}

// fbm (2D)
float fbm(vec2 p)
{
    float value = 0.0;
    float amp   = 0.5;
    for (int i = 0; i < 5; i++)
    {
        value += amp * vnoise(p);
        p = mod(p * 2.0, 1024.0);
        amp *= 0.5;
    }
    return value;
}


//======================================================================
// ハッシュ / ノイズ（3D）
//======================================================================

// 3D ハッシュ
float hash13(vec3 p)
{
    p = fract(p * 0.1031);
    p += dot(p, p.yzx + 33.33);
    return fract((p.x + p.y) * p.z);
}

// 3D value noise
float vnoise3(vec3 p)
{
    vec3 i = floor(p);
    vec3 f = fract(p);

    float n000 = hash13(i + vec3(0.0, 0.0, 0.0));
    float n100 = hash13(i + vec3(1.0, 0.0, 0.0));
    float n010 = hash13(i + vec3(0.0, 1.0, 0.0));
    float n110 = hash13(i + vec3(1.0, 1.0, 0.0));
    float n001 = hash13(i + vec3(0.0, 0.0, 1.0));
    float n101 = hash13(i + vec3(1.0, 0.0, 1.0));
    float n011 = hash13(i + vec3(0.0, 1.0, 1.0));
    float n111 = hash13(i + vec3(1.0, 1.0, 1.0));

    vec3 u = f * f * (3.0 - 2.0 * f);

    float nx00 = mix(n000, n100, u.x);
    float nx10 = mix(n010, n110, u.x);
    float nx01 = mix(n001, n101, u.x);
    float nx11 = mix(n011, n111, u.x);

    float nxy0 = mix(nx00, nx10, u.y);
    float nxy1 = mix(nx01, nx11, u.y);

    return mix(nxy0, nxy1, u.z);
}

// fbm (3D) — 全天用（継ぎ目のないノイズ）
float fbm3(vec3 p)
{
    float value = 0.0;
    float amp   = 0.5;

    for (int i = 0; i < 5; i++)
    {
        value += amp * vnoise3(p);
        p *= 2.0;
        amp *= 0.5;
    }
    return value;
}


//======================================================================
// メイン
//======================================================================
void main()
{
    // 視線方向（スカイドーム上の方向）
    vec3 dir = normalize(vWorldDir);

    // vWorldDir.y を 0〜1 にマッピング（天頂方向ほど 1）
    float t = clamp(vWorldDir.y, 0.0, 1.0);

    // 天候による「晴れ感」フェード
    float weatherFade = (uWeatherType == 0) ? 1.0 : 0.3;

    //------------------------------------------------------------------
    // 昼夜ブレンド（uTimeOfDay から Day/Night Strength を算出）
    //------------------------------------------------------------------
    float dayStrength =
        smoothstep(0.15, 0.25, uTimeOfDay) *
        (1.0 - smoothstep(0.75, 0.85, uTimeOfDay));
    float nightStrength = 1.0 - dayStrength;

    //------------------------------------------------------------------
    // ベース空色＋雲色（天候に応じて若干くすませる）
    //------------------------------------------------------------------
    vec3 rawSky   = uRawSkyColor;
    vec3 rawCloud = uRawCloudColor;

    vec3 baseSky    = mix(vec3(0.4, 0.4, 0.5), rawSky,  weatherFade);
    vec3 cloudColor = mix(vec3(0.4, 0.4, 0.4), rawCloud, weatherFade);

    // 天頂方向は baseSky を強めに、地平線側はやや暗めに
    vec3 skyColor = mix(baseSky * 0.6, baseSky, t);

    float cloudAlpha = 0.0;


    //==================================================================
    // 雲ノイズ：天候タイプごとの密度・色補正
    //==================================================================
    {
        // dir を使った球面座標ベースの 3D Proj ノイズ
        vec3 p = dir * 7.0; // スケール調整（雲の大きさ）
        p.xz += vec2(uTime * 0.03, uTime * 0.01);   // 雲の移動

        float density = fbm3(p); // 雲密度

        if (uWeatherType == 0)         // CLEAR（快晴だが、少し雲を残す）
        {
            cloudAlpha = smoothstep(0.38, 0.70, density);
        }
        else if (uWeatherType == 1)    // CLOUDY（曇天）
        {
            cloudAlpha = smoothstep(0.20, 0.55, density);
            cloudColor = vec3(0.6);
            skyColor   = mix(skyColor, vec3(0.3), 0.6);
        }
        else if (uWeatherType == 2)    // RAIN（雨）
        {
            cloudAlpha = smoothstep(0.15, 0.45, density);
            skyColor  *= 0.35;
            cloudColor = vec3(0.5);
        }
        else if (uWeatherType == 3)    // STORM（嵐）
        {
            cloudAlpha = smoothstep(0.15, 0.45, density);
            skyColor   = vec3(0.12);
            cloudColor = vec3(0.7);
        }
        else if (uWeatherType == 4)    // SNOW（雪）
        {
            cloudAlpha = smoothstep(0.22, 0.50, density);
            skyColor   = vec3(0.30);
            cloudColor = vec3(0.9);
        }

        // 雨・嵐・雪では雲の占有率をさらに増やす
        if (uWeatherType >= 2)
        {
            cloudAlpha = min(cloudAlpha + 0.4, 1.0);
        }
    }

    //------------------------------------------------------------------
    // 雷（STORM 専用）
    //------------------------------------------------------------------
    if (uWeatherType == 3)
    {
        float flash = step(0.98, fract(sin(uTime * 12.0) * 43758.5453));
        skyColor += vec3(1.0) * flash * 0.8;
    }

    //------------------------------------------------------------------
    // 雪天（SNOW）：空色・雲のブレンド強化
    //------------------------------------------------------------------
    if (uWeatherType == 4)
    {
        skyColor   = mix(skyColor, cloudColor, 0.4);
        cloudAlpha = min(cloudAlpha + 0.2, 1.0);
    }

    //------------------------------------------------------------------
    // 雲と空の最終ベース合成
    //------------------------------------------------------------------
    vec3 finalColor = mix(skyColor, cloudColor, cloudAlpha * 0.8);


    //==================================================================
    // ★ 夜空の星（Clear のみ / 夜間のみ）
    //==================================================================
    if (nightStrength > 0.01 && uWeatherType == 0)
    {
        // 上方向ほど星が見えやすく（地平線側は控えめ）
        float up     = clamp(dir.y, 0.0, 1.0);
        float upFade = smoothstep(0.0, 0.4, up);

        // 星の散布パターン（dir を xz 平面に投影して粗いノイズ）
        vec2 starUV = dir.xz * 40.0;

        float seed = hash12(starUV);
        // 星のしきい値調整（値を下げると星が増える）
        float starMask = smoothstep(0.9985, 1.0, seed);

        // 雲が多いほど星は隠れる
        float cloudBlock = 1.0 - cloudAlpha;

        vec3 starColor = vec3(1.0, 0.97, 0.92);
        float starIntensity = starMask * nightStrength * upFade * cloudBlock;

        finalColor += starColor * starIntensity;
    }


    //==================================================================
    // ★ 月 (Moon) — 固定方向の簡易実装
    //==================================================================
    if (nightStrength > 0.01 && uWeatherType == 0)
    {
        // 月の見える方向
        vec3 moonDir = normalize(uMoonDir);

        float m = clamp(dot(dir, -moonDir), 0.0, 1.0);

        // 本体ディスク（中心部）
        float moonDisk = smoothstep(0.985, 1.0, m);

        // グローとハロ（外側のぼけ）
        float moonGlow = pow(m, 7680.0);
        float halo     = pow(m, 60.0);

        vec3 moonColor  = vec3(1.2, 1.15, 1.0);
        float cloudBlock = 1.0 - cloudAlpha;

        float moonIntensity =
            (moonDisk * 1.0 +
             moonGlow * 0.6 +
             halo     * 0.4) *
             nightStrength * cloudBlock;

        finalColor += moonColor * moonIntensity;
    }


    //==================================================================
    // ★ 天の川 (Milky Way) — 3D ノイズベース / 継ぎ目対策済み
    //==================================================================
    if (nightStrength > 0.01 && uWeatherType == 0)
    {
        // 天の川の帯の方向（dir が bandDir に近いと明るくなる）
        vec3 bandDir = normalize(vec3(0.5, -0.2, 0.0));

        float milky = abs(dot(dir, bandDir));
        // 帯の太さ（内側ほど 1.0 に近く）
        float band = smoothstep(0.5, 0.2, milky);

        // dir そのものを 3D ノイズに突っ込むことで継ぎ目を防ぐ
        float noise = fbm3(dir * 4.0 + vec3(0.0, uTime * 0.02, 0.0));

        float milkyMask = band * noise * nightStrength * (1.0 - cloudAlpha);

        vec3 milkyColor = vec3(0.75, 0.8, 1.0);
        milkyColor = mix(milkyColor, vec3(1.0, 0.8, 0.9), noise * 0.3);

        finalColor += milkyColor * milkyMask * 0.7;
    }


    //==================================================================
    // ★ 太陽ハイライト（サンディスク＆グロー）
    //   - Clear / Cloudy のみ
    //   - uTimeOfDay から日の出〜日没の可視時間を決めてフェード
    //==================================================================
    if (uWeatherType <= 1)
    {
        float hour = uTimeOfDay * 24.0;

        // 日の出・日の入りの時刻とフェード幅
        const float sunriseHour  = 5.0;
        const float sunsetHour   = 18.5;
        const float dawnSpanHour = 1.0;
        const float duskSpanHour = 1.5;

        float sunVisibility = 0.0;

        // 日の出前後：0→1 にフェードイン
        if (hour >= sunriseHour - dawnSpanHour && hour < sunriseHour + dawnSpanHour)
        {
            float tt = (hour - (sunriseHour - dawnSpanHour)) / (dawnSpanHour * 2.0);
            sunVisibility = clamp(tt, 0.0, 1.0);
        }
        // 日中：フル可視
        else if (hour >= sunriseHour + dawnSpanHour && hour <= sunsetHour - duskSpanHour)
        {
            sunVisibility = 1.0;
        }
        // 日の入り前後：1→0 にフェードアウト
        else if (hour > sunsetHour - duskSpanHour && hour <= sunsetHour + duskSpanHour)
        {
            float tt = (sunsetHour + duskSpanHour - hour) / (duskSpanHour * 2.0);
            sunVisibility = clamp(tt, 0.0, 1.0);
        }

        if (uWeatherType <= 1 && sunVisibility > 0.001)
        {
            // dir と太陽方向のなす角でディスク/ハロを作成
            float sunAmount = clamp(dot(dir, -normalize(uSunDir)), 0.0, 1.0);

            // コア（中心）とハロ部分の強度
            float sunCore = pow(sunAmount, 4096.0);
            float sunHalo = pow(sunAmount, 100.0);

            vec3 sunCoreColor = vec3(1.3, 1.1, 0.8);
            vec3 sunHaloColor = vec3(1.1, 0.9, 0.7);
            vec3 sunGlow = sunCoreColor * sunCore + sunHaloColor * sunHalo;

            // 雲・天候による減衰
            float cloudFactor = 1.0 - cloudAlpha;
            float weatherDim = (uWeatherType == 1) ? 0.6 : 1.0;

            sunGlow *= sunVisibility * weatherDim * cloudFactor;

            finalColor += sunGlow;
        }
    }

    //------------------------------------------------------------------
    // 最終出力
    //------------------------------------------------------------------
    FragColor = vec4(finalColor, 1.0);
}
