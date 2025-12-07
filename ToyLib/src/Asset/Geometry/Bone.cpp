#include "Asset/Geometry/Bone.h"
#include <iostream>
#include <cassert>

namespace toy {

//---------------------------------------------------------------
// VertexBoneData::AddBoneData
//   - ボーンを最大 NUM_BONES_PER_VERTEX（4本）まで登録
//   - 空いているスロットに BoneID と Weight を追加
//   - 全て埋まっている場合は assert によりデバッグ時に検出
//---------------------------------------------------------------
void VertexBoneData::AddBoneData(unsigned int boneID, float weight)
{
    // 空きスロットを探す
    for (unsigned int i = 0; i < NUM_BONES_PER_VERTEX; ++i)
    {
        // 空きスロットは weight = 0.0 で判定
        if (Weights[i] == 0.0f)
        {
            IDs[i]     = boneID;
            Weights[i] = weight;
            return;
        }
    }

    // ここに来る＝4本を超えてボーンが割り当てられた
    // （Assimp データ側の方が4より多いケースがある）
    //assert(0 && "Too many bone influences for this vertex!");
}

} // namespace toy
