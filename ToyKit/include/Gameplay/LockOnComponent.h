#pragma once
#include "Engine/Core/Component.h"
#include "Utils/MathUtil.h"

namespace toy::kit {






class LockOnComponent : public toy::Component
{
public:
    enum class BasisMode
    {
        Camera,  // 視界＝カメラ基準
        Player   // 視界＝プレイヤー(Owner)基準
    };

    struct Desc
    {
        BasisMode mode;
        float     breakDist;
        bool      keepYawOnly;

        Desc()
            : mode(BasisMode::Camera)
            , breakDist(30.0f)
            , keepYawOnly(true)
        {}
    };

    LockOnComponent(class toy::Actor* owner, const Desc& desc = {}, int updateOrder = 5);

    // toggle / mode
    void Toggle();
    void Unlock();

    bool IsLocked() const { return mLocked && mTarget; }
    toy::Actor* GetTarget() const { return mTarget; }

    void SetMode(BasisMode m) { mDesc.mode = m; }
    BasisMode GetMode() const { return mDesc.mode; }

    // Move系が参照する「基準軸」
    Vector3 GetBasisForward() const;
    Vector3 GetBasisRight() const;

    // 任意：外から明示セットしたい時（FollowMove と同じ思想）
    void SetTarget(class toy::Actor* t, bool lock = true);

    void Update(float dt) override;

private:
    class toy::Actor* PickNearestTarget() const;

    Vector3 FlattenAndNormalize(const Vector3& v) const;

private:
    Desc  mDesc{};
    bool  mLocked = false;
    class toy::Actor* mTarget = nullptr;

    // ここでは「敵一覧」の取り方を Application から仮取得する想定にしておく
    // 実装はアプリ側の設計に合わせて差し替え。
};

} // namespace toy::kit
