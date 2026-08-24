#version 410 core

//======================================================================
// ToyLib Uniform Contract (v2) - ShadowSceneUBO
//   C++ mirror : Render/GL/GLShaderTypes.h  (GLShadowSceneUBO)
//   Binding    : Render/GL/GLBindingPoints.h (kShadowUBOBinding = 2)
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

//======================================================================
//  ShadowMapping_Mesh.vert
//  （メッシュ専用：スキニングなし）
//
//  ライト視点の深度マップ作成パス。
//  ライト視点の座標系（LightSpaceMatrix = Projection * View）に
//  頂点を変換し、gl_Position に書き込むだけ。
//
//  このパスでは色情報を扱わず、深度値（gl_FragDepth）だけを使用。
//  フラグメントシェーダーは空で OK。
//======================================================================

// === 頂点属性 ===
// メッシュは深度パスでは位置のみ使用する
layout(location = 0) in vec3 inPosition;

void main()
{
    // ワールド変換 → ライト空間変換
    // gl_Position にライト空間座標を設定
    gl_Position = vec4(inPosition, 1.0) * uObject.world * uScene.lightVP;

    // ※ 注意 ※
    // 深度マップでは gl_FragDepth が自動で書き込まれるため、
    // フラグメントに値を渡す必要はない。
}
