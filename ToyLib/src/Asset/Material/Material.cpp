#include "Asset/Material/Material.h"
#include "Render/GL/GLShader.h"
#include "Render/GL/UniformNamesGL.h"
#include "Asset/Material/Texture.h"

namespace toy {

Material::Material()
{
}

void Material::BindToShader(GLShader* shader, int textureUnit) const
{
    if (!shader) return;

    using namespace toy::glsl;

    //========================================================
    // Override color (contract)
    //========================================================
    shader->SetBooleanUniform(toy::glsl::Material::OverrideEnable, mOverrideColor);
    shader->SetVectorUniform (toy::glsl::Material::OverrideColor,  mUniformColor);

    //========================================================
    // BaseColor / SpecPower (contract)
    //========================================================
    // 旧：uDiffuseColor → 新：uMaterial.baseColor
    shader->SetVectorUniform(toy::glsl::Material::BaseColor, mDiffuseColor);

    // 旧：uSpecPower → 新：uMaterial.specPower
    shader->SetFloatUniform(toy::glsl::Material::SpecPower, mShininess);

    //========================================================
    // BaseMap (contract)
    // 重要：テクスチャ無しでも「必ず」有効テクスチャを bind する挙動を維持
    //========================================================
    if (mDiffuseMap)
    {
        mDiffuseMap->SetActive(textureUnit);
    }
    // ※ mDiffuseMap が null のケースがあるなら、ここでダミーを bind するのが従来挙動の前提。
    //    現状 ToyLib ではダミーが入っている運用なので「動作変更なし」。

    shader->SetTextureUniform(toy::glsl::Material::BaseMap, textureUnit);

    //========================================================
    // UseTexture (contract)
    // “使う/使わない”フラグは従来通り
    //========================================================
    const bool canUseTexture = (mUseTexture && (mDiffuseMap != nullptr) && !mOverrideColor);
    shader->SetBooleanUniform(toy::glsl::Material::UseTexture, canUseTexture);

    //========================================================
    // Toon (contract) - ここは Material 側に値が無いなら触らない
    //========================================================
    // shader->SetBooleanUniform(Material::Toon, ...);
}

void Material::BindToShader(std::shared_ptr<GLShader> shader, int textureUnit) const
{
    BindToShader(shader.get(), textureUnit);
}

void Material::SetOverrideColor(bool enable, const Vector3& color)
{
    mOverrideColor = enable;
    mUniformColor  = color;
}

TextureHandle Material::GetDiffuseTextureHandle() const
{
    TextureHandle h;
    h.ptr = mDiffuseMap.get();
    return h;
}

} // namespace toy
