// Engine/Render/RenderQueue.h
#pragma once
#include <vector>
#include "Engine/Render/RenderItem.h"

namespace toy {

class RenderQueueLike
{
public:
    virtual ~RenderQueueLike() = default;
    virtual void Push(const RenderItem& item) = 0;
};

// ★ “queue.Items()” を維持したい前提の実装例
class RenderQueue final : public RenderQueueLike
{
public:
    void Push(const RenderItem& item) override { mItems.emplace_back(item); }

    const std::vector<RenderItem>& Items() const { return mItems; }
    std::vector<RenderItem>&       Items()       { return mItems; }

    void Clear() { mItems.clear(); }
    
    void Sort();

private:
    std::vector<RenderItem> mItems;
};

} // namespace toy
