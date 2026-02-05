// Engine/Render/RenderQueue.h
#pragma once

#include <vector>

#include "Engine/Render/RenderItem.h"
#include "Engine/Render/RenderItemPayloads.h"

namespace toy {

//==============================================================================
// RenderQueue
//  - 1フレーム分の RenderItem を保持するキュー
//  - BuildFrameQueues() などで積まれ、描画前に Sort() される
//==============================================================================
class RenderQueue
{
public:
    //--------------------------------------------------------------------------
    // Item management
    //--------------------------------------------------------------------------

    uint32_t Push(const RenderItem& item)
    {
        mItems.emplace_back(item);
        return static_cast<uint32_t>(mItems.size() - 1);
    }

    void Clear()
    {
        mItems.clear();
    }

    //--------------------------------------------------------------------------
    // Accessors
    //--------------------------------------------------------------------------

    const std::vector<RenderItem>& Items() const { return mItems; }
    std::vector<RenderItem>&       Items()       { return mItems; }

    //--------------------------------------------------------------------------
    // Sorting
    //--------------------------------------------------------------------------

    void Sort();

private:
    std::vector<RenderItem> mItems;
};

} // namespace toy
