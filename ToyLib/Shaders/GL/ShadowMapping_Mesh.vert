#version 410 core

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

// === Uniforms ===
// モデル → ワールド変換
uniform mat4 uWorldTransform;
// ワールド → ライト空間変換（LightProj * LightView）
uniform mat4 uLightSpaceMatrix;

// === 頂点属性 ===
// メッシュは深度パスでは位置のみ使用する
layout(location = 0) in vec3 inPosition;

void main()
{
    // ワールド変換 → ライト空間変換
    // gl_Position にライト空間座標を設定
    gl_Position = vec4(inPosition, 1.0) * uWorldTransform * uLightSpaceMatrix;

    // ※ 注意 ※
    // 深度マップでは gl_FragDepth が自動で書き込まれるため、
    // フラグメントに値を渡す必要はない。
}
