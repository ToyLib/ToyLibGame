#include "Asset/Material/Material.h"
#include "Engine/Render/Shader.h"
#include "Asset/Material/Texture.h"

namespace toy {

//--------------------------------------------------------------
// コンストラクタ
//   ・基本のマテリアルカラーを設定
//   ・テクスチャなし状態で初期化
//--------------------------------------------------------------
Material::Material()
: mAmbientColor(0.2f, 0.2f, 0.2f)
, mDiffuseColor(0.8f, 0.8f, 0.8f)
, mSpecularColor(1.0f, 1.0f, 1.0f)
, mShininess(32.0f)
, mDiffuseMap(nullptr)
, mOverrideColor(false)
, mUniformColor(Vector3::Zero)
{
}

//--------------------------------------------------------------
// BindToShader()
//   Shader に対してマテリアル情報を一括で反映させる。
//   ・単色描画フラグ
//   ・Ambient / Diffuse / Specular / Shininess
//   ・DiffuseMap のバインド
//--------------------------------------------------------------
void Material::BindToShader(std::shared_ptr<Shader> shader,
                            int textureUnit) const
{
    // 単色描画（OverrideColor）
    shader->SetBooleanUniform("uOverrideColor", mOverrideColor);
    shader->SetVectorUniform("uUniformColor", mUniformColor);

    // マテリアル基本色
    shader->SetVectorUniform("uAmbientColor",  mAmbientColor);
    shader->SetVectorUniform("uDiffuseColor",  mDiffuseColor);
    shader->SetVectorUniform("uSpecColor",     mSpecularColor);
    shader->SetFloatUniform ("uSpecPower",     mShininess);

    // DiffuseMap（基本1枚のみ）
    if (mDiffuseMap)
    {
        mDiffuseMap->SetActive(textureUnit);
        shader->SetTextureUniform("uTexture", textureUnit);
        shader->SetBooleanUniform("uUseTexture", true);
    }
    else
    {
        shader->SetBooleanUniform("uUseTexture", false);
    }
}

//--------------------------------------------------------------
// SetOverrideColor()
//   DiffuseMap を無視して単色で描画したい場合に使用。
//   Toon系表現・デバッグ描画などで活用できる。
//--------------------------------------------------------------
void Material::SetOverrideColor(bool enable, const Vector3& color)
{
    mOverrideColor = enable;
    mUniformColor  = color;
}

} // namespace toy
