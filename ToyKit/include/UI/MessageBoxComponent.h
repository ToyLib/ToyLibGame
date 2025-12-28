#pragma once

#include "Graphics/Sprite/TextSpriteComponent.h"
#include "Utils/StringUtil.h"
#include "Utils/MathUtil.h"
#inlcude <string>
#include <vector>

namespace toy::kit {

class MessageBoxComponent : public toy::TextSpriteComponent
{
public:
    MessageBoxComponent(class toy::Actor* owner,
                        int drawOrder = 100,
                        toy::VisualLayer layer = toy::VisualLayer::UI);
    
    void SetTextBoxSize(const Vector2& size);
    void SetPadding(const Vector2& padding);

    void SetMessage(const std::string& text);

    bool HasNextPage() const;
    void NextPage();
    void ResetPage();

    const std::string& GetCurrentPageText() const;

private:
    void BuildPages();

private:
    // 入力
    std::string mRawText;

    // レイアウト
    Vector2 mBoxSize  = { 640, 160 };
    Vector2 mPadding  = { 16, 16 };

    float mCharWidth  = 12.0f;  // 等幅想定
    float mLineHeight = 18.0f;

    // 結果
    std::vector<std::string> mPages;
    int mCurrentPage = 0;
};

} // namespace toy::kit
