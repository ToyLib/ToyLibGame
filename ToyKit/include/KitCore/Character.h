#pragma once

#include "Utils/MathUtil.h"
#include <string>

namespace toy {
    class Actor;
}

namespace toy::kit {

/// =============================================
/// Character
///  - toy::Actor をラップした「ゲーム上のキャラ」クラス
///  - まずは位置操作などの薄いヘルパーだけを持たせる
///  - 将来 MoveComponent やアニメーション制御をここに集約していく
/// =============================================
class Character
{
public:
    Character() = default;
    explicit Character(toy::Actor* actor);

    // 有効な Actor を保持しているか？
    bool IsValid() const { return mActor != nullptr; }

    // 生の Actor へのアクセス（必要に応じて）
    toy::Actor*       GetActor()       { return mActor; }
    const toy::Actor* GetActor() const { return mActor; }

    // 位置操作ヘルパ
    Vector3 GetPosition() const;
    void    SetPosition(const Vector3& pos);
    void    AddWorldOffset(const Vector3& delta);

    // シンプルな移動ヘルパ（ワールド座標系、即時反映）
    void Move(const Vector3& dir, float speed, float deltaTime);

    // 将来の拡張ポイント：キャラ固有の更新処理
    void Update(float deltaTime);

private:
    toy::Actor* mActor = nullptr;  // 所有しない
};

} // namespace toy::kit
