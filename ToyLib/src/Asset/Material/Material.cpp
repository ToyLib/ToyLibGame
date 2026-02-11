#include "Asset/Material/Material.h"
#include "Render/GL/Shader.h"
#include "Asset/Material/Texture.h"

namespace toy {

//--------------------------------------------------------------
// コンストラクタ
//--------------------------------------------------------------
Material::Material()
{
}

//--------------------------------------------------------------
// BindToShader()
//   Shader に対してマテリアル情報を一括で反映させる。
//--------------------------------------------------------------
void Material::BindToShader(Shader* shader, int textureUnit) const
{
    if (!shader) return;

    shader->SetBooleanUniform("uOverrideColor", mOverrideColor);
    shader->SetVectorUniform ("uUniformColor",  mUniformColor);

    shader->SetVectorUniform("uAmbientColor",  mAmbientColor);
    shader->SetVectorUniform("uDiffuseColor",  mDiffuseColor);
    shader->SetVectorUniform("uSpecColor",     mSpecularColor);
    shader->SetFloatUniform ("uSpecPower",     mShininess);

    // ★重要：テクスチャ無しでも「必ず」有効テクスチャを bind する
    mDiffuseMap->SetActive(textureUnit);
    shader->SetTextureUniform("uTexture", textureUnit);

    // “使う/使わない”はシェーダ分岐用のフラグとして残してOK
    const bool canUseTexture = (mUseTexture && (mDiffuseMap != nullptr) && !mOverrideColor);
    shader->SetBooleanUniform("uUseTexture", canUseTexture);
}


void Material::BindToShader(std::shared_ptr<Shader> shader, int textureUnit) const
{
    BindToShader(shader.get(), textureUnit);
}

//--------------------------------------------------------------
// SetOverrideColor()
//--------------------------------------------------------------
void Material::SetOverrideColor(bool enable, const Vector3& color)
{
    mOverrideColor = enable;
    mUniformColor  = color;
}

} // namespace toy
