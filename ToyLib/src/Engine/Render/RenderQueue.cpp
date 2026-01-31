//==============================================================================
// RenderQueue.cpp
//  - RenderQueue::Sort 実装
//  - 旧パス（DrawVisualLayer）に近い見え方を再現するための
//    最低限のソート規約を提供
//==============================================================================

#include "Engine/Render/RenderQueue.h"

#include <algorithm>

namespace toy {

void RenderQueue::Sort()
{
    // 旧描画パスに寄せる：安定ソート（同値の場合に積んだ順を維持）
    std::stable_sort(
        mItems.begin(),
        mItems.end(),
        [](const RenderItem& a, const RenderItem& b)
        {
            // 1) Pass（World -> Shadow -> UI など。enum の順序前提）
            if (a.pass != b.pass)
                return a.pass < b.pass;

            // 2) Layer（VisualLayer の enum 順序前提）
            if (a.layer != b.layer)
                return a.layer < b.layer;

            // 3) Opaque を先に（Z を作ってから透明を重ねる）
            const bool aOpaque = (a.blend == BlendMode::Opaque);
            const bool bOpaque = (b.blend == BlendMode::Opaque);
            if (aOpaque != bOpaque)
                return aOpaque; // true(opaque) が先

            // 4) drawOrder（昇順）
            if (a.drawOrder != b.drawOrder)
                return a.drawOrder < b.drawOrder;

            // 5) ここまで全部同じなら安定ソートに任せる
            return false;
        }
    );
}

} // namespace toy
