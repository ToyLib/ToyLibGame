#pragma once

#include "KitSignal/Events.h"

namespace toy { struct InputState; }

namespace toy::kit {

class Prefab;

//=============================================================================
// IBehavior
//  Prefab に「任意の振る舞い」を後付けするためのインターフェース。
//
//  Prefab 派生クラス（Creature/Humanoid 等）を新たに作らず、Prefab の
//  Interface（SetPosition/PlayAnimation 等）と Signal 越しに動く小さな
//  振る舞いを Agent<TPrefab> に差し込む形で組み合わせる。
//
//  同じ Prefab（例: Humanoid）を使う敵が複数いて、行動だけが違う場合に
//  専用の Game Logic クラスを何個も書かずに済ませるための仕組み。
//  1体しかいない主人公のような「一回性の複雑な振る舞い」は、素直に
//  専用の Game Logic クラス（Hero 等）を書く方が読みやすい。
//=============================================================================
class IBehavior
{
public:
    virtual ~IBehavior() = default;

    // Agent 生成直後に一度だけ呼ばれる
    virtual void OnStart(Prefab& body) {}

    // 毎フレーム呼ばれる
    virtual void OnUpdate(Prefab& body, float deltaTime) {}

    // 入力処理が必要な振る舞い（プレイヤー操作等）だけ override する
    virtual void OnInput(Prefab& body, const toy::InputState& state) {}

    // body 側で衝突が検知されたときに呼ばれる
    virtual void OnCollision(Prefab& body, const CollisionEvent& event) {}
};

} // namespace toy::kit
