#pragma once

#include "Graphics/VisualComponent.h"
#include "Utils/MathUtil.h"
#include <memory>

namespace toy {

class Mesh;
class VertexArray;
class Texture;
class LightingManager;
class Shader;
class RenderQueue;

//------------------------------------------------------------
// MeshComponent
//  - 3Dメッシュ（静的）を描画するコンポーネント
//  - 新描画パス：Draw()は使わず、GatherRenderItems/GatherShadowItemsでRenderItemを積む
//  - Toonはフラグだけ渡す（輪郭は新パス完成後に再導入）
//------------------------------------------------------------
class MeshComponent : public VisualComponent
{
public:
    MeshComponent(class Actor* a,
                  int drawOrder = 100,
                  VisualLayer layer = VisualLayer::Object3D,
                  bool isSkeletal = false);

    virtual ~MeshComponent() = default;

    // 新描画パス
    virtual void GatherRenderItems(RenderQueue& out) override;
    virtual void GatherShadowItems(RenderQueue& out) override;

    // Mesh / Texture
    virtual void SetMesh(std::shared_ptr<class Mesh> m) { mMesh = std::move(m); }
    const std::shared_ptr<class Mesh>& GetMesh() const { return mMesh; }

    void SetTextureIndex(unsigned int index) { mTextureIndex = index; }

    // Skeletal/Static状態
    bool GetIsSkeletal() const { return mIsSkeletal; }

    // 複数VAOを持つメッシュへのアクセス
    std::shared_ptr<class VertexArray> GetVertexArray(int id) const;

    // Toon（フラグのみ）
    void SetToonRender(bool t) { mIsToon = t; }
    bool GetToon() const { return mIsToon; }

    // 今後使う（値保持だけ）
    void SetContourFactor(float f) { mContourFactor = f; }
    float GetContourFactor() const { return mContourFactor; }

    void SetContourColor(const Vector3& color) { mContourColor = color; }
    const Vector3& GetContourColor() const { return mContourColor; }

    // Skeletal側でoverrideする想定
    virtual void SetAnimID(unsigned int /*animID*/, bool /*mode*/) {}

    // ローカル補正
    void SetLocalScale(float scale) { mLocalScale = scale; }
    float GetLocalScale() const { return mLocalScale; }

    void SetLocalPositon(const Vector3& pos) { mLocalPos = pos; }
    const Vector3& GetLocalPosition() const { return mLocalPos; }

    void SetYawOffset(float radians) { mLocalRot = Quaternion(Vector3::UnitY, radians); }

protected:
    // resources
    std::shared_ptr<class Mesh>  mMesh;
    unsigned int                 mTextureIndex { 0 };

    bool mIsSkeletal { false };

    std::shared_ptr<class LightingManager> mLightingManger;
    std::shared_ptr<class Shader>          mShader;
    std::shared_ptr<class Shader>          mShadowShader;

    // toon params (flag only for now)
    bool    mIsToon { false };
    float   mContourFactor { 1.0f };
    Vector3 mContourColor  { 0.0f, 0.0f, 0.0f };

    // local transform tweak
    Vector3    mLocalPos   { Vector3::Zero };
    Quaternion mLocalRot   { Quaternion(Vector3::UnitY, 0.0f) };
    float      mLocalScale { 1.0f };
};

} // namespace toy
