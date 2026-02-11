#version 410 core

//======================================================================
//  ShadowMapping_Skinned.vert
//
//  スキンメッシュ専用のシャドウマッピング用頂点シェーダ。
//  ・アニメーションのスキニング（ボーン変換）
//  ・ワールド変換
//  ・ライト空間（LightViewProj）への変換
//
//  ※色情報・法線・UV は深度パスでは使用しないため不要。
//======================================================================

// ---------------------------------------------------------
// Uniforms
// ---------------------------------------------------------

// ボーン変換行列パレット（最大96ボーン）
uniform mat4 uMatrixPalette[96];

// モデル → ワールド変換
uniform mat4 uWorldTransform;

// ワールド → ライト空間変換（LightProj * LightView）
uniform mat4 uLightSpaceMatrix;


// ---------------------------------------------------------
// 頂点属性（頂点バッファ）
// ---------------------------------------------------------
layout(location = 0) in vec3 inPosition;     // 頂点位置
layout(location = 3) in uvec4 inSkinBones;   // 影響ボーンID（4つ）
layout(location = 4) in vec4  inSkinWeights; // ボーンウエイト（4つ）


// ---------------------------------------------------------
// メインシェーダ
// ---------------------------------------------------------
void main()
{
    // 1) スキニング処理
    vec4 pos = vec4(inPosition, 1.0);

    // 4ボーンの線形合成
    mat4 skinMat =
          uMatrixPalette[inSkinBones[0]] * inSkinWeights[0]
        + uMatrixPalette[inSkinBones[1]] * inSkinWeights[1]
        + uMatrixPalette[inSkinBones[2]] * inSkinWeights[2]
        + uMatrixPalette[inSkinBones[3]] * inSkinWeights[3];

    // スキン変換（ToyLib は 行ベクトル × 行列 ）
    vec4 skinnedPos = pos * skinMat;

    // 2) モデル → ワールド変換
    skinnedPos = skinnedPos * uWorldTransform;

    // 3) ワールド → ライト空間変換（これが影マップ座標）
    gl_Position = skinnedPos * uLightSpaceMatrix;

    // ※ 深度だけ使うのでフラグメント向け varyings は不要
}
