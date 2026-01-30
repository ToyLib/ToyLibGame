// Graphics/Billboard/TextBillboardComponent.h
#pragma once

#include "Graphics/Billboard/BillboardComponent.h"
#include "Engine/Render/RenderQueue.h"
#include "Utils/StringUtil.h"
#include "Utils/MathUtil.h"

#include <string>
#include <memory>

namespace toy {

//==============================================================
// TextBillboardComponent
//   - テキストを「ビルボードとして」3D空間に描画
//   - 内部で文字列 → テクスチャ生成
//   - 描画は RenderQueue 経由（新パス）
//==============================================================
class TextBillboardComponent : public BillboardComponent
{
public:
    TextBillboardComponent(class Actor* owner,
                           int drawOrder = 200);

    virtual ~TextBillboardComponent();

    //----------------------------------------------------------
    // テキスト設定
    //----------------------------------------------------------
    void SetText(const std::string& text);

    template<typename... Args>
    void SetFormat(const std::string& fmt, Args&&... args)
    {
        SetText(StringUtil::Format(fmt, std::forward<Args>(args)...));
    }

    //----------------------------------------------------------
    // 色 / フォント
    //----------------------------------------------------------
    void SetColor(const Vector3& color);
    void SetFont(std::shared_ptr<class TextFont> font);

    void Refresh();

    const std::string& GetText()  const { return mText; }
    const Vector3&     GetColor() const { return mColor; }
    std::shared_ptr<class TextFont> GetFont() const { return mFont; }

    //----------------------------------------------------------
    // 新パス用：RenderItem 生成
    //----------------------------------------------------------
    void GatherRenderItems(RenderQueueLike& out) override;


private:
    void UpdateTexture();

private:
    std::string mText;
    Vector3     mColor { 1.0f, 1.0f, 1.0f };
    std::shared_ptr<class TextFont> mFont;

    bool mIsDirty { true };
};

} // namespace toy
