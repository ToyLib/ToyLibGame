// Engine/Render/RenderQueue.h
#pragma once
#include <vector>
#include "Engine/Render/RenderItem.h"

namespace toy {

class RenderQueue
{
public:
    void Push(const RenderItem& item) { mItems.emplace_back(item); }

    const std::vector<RenderItem>& Items() const { return mItems; }
    std::vector<RenderItem>&       Items()       { return mItems; }

    void Clear() { mItems.clear(); }

    void Sort();

private:
    std::vector<RenderItem> mItems;
};

} // namespace toy
