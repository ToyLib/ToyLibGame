#pragma once

#include "Utils/MathUtil.h"

#include <cstdint>
#include <string>

namespace toy::kit {

//=============================================================================
// StaticObjectDesc
//  StaticObject Prefab の構築情報（設計方針 5/16）。
//  建物・岩・家具・障害物など、メッシュとコライダーを置くだけの
//  動かない物に使う。
//=============================================================================
struct StaticObjectDesc
{
    std::string model;
    bool        toonRender = false;

    // Actor 全体のスケール（Wolf/House/Brick方式）。
    // メッシュコンポーネント側のローカルスケールを使いたい場合は meshScale を使う。
    float actorScale = 1.0f;
    float meshScale   = 1.0f;

    // コライダーの構築方法
    //  false: メッシュアセットの頂点配列から直接（actorScale の影響を受けない）
    //  true : MeshComponent（meshScale 適用後）から
    bool colliderFromMeshComponent = false;

    Vector3  colliderOffset = Vector3::Zero;
    Vector3  colliderScale  = Vector3::One;
    uint32_t colliderFlags  = 0;

    // 地面に沈める/乗せるなど、生成時に一度だけ重力で落ち着かせたい場合に使う
    bool useGravity = false;
};

} // namespace toy::kit
