#pragma once

#include "Engine/Core/Component.h"
#include "Engine/Render/Renderer.h"

namespace toy {

//----------------------------------------------------------------------
// VisualComponent
//  - すべての「描画を行うコンポーネント」の共通基底クラス
//  - Renderer からレイヤー順・描画順でまとめて呼び出される前提
//----------------------------------------------------------------------
class VisualComponent : public Component
{
public:
    // コンストラクタ
    //  owner      : 所有している Actor
    //  drawOrder  : 同一レイヤー内での描画順（小さいほど先に描画）
    //  layer      : 描画レイヤー（UI, 3Dオブジェクト, エフェクトなど）
    VisualComponent(class Actor* owner, int drawOrder, VisualLayer layer = VisualLayer::Effect3D);
    virtual ~VisualComponent();
    
    // 実際の描画処理
    //  各派生クラスで必ず実装する
    virtual void Draw() = 0;

    // シャドウマップへの描画
    //  影が不要なコンポーネントはデフォルト実装（何もしない）を使う
    virtual void DrawShadow() { DrawShadow(0); }
    virtual void DrawShadow(int cascadeIndex) { (void)cascadeIndex; }

    // 使用テクスチャの設定／取得
    virtual void SetTexture(std::shared_ptr<class Texture> tex) { mTexture = tex; }
    std::shared_ptr<class Texture> GetTexture() const { return mTexture; }
    
    // 表示・非表示の切り替え
    void SetVisible(bool v) { mIsVisible = v; }
    bool IsVisible() const { return mIsVisible; }
    
    // 加算ブレンドの有効／無効
    void SetBlendAdd(bool b) { mIsBlendAdd = b; }
    bool IsBlendAdd() const { return mIsBlendAdd; }
    
    // 描画レイヤーの設定／取得
    void SetLayer(VisualLayer layer) { mLayer = layer; }
    VisualLayer GetLayer() const { return mLayer; }
    
    // 描画順の設定／取得（同一レイヤー内のソートに使用）
    int GetDrawOrder() const { return mDrawOrder; }
    void SetDrawOrder(int order) { mDrawOrder = order; }
    
    // 使用シェーダ／ライティング管理の設定
    void SetShader(std::shared_ptr<class Shader> shader) { mShader = shader; }
    void SetLightingManager(std::shared_ptr<LightingManager> light) { mLightingManager = light; }
    
    // シャドウ描画を行うかどうか
    bool GetEnableShadow() const { return mEnableShadow; }
    void SetEnableShadow(const bool b) { mEnableShadow = b; }
    
    // 視錐台カリングを無効
    void SetDisableFrustumCulling(bool b) { mDisableFrustumCulling = b; }
    bool GetDisableFrustumCulling() const { return mDisableFrustumCulling; }


protected:
    // メインテクスチャ
    std::shared_ptr<class Texture> mTexture;

    // 使用するシェーダ
    std::shared_ptr<class Shader> mShader;

    // ライティング情報をまとめて管理するオブジェクト
    std::shared_ptr<class LightingManager> mLightingManager;

    // 可視状態フラグ
    bool mIsVisible { true };

    // 加算ブレンドを使うかどうか
    bool mIsBlendAdd { false };

    // 描画レイヤー種別（Object3D, OverlayScreen など）
    VisualLayer mLayer {};

    // 同一レイヤー内での描画順
    int mDrawOrder {};

    // シャドウマップに描画するかどうか
    bool mEnableShadow { false };

    // 描画に使う頂点配列（フルスクリーンクアッドなど）
    std::shared_ptr<class VertexArray> mVertexArray;
    
    // 視錐台カリングを無効
    bool mDisableFrustumCulling { false };
};

} // namespace toy
