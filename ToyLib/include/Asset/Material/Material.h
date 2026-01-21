#pragma once
#include "Utils/MathUtil.h"
#include <memory>

namespace toy {

//======================================================
// Material
//  - シェーダへ送るマテリアル情報を保持
//  - Texture, 色, Shininess などを統合
//  - Renderer 側で Shader と組み合わせて使用される
//======================================================
class Material
{
public:
    Material();

    // 指定シェーダへマテリアル情報をバインドする
    //   textureUnit … DiffuseMap を貼るスロット番号（通常 0）
    void BindToShader(std::shared_ptr<class Shader> shader,
                      int textureUnit = 0) const;

    //--- テクスチャ関連 ------------------------------------
    void SetDiffuseMap(std::shared_ptr<class Texture> tex)
    {
        mDiffuseMap = tex;
    }

    //--- 光沢（スペキュラー強度） ---------------------------
    void SetSpecPower(float power) { mShininess = power; }

    //--- カラー設定 -----------------------------------------
    // Diffuse/Specular/Ambient など通常の PBR で使う値
    void SetDiffuseColor(const Vector3& color)  { mDiffuseColor  = color; }
    void SetSpecularColor(const Vector3& color) { mSpecularColor = color; }
    void SetAmbientColor(const Vector3& color)  { mAmbientColor  = color; }

    // DiffuseMap を無視して単色で描画したいときに使用
    void SetOverrideColor(bool enable, const Vector3& color);

private:
    //--- 基本テクスチャ -------------------------------------
    std::shared_ptr<class Texture> mDiffuseMap;

    //--- 光沢値（Phong/Blinn 用） ----------------------------
    float mShininess{32.0f};

    //--- マテリアルカラーセット -----------------------------
    Vector3 mAmbientColor{0.5f, 0.5f, 0.5f};
    Vector3 mDiffuseColor{0.5f, 0.5f, 0.5f};
    Vector3 mSpecularColor{0.5f, 0.5f, 0.5f};

    //--- 完全に単色化する場合の制御 -------------------------
    bool    mOverrideColor = false;
    Vector3 mUniformColor  = Vector3::Zero;
};

} // namespace toy
