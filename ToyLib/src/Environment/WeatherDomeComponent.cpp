#include "Environment/WeatherDomeComponent.h"
#include "Environment/SkyDomeMeshGenerator.h"
#include "Asset/Geometry/VertexArray.h"
#include "Engine/Render/Renderer.h"
#include "Engine/Render/LightingManager.h"
#include "Engine/Core/Actor.h"
#include "Engine/Core/Application.h"
#include "Utils/MathUtil.h"
#include "Engine/Runtime/TimeOfDaySystem.h"
#include <algorithm>
#include <cmath>

namespace toy {

//======================================
// コンストラクタ
//  - スカイドームメッシュ作成
//  - Renderer への登録
//  - SkyDome シェーダ取得
//======================================
WeatherDomeComponent::WeatherDomeComponent(Actor* a)
: SkyDomeComponent(a)
, mTime(0.5f)                          // 1日(0〜1)中の現在時間
, mSunDir(Vector3::UnitY)              // 初期は真上
, mMoonDir(Vector3::NegUnitY) // 固定の月方向
, mWeatherType(WeatherType::CLEAR)     // 初期天気：快晴
{
    // 月方向は念のため正規化
    mMoonDir.Normalize();

    // 半球メッシュ（頂点/インデックスバッファ）を生成
    mSkyVAO = SkyDomeMeshGenerator::CreateSkyDomeVAO(32, 16, 1.0f);
    
    // Renderer に「スカイドーム描画対象」として登録
    GetOwner()->GetApp()->GetRenderer()->RegisterSkyDome(this);
    
    // 天空描画用シェーダ（GLSL）の取得
    mShader = GetOwner()->GetApp()->GetRenderer()->GetShader("SkyDome");
}

//======================================
// 時間設定（0〜1）
//  1日を 0.0〜1.0 で表現しているので fmod でループ
//======================================
void WeatherDomeComponent::SetTime(float t)
{
    mTime = fmod(t, 1.0f);
}

//======================================
// 太陽方向の外部設定
//  ※ 通常は ApplyTime() で内部計算するが、外から上書きも可能
//======================================
void WeatherDomeComponent::SetSunDirection(const Vector3& dir)
{
    mSunDir = dir;
}

// （必要ならヘッダに宣言して使う）
// void WeatherDomeComponent::SetMoonDirection(const Vector3& dir)
// {
//     mMoonDir = dir;
//     mMoonDir.Normalize();
// }

//======================================
// スカイドーム描画
//  - カメラ位置を中心に巨大な半球を描画
//  - 時間・天候などのパラメータをシェーダに渡す
//======================================
void WeatherDomeComponent::Draw()
{
    if (!mSkyVAO || !mShader) return;
    
    // カメラの逆行列からワールド座標での位置を取得
    Matrix4 invView = GetOwner()->GetApp()->GetRenderer()->GetInvViewMatrix();
    
    // スカイドームの中心をカメラ位置
    Vector3 camPos = invView.GetTranslation();
    
    // 大きな半球として描画（スケール200）
    Matrix4 model = Matrix4::CreateScale(200.0f) * Matrix4::CreateTranslation(camPos);
    Matrix4 view  = GetOwner()->GetApp()->GetRenderer()->GetViewMatrix();
    Matrix4 proj  = GetOwner()->GetApp()->GetRenderer()->GetProjectionMatrix();
    Matrix4 mvp   = model * view * proj;
    
    // シェーダ有効化
    mShader->SetActive();
    mShader->SetMatrixUniform("uMVP", mvp);
    
    // 雲のアニメーション用時間（60秒で0〜1を1周）
    float t = fmod(SDL_GetTicks() / 1000.0f, 60.0f) / 60.0f;
    mShader->SetFloatUniform("uTime", t);
    
    // 天候タイプ（GLSL側では int で受け取る）
    mShader->SetIntUniform("uWeatherType", static_cast<int>(mWeatherType));
    
    // 1日を0.0〜1.0で表現した時間帯（朝/昼/夕/夜のベース）
    mShader->SetFloatUniform("uTimeOfDay", fmod(mTime, 1.0f));
    
    // 太陽・月の方向（ライティング＆レイマーチ等で使用）
    mShader->SetVectorUniform("uSunDir",  mSunDir);
    mShader->SetVectorUniform("uMoonDir", mMoonDir);   // ★ 追加：月方向を送る
    
    // CPU側で計算した生の空色・雲色（GLSLでの補正のベース）
    mShader->SetVectorUniform("uRawSkyColor",   mRawSkyColor);
    mShader->SetVectorUniform("uRawCloudColor", mRawCloudColor);
    
    // 背景なのでカリング/深度書き込みを一時的に無効化して描画
    glDisable(GL_CULL_FACE);
    glDepthMask(GL_FALSE); // 背景なので Z 書き込み不要
    mSkyVAO->SetActive();
    glDrawElements(GL_TRIANGLES, mSkyVAO->GetNumIndices(), GL_UNSIGNED_INT, nullptr);
    glDepthMask(GL_TRUE);
    glEnable(GL_CULL_FACE);
}

//======================================
// SmoothStep
//  - 標準的な smoothstep(edge0, edge1, x)
//  - 時間帯の遷移を滑らかにするために使用
//======================================
float WeatherDomeComponent::SmoothStep(float edge0, float edge1, float x)
{
    // まず 0〜1 に正規化してから 3次補間
    float t = std::clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

//======================================
// Update
//  - TimeOfDaySystem から現在時刻(0〜24)を取得
//  - mTime (0〜1) に変換して ApplyTime() で各種更新
//======================================
void WeatherDomeComponent::Update(float deltaTime)
{
    auto timeSystem = GetOwner()->GetApp()->GetTimeOfDaySystem();
    
    float hour = timeSystem->GetHourFloat();    // 0〜24
    float t    = hour / 24.f;                   // 0〜1に正規化
    mTime      = t;
    
    // 時間・天候に応じて太陽方向や空色・フォグなどを更新
    ApplyTime();
}

//======================================
// GetSkyColor
//  - 1日(0〜1)の中で「夜→朝焼け→昼→夕焼け→夜」を滑らかに補間
//  - さらに天候に応じて彩度/明度を調整
//======================================
Vector3 WeatherDomeComponent::GetSkyColor(float time)
{
    // 各時間帯の代表色
    Vector3 night(0.02f, 0.03f, 0.08f);
    Vector3 day  (0.55f, 0.75f, 1.00f);   // ちょい落ち着いた昼空
    Vector3 dusk (0.95f, 0.55f, 0.35f);   // オレンジ寄り夕焼け
    
    // time: 0.0〜1.0 を想定（0:00〜24:00）
    time = fmodf(time, 1.0f);
    if (time < 0.0f) time += 1.0f;
    
    // 日の出/日の入り（時間単位）
    const float sunriseHour  = 6.0f;   // 5:00
    const float sunsetHour   = 18.0f;  // 18:00
    const float dawnSpanHour = 1.0f;   // 日の出前後1時間をグラデーション
    const float duskSpanHour = 1.0f;   // 日の入り前後1時間をグラデーション
    
    // 0〜1 に変換
    const float sunriseT   = sunriseHour   / 24.0f;
    const float sunsetT    = sunsetHour    / 24.0f;
    const float dawnSpanT  = dawnSpanHour  / 24.0f;
    const float duskSpanT  = duskSpanHour  / 24.0f;
    
    // 区間境界
    const float tNightEnd1    = sunriseT - dawnSpanT;   // 夜 → 朝焼け開始
    const float tDawnMid      = sunriseT;               // 夜→dusk の切れ目
    const float tDawnEnd      = sunriseT + dawnSpanT;   // 朝焼け → 昼
    const float tDuskStart    = sunsetT - duskSpanT;    // 昼 → 夕焼け開始
    const float tDuskMid      = sunsetT;                // 昼→dusk の切れ目
    const float tNightStart2  = sunsetT + duskSpanT;    // 夕焼け → 夜
    
    // 0〜1 の smoothstep を簡略に書くラムダ
    auto smooth01 = [](float x)
    {
        x = Math::Clamp(x, 0.0f, 1.0f);
        return x * x * (3.0f - 2.0f * x); // smoothstep(0,1,x)
    };
    
    Vector3 col;
    
    // --- 夜（日の入り後〜日の出前） ---
    if (time < tNightEnd1 || time >= tNightStart2)
    {
        col = night;
    }
    // --- 朝焼け：夜色 → 夕焼け色 ---
    else if (time < tDawnMid)
    {
        float u = (time - tNightEnd1) / (tDawnMid - tNightEnd1);
        u = smooth01(u);
        col = Vector3::Lerp(night, dusk, u);
    }
    // --- 朝焼け：夕焼け色 → 昼空 ---
    else if (time < tDawnEnd)
    {
        float u = (time - tDawnMid) / (tDawnEnd - tDawnMid);
        u = smooth01(u);
        col = Vector3::Lerp(dusk, day, u);
    }
    // --- 昼 ---
    else if (time < tDuskStart)
    {
        // 正午付近は少しだけ明るさを上げる
        float dayMid = (tDawnEnd + tDuskStart) * 0.5f;
        float w = 1.0f - fabsf(time - dayMid) / (tDuskStart - tDawnEnd);
        w = Math::Clamp(w, 0.0f, 1.0f);
        float bright = 0.9f + 0.1f * smooth01(w); // 正午付近で少し明るく
        col = day * bright;
    }
    // --- 夕焼け：昼空 → 夕焼け空 ---
    else if (time < tDuskMid)
    {
        float u = (time - tDuskStart) / (tDuskMid - tDuskStart);
        u = smooth01(u);
        col = Vector3::Lerp(day, dusk, u);
    }
    // --- 夕焼け：夕焼け空 → 夜空 ---
    else // time < tNightStart2 が保証されている
    {
        float u = (time - tDuskMid) / (tNightStart2 - tDuskMid);
        u = smooth01(u);
        col = Vector3::Lerp(dusk, night, u);
    }
    
    //========================================
    // 晴れ以外の補正（曇天・雨天など）
    //  - 空色を曇天グレーに強めに寄せる
    //  - 彩度もかなり落として「色味の少ない空」にする
    //========================================
    if (mWeatherType != WeatherType::CLEAR)
    {
        // 明るめの典型的な曇天色（少し青寄りのグレー）
        Vector3 overcast(0.65f, 0.68f, 0.72f);
        
        // まず時間帯ベースの空色を、かなり強めに曇天に寄せる
        float overcastStrength = 0.75f; // 思い切って 0.75 くらい
        col = Vector3::Lerp(col, overcast, overcastStrength);
        
        // さらに彩度をガッツリ落として「空の色味」を抑える
        float g = (col.x + col.y + col.z) / 3.0f;
        Vector3 gray(g, g, g);
        // 0.0:元の色, 1.0:完全グレー
        float desat = 0.6f; // 60% くらい無彩色寄りにする
        col = Vector3::Lerp(col, gray, desat);
    }
    
    return col;
}

//======================================
// GetCloudColor
//  - 雲レイヤー用の色を時間帯（夜・朝焼け・昼・夕焼け）で補間
//  - 「夜＋悪天候」のときはさらに暗さ＆彩度を落とす
//======================================
Vector3 WeatherDomeComponent::GetCloudColor(float time)
{
    // ========= ここは今までの時間ベースの処理 =========
    Vector3 dayColor   (0.95f, 0.98f, 1.00f);
    Vector3 duskColor  (1.00f, 0.75f, 0.55f);
    Vector3 nightColor (0.16f, 0.18f, 0.26f);
    
    time = fmodf(time, 1.0f);
    if (time < 0.0f) time += 1.0f;
    
    const float sunriseHour  = 5.0f;
    const float sunsetHour   = 18.0f;
    const float dawnSpanHour = 1.0f;
    const float duskSpanHour = 1.0f;
    
    const float sunriseT   = sunriseHour   / 24.0f;
    const float sunsetT    = sunsetHour    / 24.0f;
    const float dawnSpanT  = dawnSpanHour  / 24.0f;
    const float duskSpanT  = duskSpanHour  / 24.0f;
    
    const float tNightEnd1    = sunriseT - dawnSpanT;
    const float tDawnMid      = sunriseT;
    const float tDawnEnd      = sunriseT + dawnSpanT;
    const float tDuskStart    = sunsetT - duskSpanT;
    const float tDuskMid      = sunsetT;
    const float tNightStart2  = sunsetT + duskSpanT;
    
    auto smooth01 = [](float x)
    {
        x = Math::Clamp(x, 0.0f, 1.0f);
        return x * x * (3.0f - 2.0f * x);
    };
    
    Vector3 col;
    
    // 時間帯ごとに雲色を Lerp
    if (time < tNightEnd1 || time >= tNightStart2)
    {
        col = nightColor;
    }
    else if (time < tDawnMid)
    {
        float u = (time - tNightEnd1) / (tDawnMid - tNightEnd1);
        u = smooth01(u);
        col = Vector3::Lerp(nightColor, duskColor, u);
    }
    else if (time < tDawnEnd)
    {
        float u = (time - tDawnMid) / (tDawnEnd - tDawnMid);
        u = smooth01(u);
        col = Vector3::Lerp(duskColor, dayColor, u);
    }
    else if (time < tDuskStart)
    {
        col = dayColor;
    }
    else if (time < tDuskMid)
    {
        float u = (time - tDuskStart) / (tDuskMid - tDuskStart);
        u = smooth01(u);
        col = Vector3::Lerp(dayColor, duskColor, u);
    }
    else
    {
        float u = (time - tDuskMid) / (tNightStart2 - tDuskMid);
        u = smooth01(u);
        col = Vector3::Lerp(duskColor, nightColor, u);
    }
    
    // ========= ここから「夜＋悪天候」の暗さ補正 =========
    
    // 「どれくらい夜か」を 0〜1 で出す
    float nightFactor = 0.0f;
    if (time < tNightEnd1)
    {
        // 深夜〜夜明け前：time=0〜tNightEnd1 を 1→0 にマップ
        nightFactor = 1.0f - (time / tNightEnd1);
    }
    else if (time >= tNightStart2)
    {
        // 夕焼け後〜深夜：tNightStart2〜1 を 0→1 にマップ
        nightFactor = (time - tNightStart2) / (1.0f - tNightStart2);
    }
    nightFactor = Math::Clamp(nightFactor, 0.0f, 1.0f);
    
    // 対象の天気：曇り・雨・嵐・雪
    bool badWeather =
        (mWeatherType == WeatherType::CLOUDY) ||
        (mWeatherType == WeatherType::RAIN)   ||
        (mWeatherType == WeatherType::STORM)  ||
        (mWeatherType == WeatherType::SNOW);
    
    if (badWeather && nightFactor > 0.0f)
    {
        // ベースの「暗い夜雲」のターゲット色（少し青い暗グレー）
        Vector3 darkTarget(0.08f, 0.09f, 0.12f);
        
        // 天気ごとに「どれくらい暗くするか」の係数を変える
        float darkStrength = 0.4f; // CLOUDY の基準
        switch (mWeatherType)
        {
            case WeatherType::CLOUDY: darkStrength = 0.6f; break; // 曇りはちょい暗め
            case WeatherType::RAIN:   darkStrength = 0.8f; break; // 雨はもう少し暗い
            case WeatherType::STORM:  darkStrength = 0.9f; break; // 嵐はかなり暗く
            case WeatherType::SNOW:   darkStrength = 0.5f; break; // 雪は暗いけど少し明るさ残す
            default: break;
        }
        
        float w = nightFactor * darkStrength;
        
        // 色そのものを暗いターゲットに寄せる
        col = Vector3::Lerp(col, darkTarget, w);
        
        // さらに彩度も少し落とすと「夜の空気感」が出る
        float g = (col.x + col.y + col.z) / 3.0f;
        Vector3 gray(g, g, g);
        float desat = 0.4f * nightFactor; // 夜が深いほど無彩色寄りに
        col = Vector3::Lerp(col, gray, desat);
    }
    
    return col;
}

//======================================
// ApplyTime
//  - mTime(0〜1)から：
//    ・太陽方向
//    ・空色 / 雲色
//    ・直射光カラー＆強度
//    ・アンビエントカラー
//    ・フォグ色＆密度
//    を一括で更新
//======================================
void WeatherDomeComponent::ApplyTime()
{
    float timeOfDay = fmod(mTime, 1.0f);
    
    //==============================================================
    // 太陽・月の方向を timeOfDay(0〜1) から計算
    //==============================================================
    // 太陽：timeOfDay(0〜1) を 1周(2π)の角度に変換
    float sunAngle  = Math::TwoPi * (timeOfDay - 0.25f);
    // 月：太陽から 180°（π）ずらす＝常に反対側の空を移動
    float moonAngle = sunAngle + Math::Pi;
    
    // 太陽の軌道パラメータ
    const float sunElevationScale  = 0.6f;   // 高度の振れ幅
    const float sunVerticalOffset  = -0.1f;  // 全体のオフセット
    const float sunSideOffset      = 0.4f;   // 南寄せ
    
    // 月の軌道パラメータ（少し高さやオフセットを変えてもOK）
    const float moonElevationScale = 0.5f;   // 太陽よりちょい低め
    const float moonVerticalOffset = 0.05f;  // 少しだけ持ち上げる
    const float moonSideOffset     = 0.4f;   // 同じく南寄せ
    
    // 太陽方向
    mSunDir = Vector3(
        -cosf(sunAngle),
        -sinf(sunAngle) * sunElevationScale + sunVerticalOffset,
        sunSideOffset * sinf(sunAngle)
    );
    mSunDir.Normalize();
    
    // 月方向（太陽の反対側を回る）
    mMoonDir = Vector3(
        -cosf(moonAngle),
        -sinf(moonAngle) * moonElevationScale + moonVerticalOffset,
        moonSideOffset * sinf(moonAngle)
    );
    mMoonDir.Normalize();
    
    //==============================================================
    // 空色・雲色
    //==============================================================
    mRawSkyColor   = GetSkyColor(timeOfDay);
    mRawCloudColor = GetCloudColor(timeOfDay);
    
    //==============================================================
    // 天気による全体減衰（明るさスケール）
    //==============================================================
    float weatherDim = 1.0f;
    switch (mWeatherType)
    {
        case WeatherType::CLEAR:  weatherDim = 1.0f; break;
        case WeatherType::CLOUDY: weatherDim = 0.7f; break;
        case WeatherType::RAIN:   weatherDim = 0.5f; break;
        case WeatherType::STORM:  weatherDim = 0.3f; break;
        case WeatherType::SNOW:   weatherDim = 0.6f; break;
    }
    
    //==============================================================
    // 昼夜の強さ（0〜1）
    //  0.15〜0.25 → 朝に向けて昼成分を立ち上げ
    //  0.75〜0.85 → 夜に向けて昼成分を落とす
    //==============================================================
    float dayStrength =
        SmoothStep(0.15f, 0.25f, timeOfDay) *
        (1.0f - SmoothStep(0.75f, 0.85f, timeOfDay));
    float nightStrength = 1.0f - dayStrength;
    
    //==============================================================
    // ライティングマネージャ更新
    //==============================================================
    if (mLightingManager)
    {
        // 太陽の強度（今はシャドウとは無関係。必要なら別用途に）
        mLightingManager->SetSunIntensity(dayStrength);
        
        // 昼：太陽の方向、夜：月の方向（ディレクショナルライトの向き）
        Vector3 lightDir;
        if (dayStrength > 0.01f)
        {
            lightDir = Vector3(-mSunDir.x, -mSunDir.y, -mSunDir.z);
        }
        else
        {
            lightDir = Vector3(-mMoonDir.x, -mMoonDir.y, -mMoonDir.z);
        }
        mLightingManager->SetLightDirection(lightDir, Vector3::Zero);
        
        // ライト色（太陽＋月）
        Vector3 sunColor  = Vector3(1.0f, 0.95f, 0.8f);
        Vector3 moonColor = Vector3(0.25f, 0.35f, 0.5f);
        Vector3 finalLightColor =
            (sunColor * dayStrength + moonColor * nightStrength) * weatherDim;
        mLightingManager->SetLightDiffuseColor(finalLightColor);
        
        // アンビエント色
        Vector3 dayAmbient   = Vector3(0.7f, 0.7f, 0.7f);
        Vector3 nightAmbient = Vector3(0.25f, 0.2f, 0.3f);
        Vector3 finalAmbient =
            (dayAmbient * dayStrength + nightAmbient * nightStrength) * weatherDim;
        mLightingManager->SetAmbientColor(finalAmbient);
    }
    
    //==============================================================
    // フォグ色＋密度
    //==============================================================
    ComputeFogFromSky(timeOfDay);
}
//======================================
// ComputeFogFromSky
//  - mRawSkyColor / mRawCloudColor / WeatherType / timeOfDay
//    からフォグ色 & フォグ濃度を決定
//  - 地表近くの色を意識した、ややグレー寄りの色味
//======================================
void WeatherDomeComponent::ComputeFogFromSky(float timeOfDay)
{
    // 曇り・雨ベースの少し青みがかったグレー
    Vector3 overcastBase(0.4f, 0.4f, 0.5f);
    
    // 晴れ：1.0 で mRawSkyColor そのまま
    // 曇り：少しだけ夕焼けを混ぜる
    // 雨・嵐・雪：夕焼けは混ぜない
    float weatherFade = 1.0f;
    switch (mWeatherType)
    {
        case WeatherType::CLEAR:
            weatherFade = 1.0f;
            break;
        case WeatherType::CLOUDY:
            weatherFade = 0.25f;    // 曇りはほんのり夕焼け
            break;
        case WeatherType::RAIN:
        case WeatherType::STORM:
        case WeatherType::SNOW:
            weatherFade = 0.0f;     // 夕焼けは一切混ぜない
            break;
    }
    
    // 晴れ：mRawSkyColor ベース
    // 曇り：overcastBase 75% + mRawSkyColor 25%
    // 雨/嵐/雪：overcastBase 100%
    Vector3 baseSky = Vector3::Lerp(overcastBase, mRawSkyColor, weatherFade);
    
    // 地平線寄り（少し暗め）の空色
    float t = 0.1f;
    Vector3 skyHorizon = Vector3::Lerp(baseSky * 0.6f, baseSky, t);
    
    Vector3 cloudColor = mRawCloudColor;
    float cloudMix = 0.0f;
    
    // 天候ごとにフォグの濃度と、雲色との混ざり具合を調整
    switch (mWeatherType)
    {
        case WeatherType::CLEAR:
            cloudMix    = 0.2f;
            mFogDensity = 0.002f;
            break;
            
        case WeatherType::CLOUDY:
            cloudMix    = 0.5f;
            mFogDensity = 0.004f;
            // 曇りは少しだけ暗くして、夕焼け＋グレーの中間くらい
            skyHorizon = Vector3::Lerp(skyHorizon, Vector3(0.3f, 0.3f, 0.32f), 0.4f);
            break;
            
        case WeatherType::RAIN:
            cloudMix    = 0.7f;
            mFogDensity = 0.008f;
            // 完全に無彩色のグレー（r=g=b）
            skyHorizon  = Vector3(0.20f, 0.20f, 0.21f);
            cloudColor  = skyHorizon; // fog が完全グレーになるよう揃える
            break;
            
        case WeatherType::STORM:
            cloudMix    = 0.9f;
            mFogDensity = 0.015f;
            // かなり暗いグレー
            skyHorizon  = Vector3(0.12f, 0.12f, 0.13f);
            cloudColor  = skyHorizon;
            break;
            
        case WeatherType::SNOW:
            cloudMix    = 0.7f;
            mFogDensity = 0.010f;
            // 明るいグレー（少しだけ白寄りだけど無彩色）
            skyHorizon  = Vector3(0.80f, 0.80f, 0.82f);
            cloudColor  = skyHorizon;
            break;
    }
    
    // 地平線色と雲色の中間をフォグ色とする
    Vector3 fog = Vector3::Lerp(skyHorizon, cloudColor, cloudMix);
    
    // 夜は少し暗めにする
    float nightFactor = 0.0f;
    if (timeOfDay < 0.25f)
    {
        nightFactor = (0.25f - timeOfDay) / 0.25f;
    }
    else if (timeOfDay > 0.75f)
    {
        nightFactor = (timeOfDay - 0.75f) / 0.25f;
    }
    
    fog *= (1.0f - 0.6f * nightFactor);
    
    mFogColor = fog;
    
    // ライティング側へフォグ色を通知
    if (mLightingManager)
    {
        mLightingManager->SetFogColor(mFogColor);
    }
}

} // namespace toy
