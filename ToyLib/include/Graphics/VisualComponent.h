#pragma once

#include "Render/VisualLayer.h"
#include "Engine/Core/Component.h"
#include "Asset/Material/Texture.h"   // 既存 SetTexture を維持したいなら
#include <memory>

namespace toy {

class Shader;
class LightingManager;
class VertexArray;

// 新方式のキュー（まだ無くてもビルド通すため forward）
struct RenderItem;
class RenderQueue;

//----------------------------------------------------------------------
// VisualComponent (Transitional)
//  - 旧: Draw() で直接描画（GL直叩き）
//  - 新: GatherRenderItems() で RenderItem を提出
//----------------------------------------------------------------------
class VisualComponent : public Component
{
public:
    VisualComponent(class Actor* owner, int drawOrder, VisualLayer layer = VisualLayer::Effect3D);
    virtual ~VisualComponent();

    //========================
    // 新方式（段階導入）
    //========================
    virtual void GatherRenderItems(RenderQueue& out) {}
    virtual void GatherShadowItems(RenderQueue& out) {} // ★追加

    //========================
    // 共通状態
    //========================
    virtual void SetTexture(std::shared_ptr<class Texture> tex) { mTexture = tex; }
    std::shared_ptr<class Texture> GetTexture() const { return mTexture; }

    void SetVisible(bool v) { mIsVisible = v; }
    bool IsVisible() const { return mIsVisible; }

    void SetBlendAdd(bool b) { mIsBlendAdd = b; }
    bool IsBlendAdd() const { return mIsBlendAdd; }

    void SetLayer(VisualLayer layer) { mLayer = layer; }
    VisualLayer GetLayer() const { return mLayer; }

    int  GetDrawOrder() const { return mDrawOrder; }
    void SetDrawOrder(int order) { mDrawOrder = order; }

    //void SetShader(std::shared_ptr<class Shader> shader) { mShader = shader; }
    void SetLightingManager(std::shared_ptr<LightingManager> light) { mLightingManager = light; }

    bool GetEnableShadow() const { return mEnableShadow; }
    void SetEnableShadow(const bool b) { mEnableShadow = b; }

    void SetDisableFrustumCulling(bool b) { mDisableFrustumCulling = b; }
    bool GetDisableFrustumCulling() const { return mDisableFrustumCulling; }

protected:
    // 旧方式で使っているもの（Sprite/Meshが依存してるので当面残す）
    std::shared_ptr<class Texture>         mTexture;
    std::string mPipelineName;
    
    std::shared_ptr<class LightingManager> mLightingManager;
    std::shared_ptr<class VertexArray>     mVertexArray;

    bool        mIsVisible { true };
    bool        mIsBlendAdd { false };
    VisualLayer mLayer {};
    int         mDrawOrder {};

    bool        mEnableShadow { false };
    bool        mDisableFrustumCulling { false };
};

} // namespace toy
