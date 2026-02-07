#include "Asset/Material/Material.h"
#include "Render/Shader.h"
#include "Asset/Material/Texture.h"

namespace toy {

//--------------------------------------------------------------
// コンストラクタ
//   ・基本のマテリアルカラーを設定
//   ・テクスチャなし状態で初期化
//--------------------------------------------------------------
Material::Material()
{
}

//--------------------------------------------------------------
// BindToShader()
//   Shader に対してマテリアル情報を一括で反映させる。
//   ・単色描画フラグ
//   ・Ambient / Diffuse / Specular / Shininess
//   ・DiffuseMap のバインド
//--------------------------------------------------------------
void Material::BindToShader(Shader* shader, int textureUnit) const
{
    if (!shader) return;

    shader->SetBooleanUniform("uOverrideColor", mOverrideColor);
    shader->SetVectorUniform("uUniformColor",  mUniformColor);

    shader->SetVectorUniform("uAmbientColor",  mAmbientColor);
    shader->SetVectorUniform("uDiffuseColor",  mDiffuseColor);
    shader->SetVectorUniform("uSpecColor",     mSpecularColor);
    shader->SetFloatUniform ("uSpecPower",     mShininess);

    // ★ここ、mDiffuseMapがnullの可能性があるならガード推奨
    if (mDiffuseMap)
    {
        mDiffuseMap->SetActive(textureUnit);
        shader->SetTextureUniform("uTexture", textureUnit);
    }
    shader->SetBooleanUniform("uUseTexture", mUseTexture);
}

void Material::BindToShader(std::shared_ptr<Shader> shader, int textureUnit) const
{
    BindToShader(shader.get(), textureUnit);
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
