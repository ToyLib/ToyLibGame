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

class RenderQueue final : public RenderQueueLike
{
public:
    void Push(const RenderItem& item) override { mItems.push_back(item); }

    // Renderer側で消費する用
    const std::vector<RenderItem>& Items() const { return mItems; }
    std::vector<RenderItem>& Items() { return mItems; }

    void Clear() { mItems.clear(); }
    bool Empty() const { return mItems.empty(); }

private:
    std::vector<RenderItem> mItems;
};

} // namespace toy
