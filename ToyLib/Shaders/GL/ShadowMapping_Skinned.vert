#version 410 core

//======================================================================
// ToyLib Uniform Contract (v2) - ShadowSceneUBO + BonePalette
//   C++ mirror : Render/GL/GLShaderTypes.h  (GLShadowSceneUBO)
//   Binding    : kShadowUBOBinding=2 / kBonePaletteBinding=0
//======================================================================

struct ObjectData
{
    mat4 world;
};

// GL 4.1 は layout(binding=N) 不可 → C++ 側で glUniformBlockBinding 設定済み
// row_major: ToyLib は v*M（行ベクトル×行列）規約
layout(std140, row_major) uniform ShadowSceneUBO
{
    mat4 lightVP;
} uScene;

uniform ObjectData uObject;

// ボーンパレット UBO（binding=0）
// ★256 = GL_MAX_UNIFORM_BLOCK_SIZE の仕様保証最小値(16384byte)に収まる上限。
//   320本(20480byte)は環境依存でリンク失敗する（AMD環境で起動不能を確認）。
//   256本を超える大規模リグはVKバックエンド（SSBO）を使うこと。
const int kMaxPalette = 256;

layout(std140, row_major) uniform BonePalette
{
    mat4 matrixPalette[kMaxPalette];
};

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
          matrixPalette[inSkinBones[0]] * inSkinWeights[0]
        + matrixPalette[inSkinBones[1]] * inSkinWeights[1]
        + matrixPalette[inSkinBones[2]] * inSkinWeights[2]
        + matrixPalette[inSkinBones[3]] * inSkinWeights[3];

    // スキン変換（ToyLib は 行ベクトル × 行列 ）
    vec4 skinnedPos = pos * skinMat;

    // 2) モデル → ワールド変換
    skinnedPos = skinnedPos * uObject.world;

    // 3) ワールド → ライト空間変換（これが影マップ座標）
    gl_Position = skinnedPos * uScene.lightVP;

    // ※ 深度だけ使うのでフラグメント向け varyings は不要
}
