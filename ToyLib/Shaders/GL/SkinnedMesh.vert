#version 410 core

//======================================================================
//  SkinnedMesh.vert
//
//  スキンメッシュ用のメイン頂点シェーダ。
//  ・ボーンパレットを使ったスキニング
//  ・ワールド変換
//  ・ビュー射影変換（カメラ空間→クリップ空間）
//  ・ライト空間座標の出力（シャドウマッピング用）
//
//  ※ ToyLib は「行ベクトル × 行列 (v * M)」で統一。
//======================================================================

// ---------------------------------------------------------
// Uniforms
// ---------------------------------------------------------

// モデル → ワールド
uniform mat4 uWorldTransform;

// ワールド → クリップ（ビューProj）
uniform mat4 uViewProj;

// スキニング用ボーン行列パレット
uniform mat4 uMatrixPalette[96];

// ワールド → ライト空間（LightProj * LightView）
uniform mat4 uLightSpaceMatrix;


// ---------------------------------------------------------
// Attributes（頂点属性）
// ---------------------------------------------------------
layout(location = 0) in vec3 inPosition;    // 頂点位置
layout(location = 1) in vec3 inNormal;      // 法線
layout(location = 2) in vec2 inTexCoord;    // UV
layout(location = 3) in uvec4 inSkinBones;  // 影響ボーンID（最大4本）
layout(location = 4) in vec4  inSkinWeights;// ボーンウェイト


// ---------------------------------------------------------
// Varyings（フラグメントシェーダへ渡す値）
// ---------------------------------------------------------
out vec2 fragTexCoord;       // UV
out vec3 fragNormal;         // ワールド空間の法線
out vec3 fragWorldPos;       // ワールド座標
out vec4 fragPosLightSpace;  // ライト空間座標（シャドウマップ用）


// ---------------------------------------------------------
// メイン
// ---------------------------------------------------------
void main()
{
    // 1) 入力位置を vec4 に拡張
    vec4 pos = vec4(inPosition, 1.0);

    // 2) スキニング行列を作成（ボーン4本分の線形結合）
    mat4 skinMat =
          uMatrixPalette[inSkinBones[0]] * inSkinWeights[0]
        + uMatrixPalette[inSkinBones[1]] * inSkinWeights[1]
        + uMatrixPalette[inSkinBones[2]] * inSkinWeights[2]
        + uMatrixPalette[inSkinBones[3]] * inSkinWeights[3];

    // 3) 頂点位置のスキニング（ToyLib は v * M）
    vec4 skinnedPos = pos * skinMat;

    // 4) モデル → ワールド
    skinnedPos = skinnedPos * uWorldTransform;
    fragWorldPos = skinnedPos.xyz;

    // 5) ワールド → クリップ（ビュー射影）
    gl_Position = skinnedPos * uViewProj;

    // 6) 法線のスキニング＆ワールド変換
    //    ※ 法線は w = 0 として扱う
    vec4 n = vec4(inNormal, 0.0);
    n = n * skinMat;             // スキニング
    n = n * uWorldTransform;     // ワールド変換
    fragNormal = normalize(n.xyz);

    // 7) UV をそのまま転送
    fragTexCoord = inTexCoord;

}

