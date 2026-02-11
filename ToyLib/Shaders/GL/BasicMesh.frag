#version 410 core

//======================================================================
// BasicMesh.frag
// ・通常メッシュ用の基本フラグメントシェーダ
// ・Phong ライティング（Ambient + Diffuse + Specular）
// ・Toon でない通常メッシュ用
//======================================================================


//======================================================================
//  入力（Vertex Shader → Fragment Shader）
//======================================================================
in vec2 fragTexCoord;   // UV座標
in vec3 fragNormal;     // ワールド空間の法線
in vec3 fragWorldPos;   // ワールド空間の頂点座標


//-----------------------------------------------------------------------
// 出力
//-----------------------------------------------------------------------
out vec4 outColor;


//======================================================================
//  Uniforms
//======================================================================

// --------------------- テクスチャ ---------------------
uniform sampler2D uTexture;


// --------------------- ライティング ---------------------
struct DirectionalLight
{
    vec3 mDirection;      // 光の方向（ワールド空間・向き）
    vec3 mDiffuseColor;   // 拡散光
    vec3 mSpecColor;      // 鏡面反射光
};

// カメラ位置（鏡面反射に必要）
uniform vec3 uCameraPos;

// 環境光
uniform vec3 uAmbientLight;

// 鏡面反射指数（光沢）
uniform float uSpecPower;

// ディレクショナルライト
uniform DirectionalLight uDirLight;


//======================================================================
// main()
//======================================================================
void main()
{
    //----------------------------------------------
    // 基本ベクトル計算
    //----------------------------------------------
    vec3 N = normalize(fragNormal);                 // 法線
    vec3 L = normalize(-uDirLight.mDirection);      // 光方向（入射方向）
    vec3 V = normalize(uCameraPos - fragWorldPos);  // カメラ方向
    vec3 R = normalize(reflect(-L, N));             // 反射ベクトル


    //----------------------------------------------
    // ライティング（Phong）
    //----------------------------------------------

    // --- Ambient ---
    vec3 ambient = uAmbientLight;

    // --- Diffuse ---
    float diff = max(dot(N, L), 0.0);
    vec3 diffuse = diff * uDirLight.mDiffuseColor;

    // --- Specular ---
    float spec = 0.0;
    if (diff > 0.0) // 表面が光を受けているときだけ
    {
        spec = pow(max(dot(R, V), 0.0), uSpecPower);
    }
    vec3 specular = spec * uDirLight.mSpecColor;


    //----------------------------------------------
    // 最終ライティング合成
    //----------------------------------------------
    vec3 lighting = ambient + diffuse + specular;


    //----------------------------------------------
    // テクスチャ + ライティング
    //----------------------------------------------
    vec4 texColor = texture(uTexture, fragTexCoord);

    outColor = texColor * vec4(lighting, texColor.a);
}
