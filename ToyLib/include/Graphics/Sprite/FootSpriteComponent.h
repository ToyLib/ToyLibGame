#pragma once
#include "Graphics/VisualComponent.h"
#include "Utils/MathUtil.h"
#include <memory>
#include <string>

namespace toy {

enum class FootBlendMode
{
    Alpha,
    Additive
};

class FootSpriteComponent : public VisualComponent
{
public:
    FootSpriteComponent(class Actor* owner,
                        int drawOrder = 10,
                        VisualLayer layer = VisualLayer::Effect3D);

    virtual ~FootSpriteComponent() = default;

    // 描画
    void Draw() override;

    // テクスチャ（VisualComponent互換）
    void SetTexture(std::shared_ptr<class Texture> tex) override;
    std::shared_ptr<Texture> GetTexture() const { return mTexture; }

    // ワールド単位サイズ（幅= X, 奥行= Z）
    void SetSize(float width, float depth) { mWidth = width; mDepth = depth; }
    void SetWidth(float w) { mWidth = w; }
    void SetDepth(float d) { mDepth = d; }

    // オフセット（足元基準）
    void SetOffsetPosition(const Vector3& v) { mOffsetPosition = v; }

    // 追加スケール（演出で拡縮したい時）
    void SetOffsetScale(float s) { mOffsetScale = s; }

    // Yaw（地面上で回転）
    void SetYaw(float rad) { mYaw = rad; }

    // 色/透明度（Unlit側で使う想定。Meshシェーダでも無視されるだけ）
    void SetTint(const Vector3& c) { mTint = c; }
    void SetAlpha(float a) { mAlpha = a; }

    // ブレンド
    void SetBlendMode(FootBlendMode m) { mBlendMode = m; }

    // Shader選択（"Unlit" / "Mesh" など）
    //void SetShaderName(const std::string& name) { mShaderName = name; }
    //const std::string& GetShaderName() const { return mShaderName; }

protected:
    // 派生側が “ワールド行列の組み立てだけ” 触れるようにする
    virtual Matrix4 BuildWorldMatrix() const;

    // 描画前後フック（必要になったら派生で上書き）
    virtual void PreDraw()  {}
    virtual void PostDraw() {}

protected:
    std::shared_ptr<class Texture> mTexture;

    // world units
    float mWidth  = 1.0f;
    float mDepth  = 1.0f;

    Vector3 mOffsetPosition = Vector3::Zero;
    float   mOffsetScale    = 1.0f;
    float   mYaw            = 0.0f;

    // Unlit向けパラメータ
    Vector3 mTint  = Vector3(1.0f, 1.0f, 1.0f);
    float   mAlpha = 1.0f;

    FootBlendMode mBlendMode = FootBlendMode::Alpha;
};

} // namespace toy
