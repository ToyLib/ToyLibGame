#pragma once

#include "Graphics/VisualComponent.h"
#include "Utils/MathUtil.h"
#include <memory>

namespace toy {

//------------------------------------------------------------
// MeshComponent
//   ・3Dメッシュ（静的 / スキンメッシュ）を描画するコンポーネント
//   ・Mesh, Shader, LightingManager などの描画リソースを保持
//   ・各種描画モード（トゥーン / シャドウ）に対応
//------------------------------------------------------------
class MeshComponent : public VisualComponent
{
public:
    //--------------------------------------------------------
    // コンストラクタ
    // a         : 所有Actor
    // drawOrder : 描画順序
    // layer     : VisualLayer（通常3D、エフェクト3D等）
    // isSkeletal: スキンメッシュかどうか
    //--------------------------------------------------------
    MeshComponent(class Actor* a,
                  int drawOrder = 100,
                  VisualLayer layer = VisualLayer::Effect3D,
                  bool isSkeletal = false);

    virtual ~MeshComponent();
    
    //--------------------------------------------------------
    // Draw()
    //   ・通常のメッシュ描画
    //--------------------------------------------------------
    virtual void Draw() override;

    //--------------------------------------------------------
    // DrawShadow()
    //   ・影描画専用（ShadowMap生成用）
    //--------------------------------------------------------
    virtual void DrawShadow(int cascadeIndex) override;
    
    //--------------------------------------------------------
    // Mesh / Texture 設定
    //--------------------------------------------------------
    virtual void SetMesh(std::shared_ptr<class Mesh> m) { mMesh = m; }
    const std::shared_ptr<class Mesh>& GetMesh() const { return mMesh; }
    void SetTextureIndex(unsigned int index) { mTextureIndex = index; }

    //--------------------------------------------------------
    // Skeletal / Static メッシュ状態
    //--------------------------------------------------------
    bool GetIsSkeletal() const { return mIsSkeletal; }

    // 複数 VAO を持つメッシュ（マルチマテリアル等）へのアクセス
    std::shared_ptr<class VertexArray> GetVertexArray(int id) const;

    //--------------------------------------------------------
    // トゥーン描画設定（輪郭強調）
    //--------------------------------------------------------
    void SetToonRender(bool t) { mIsToon = t; }
    
    //--------------------------------------------------------
    // トゥーン描画設定（輪郭強調）
    //--------------------------------------------------------
    void SetContourFactor(float f) { mContourFactor = f; }
    bool GetToon() const { return mIsToon; }
    void SetContourColor(const Vector3& color) { mContourColor = color; }

    //--------------------------------------------------------
    // アニメーションの現在IDをセット（SkeletalMeshComponent が override）
    //--------------------------------------------------------
    virtual void SetAnimID(unsigned int animID, bool mode) {}
    
    void SetLocalScale(const float scale) { mLocalScale = scale; }
    float GetLocalScale() const { return mLocalScale; }
    void SetLocalPositon(const Vector3& pos) { mLocalPos = pos; }
    const Vector3& GetLocalPosition() const { return mLocalPos; }
    void SetYawOffset(float radians)
    {
        mLocalRot = Quaternion(Vector3::UnitY, radians);
    }
protected:
    //--------------------------------------------------------
    // 保持している描画リソース
    //--------------------------------------------------------
    std::shared_ptr<class Mesh>  mMesh{};               // 描画対象メッシュ
    unsigned int mTextureIndex {};                       // 使用するメッシュ内テクスチャインデックス

    bool mIsSkeletal;                                 // スキンメッシュかどうか

    std::shared_ptr<class Texture> mShadowMapTexture; // 影用テクスチャ（必要に応じて）

    // ライティング・シェーダー
    std::shared_ptr<class LightingManager> mLightingManger;
    std::shared_ptr<class Shader> mShader;           // 通常描画用シェーダ
    std::shared_ptr<class Shader> mShadowShader;     // シャドウマップ描画用シェーダ

    //--------------------------------------------------------
    // トゥーン（輪郭）描画設定
    //--------------------------------------------------------
    bool  mIsToon;          // true なら toon + Outline
    float mContourFactor;   // 1.05f など。輪郭スケール係数
    Vector3 mContourColor;  // 輪郭色
    
    
    //--------------------------------------------------------
    // ローカル変換（補正値）
    //--------------------------------------------------------
    Vector3    mLocalPos;
    Quaternion mLocalRot;
    float      mLocalScale;
    
};

} // namespace toy
