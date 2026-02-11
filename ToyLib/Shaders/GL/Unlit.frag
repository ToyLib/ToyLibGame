//======================================================================
//  Unlit.frag
//  - テクスチャ色をそのまま出す（ライト/フォグ/影は無視）
//  - ただし FootSprite/ShadowSprite 用に Tint/Alpha を追加
//  - 互換維持：uUseTint==0 のときは従来通り “テクスチャそのまま”
//
//  使い分け：
//   - TextBillboard: uUseTint を触らない（=0扱いでそのまま表示）
//   - FootSprite系 : uUseTint=1 を必ずセットして色/透明度を制御
//======================================================================
#version 410 core

// ===== Phong/Mesh と共通（互換）=====
uniform sampler2D uTexture;

// 互換用：LightingManager がセットする可能性があるので宣言だけ置く
uniform vec3  uCameraPos;
uniform float uSpecPower;
uniform vec3  uAmbientLight;
struct FogInfo { float maxDist; float minDist; vec3 color; };
uniform FogInfo uFoginfo;

// ===== Unlit 拡張 =====
// ※ int を推奨（bool は環境で扱いが面倒なことがある）
uniform int   uUseTint;       // 0: 何もしない（従来互換） / 1: tint/alpha を適用
uniform int   uUseTexture;    // 0: uDiffuseColor を使う / 1: uTexture を使う
uniform vec3  uTint;          // 乗算色（デフォルト: 1,1,1）
uniform float uAlpha;         // 乗算アルファ（デフォルト: 1）
uniform vec3  uDiffuseColor;  // テクスチャ無しのときの色（デフォルト: 1,1,1）

in vec2 fragTexCoord;
out vec4 outColor;

void main()
{
    // ---- base color（互換のためまずはテクスチャ）----
    vec4 base = texture(uTexture, fragTexCoord);

    // テクスチャ無し運用（FootSpriteの円形マスクを色だけで出したい等）
    if (uUseTexture == 0)
    {
        base = vec4(uDiffuseColor, 1.0);
    }

    // 互換モード：TextBillboard等は “そのまま” 出す
    if (uUseTint == 0)
    {
        outColor = base;
        return;
    }

    // 拡張：Tint + Alpha
    base.rgb *= uTint;
    base.a   *= uAlpha;

    outColor = base;
}
