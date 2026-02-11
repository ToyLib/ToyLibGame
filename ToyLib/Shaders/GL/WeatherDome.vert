#version 410 core

//======================================================================
// WeatherDome.vert
//
// ・スカイドーム用の頂点シェーダ
// ・aPosition は **単位球上の頂点座標**（中心原点の sky dome）
// ・そのままの方向ベクトルが「空のピクセル方向」になるため、
//   vWorldDir としてフラグメントシェーダに送る。
//   → 雲ノイズ、天の川、星などはこの方向ベクトルで計算する
//
//   uMVP = Projection * View * Model
//   → WeatherDome は常にカメラ中心で描画される想定なので、
//     モデル行列は拡大縮小のみ、位置は (0,0,0)
//======================================================================

// 頂点入力：スカイドームメッシュの位置（単位球）
layout (location = 0) in vec3 aPosition;

// MVP 行列
uniform mat4 uMVP;

// フラグメントシェーダへ送る：方向ベクトル（vWorldDir）
out vec3 vWorldDir;

void main()
{
    // aPosition はスカイドームのローカル座標（＝方向）
    // スカイドームは常にカメラ中心なので transform 不要
    vWorldDir = normalize(aPosition);

    // 標準の MVP で位置をクリップ空間へ
    gl_Position = vec4(aPosition, 1.0) * uMVP;
}
