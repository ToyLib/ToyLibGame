//==============================================================================
// RenderQueue.cpp
//  - RenderQueue::Sort 実装
//  - 旧描画パス（DrawVisualLayer）に近い見え方を再現するための
//    最低限のソート規約を提供
//==============================================================================

#include "Render/RenderQueue.h"

#include <algorithm>

namespace toy {

void RenderQueue::Sort()
{
    // 旧描画パスに寄せるため、安定ソートを使用
    // （同値の場合は Push された順序を維持）
    std::stable_sort(
        mItems.begin(),
        mItems.end(),
        [](const RenderItem& a, const RenderItem& b)
        {
            //--------------------------------------------------------------------------
            // 1) RenderPass
            //    - World -> Shadow -> UI ... の順
            //    - enum の定義順に依存
            //--------------------------------------------------------------------------
            if (a.pass != b.pass)
            {
                return a.pass < b.pass;
            }

            //--------------------------------------------------------------------------
            // 2) VisualLayer
            //    - Object3D / Effect / UI など
            //    - enum の定義順に依存
            //--------------------------------------------------------------------------
            if (a.layer != b.layer)
            {
                return a.layer < b.layer;
            }

            //--------------------------------------------------------------------------
            // 3) BlendMode
            //    - Opaque を先に描いて Z を確定
            //    - Transparent は後段で重ねる
            //--------------------------------------------------------------------------
            const bool aOpaque = (a.blend == BlendMode::Opaque);
            const bool bOpaque = (b.blend == BlendMode::Opaque);
            if (aOpaque != bOpaque)
            {
                return aOpaque; // true (opaque) が先
            }

            //--------------------------------------------------------------------------
            // 4) DrawOrder
            //--------------------------------------------------------------------------
            if (a.drawOrder != b.drawOrder)
            {
                return a.drawOrder < b.drawOrder;
            }

            //--------------------------------------------------------------------------
            // 5) 完全一致
            //    - stable_sort により、投入順を維持
            //--------------------------------------------------------------------------
            return false;
        }
    );
}

} // namespace toy
