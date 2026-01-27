#pragma once

#include "Utils/MathUtil.h"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

namespace toy {

//-------------------------------------------------------------
// Actor
//------------------------------------------------------------------------------
// ・ToyLib の基本単位（エンティティ）
// ・Component を保持し、Update / Input / Transform を制御する
// ・Transform は「位置・回転・スケール」を保持し、必要なタイミングで
//   WorldTransform（および RenderWorldTransform）を再計算する
//-------------------------------------------------------------
class Actor
{
public:
    //=========================================================================
    // State
    //=========================================================================
    enum class State
    {
        Active,   // 通常動作
        Paused,   // 更新停止
        Dead      // 削除予約
    };

    //=========================================================================
    // Lifecycle
    //=========================================================================
    Actor(class Application* a);
    virtual ~Actor();

    //=========================================================================
    // Update / Input
    //=========================================================================
    // フレーム更新（内部 → 派生Actor → コンポーネント → Transform確定）
    void Update(float deltaTime);

    // Component 更新
    void UpdateComponents(float deltaTime);

    // 派生 Actor が override する更新処理
    virtual void UpdateActor(float deltaTime) {}

    // 入力を全 Component に伝える
    void ProcessInput(const struct InputState& state);

    // Actor 固有の入力処理
    virtual void ActorInput(const struct InputState& state) {};

    //=========================================================================
    // Transform (SRT)  ※保持しているのはワールド基準の値
    //=========================================================================
    // 変更が入ったら MarkWorldDirty() を立て、WorldTransform は遅延再計算する
    const Vector3& GetPosition() const { return mPosition; }
    void SetPosition(const Vector3& pos) { mPosition = pos; MarkWorldDirty(); }

    const float& GetScale() const { return mScale; }
    void SetScale(float sc) { mScale = sc; MarkWorldDirty(); }

    const Quaternion& GetRotation() const { return mRotation; }
    void SetRotation(const Quaternion& rot) { mRotation = rot; MarkWorldDirty(); }

    // Forward/Right/Up
    const Vector3 GetForward() const { return Vector3::Transform(Vector3::UnitZ, mRotation); }
    const Vector3 GetRight()   const { return Vector3::Transform(Vector3::UnitX, mRotation); }
    const Vector3 GetUpward()  const { return Vector3::Transform(Vector3::UnitY, mRotation); }

    // Forward を直接セットする（内部で回転を調整）
    void SetForward(const Vector3& dir);

    //=========================================================================
    // World Transform (lazy recompute)
    //=========================================================================
    // 変更が入ったら MarkWorldDirty() を立て、必要時にのみ再計算する
    void MarkWorldDirty();

    // ワールド行列の計算（dirty のときだけ実行）
    void ComputeWorldTransform();

    // ワールド行列取得（必要なら再計算）
    const Matrix4& GetWorldTransform()
    {
        ComputeWorldTransform();
        return mWorldTransform;
    }

    //=========================================================================
    // Pose (Render only)
    //=========================================================================
    // ・描画用の姿勢（Pitch/Roll など）。物理には基本適用しない
    // ・RenderWorldTransform = Pose * WorldTransform
    void SetPoseRotation(const Quaternion& q) { mPoseRotation = q; }
    const Quaternion& GetPoseRotation() const { return mPoseRotation; }

    const Matrix4& GetRenderWorldTransform()
    {
        ComputeWorldTransform();
        return mRenderWorldTransform;
    }

    //=========================================================================
    // State / Application
    //=========================================================================
    const State& GetState() const { return mStatus; }
    void SetState(const State& state) { mStatus = state; }

    class Application* GetApp() { return mApp; }

    //=========================================================================
    // Component
    //=========================================================================
    void AddComponent(std::unique_ptr<class Component> component);
    void RemoveComponent(class Component* component);

    // Component 生成（Owner = this）
    template <typename T, typename... Args>
    T* CreateComponent(Args&&... args)
    {
        auto comp = std::make_unique<T>(this, std::forward<Args>(args)...);
        T* rawPtr = comp.get();
        AddComponent(std::unique_ptr<class Component>(std::move(comp)));
        return rawPtr;
    }

    // 最初に見つかった T を返す
    template <typename T>
    T* GetComponent() const
    {
        for (const auto& comp : mComponents)
        {
            if (auto casted = dynamic_cast<T*>(comp.get()))
            {
                return casted;
            }
        }
        return nullptr;
    }

    // 該当 Component をすべて返す
    template <typename T>
    std::vector<T*> GetAllComponents() const
    {
        std::vector<T*> results;
        for (auto& comp : mComponents)
        {
            T* casted = dynamic_cast<T*>(comp.get());
            if (casted)
            {
                results.emplace_back(casted);
            }
        }
        return results;
    }

    // Actor削除用
    void DestroyActor() { mStatus = State::Dead; }
    
    //=========================================================================
    // ID
    //=========================================================================
    void SetActorID(const std::string& actorID) { mActorID = actorID; }
    std::string GetActorID() const { return mActorID; }

private:
    //=========================================================================
    // Transform (stored values)
    //=========================================================================
    // ・mPosition/mRotation/mScale は「保持値」（ワールド基準）
    // ・mWorldTransform は dirty のときだけ ComputeWorldTransform で再構築
    Matrix4     mWorldTransform {Matrix4::Identity};     // ワールド行列
    Vector3     mPosition       {Vector3::Zero};         // 位置（ワールド）
    Quaternion  mRotation       {Quaternion::Identity};  // 回転（ワールド）
    float       mScale          {1.0f};                  // スケール（ワールド）
    bool        mIsRecomputeWorldTransform{true};

    //=========================================================================
    // Pose (render-only)
    //=========================================================================
    Quaternion  mPoseRotation{Quaternion::Identity};
    Matrix4     mRenderWorldTransform{Matrix4::Identity};

    //=========================================================================
    // Component / Application
    //=========================================================================
    std::vector<std::unique_ptr<class Component>> mComponents;
    class Application* mApp {};

    //=========================================================================
    // State / ID
    //=========================================================================
    enum State  mStatus  { State::Active };
    std::string mActorID { "Unnamed Actor" };
};

} // namespace toy
