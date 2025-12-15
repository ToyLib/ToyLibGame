#pragma once

#include "Utils/MathUtil.h"
#include <vector>
#include <string>
#include <memory>
#include <algorithm>

namespace toy {

//-------------------------------------------------------------
// Actor
// ・ToyLib の基本単位となるエンティティ
// ・Component を保持し、Update/Transform/Input を制御する
// ・親子関係に対応し、ローカル座標／ワールド座標を管理する
//-------------------------------------------------------------
class Actor
{
public:
    //---------------------------------------------------------
    // Actor の状態
    //---------------------------------------------------------
    enum State
    {
        EActive,   // 通常動作
        EPaused,   // 更新停止
        EDead      // 削除予約
    };
    
    Actor(class Application* a);
    virtual ~Actor();
    
    //=========================================================
    // 親子関係（Transform 階層）
    //=========================================================
    
    // 親を設定（※ keepWorld=true ならワールド位置を維持する機能に対応予定）
    void SetParent(Actor* newParent);
    
    Actor* GetParent() const { return mParent; }
    const std::vector<Actor*>& GetChildren() const { return mChildren; }
    
    // ワールド行列を再計算する必要があることを通知し、子へ伝播
    void MarkWorldDirty();
    
    //=========================================================
    // 更新処理（Actor が毎フレーム行うこと）
    //=========================================================
    
    // フレーム更新（内部 → コンポーネント → 派生 Actor の順）
    void Update(float deltaTime);
    
    // Component 更新
    void UpdateComponents(float deltaTime);
    
    // 派生 Actor が override する更新処理
    virtual void UpdateActor(float deltaTime) {}
    
    //=========================================================
    // 入力処理
    //=========================================================
    
    // 入力を全 Component に伝える
    void ProcessInput(const struct InputState& state);
    
    // Actor 固有の入力処理
    virtual void ActorInput(const struct InputState& state);
    
    //=========================================================
    // トランスフォーム（ローカル → ワールド）
    //=========================================================
    
    // ※ mPosition は「親がいればローカル座標」「親がなければワールド」
    Vector3 GetPosition() const { return mPosition; }
    void SetPosition(const Vector3& pos);   // 実装は .cpp
    
    float GetScale() const { return mScale; }
    void SetScale(float sc) { mScale = sc; MarkWorldDirty(); }
    
    const Quaternion& GetRotation() const { return mRotation; }
    void SetRotation(const Quaternion& rot) { mRotation = rot; MarkWorldDirty(); }
    
    // 親のワールドを考慮してワールド行列を作成
    void ComputeWorldTransform();
    
    // ワールド行列取得
    const Matrix4& GetWorldTransform() const { return mWorldTransform; }
    void SetWorldTransform(const Matrix4& mat) { mWorldTransform = mat; }
    // 向きベクトル（ローカル回転から算出）
    virtual Vector3 GetForward() { return Vector3::Transform(Vector3::UnitZ, mRotation); }
    virtual Vector3 GetRight()   { return Vector3::Transform(Vector3::UnitX, mRotation); }
    virtual Vector3 GetUpward()  { return Vector3::Transform(Vector3::UnitY, mRotation); }
    
    // Forward を直接セットする（内部で回転を調整）
    void SetForward(const Vector3& dir);
    
    
    //=============================================================
    // Pose（描画用の姿勢：Pitch/Rollなど。物理には基本適用しない）
    //=============================================================
    void SetPoseRotation(const Quaternion& q) { mPoseRotation = q; };
    const Quaternion& GetPoseRotation() const { return mPoseRotation; }
    // ポーズ合成済みワールド行列取得
    const Matrix4& GetRenderWorldTransform() const { return mRenderWorldTransform; }
    void SetInheritParentPose(const bool b) { mInheritParentPose = b; }
    bool GetInheritParentPose() const { return mInheritParentPose; }
    
    //=========================================================
    // State / Application
    //=========================================================
    
    State GetState() const { return mStatus; }
    void SetState(State state) { mStatus = state; }
    
    class Application* GetApp() { return mApp; }
    
    //=========================================================
    // Component 管理
    //=========================================================
    
    void AddComponent(std::unique_ptr<class Component> component);
    void RemoveComponent(class Component* component);
    
    // Component 生成（CreateActor と同様に Owner = this）
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
    
    // Actor 識別 ID
    void SetActorID(const std::string actorID) { mActorID = actorID; }
    std::string GetActorID() const { return mActorID; }
    
    
private:
    //---------------------------------------------------------
    // トランスフォーム（ローカル）
    //---------------------------------------------------------
    
    Matrix4     mWorldTransform;         // ワールド行列
    Vector3     mPosition;               // 親あり：ローカル位置／親なし：ワールド位置
    Quaternion  mRotation;
    float       mScale;
    bool        mIsRecomputeWorldTransform;
    
    //---------------------------------------------------------
    // Pose
    //---------------------------------------------------------
    Quaternion  mPoseRotation;
    Matrix4     mRenderWorldTransform;
    //---------------------------------------------------------
    // 親のポーズの影響を受けるか
    // true  : 親の RenderTransform（Pose込み） を使う
    // false : 親の WorldTransform（Poseなし） を使う
    //---------------------------------------------------------
    bool mInheritParentPose;
    
    //---------------------------------------------------------
    // 親子関係
    //---------------------------------------------------------
    Actor* mParent = nullptr;
    std::vector<Actor*> mChildren;
    
    //---------------------------------------------------------
    // Component / Application
    //---------------------------------------------------------
    std::vector<std::unique_ptr<class Component>> mComponents;
    class Application* mApp;
    
    //---------------------------------------------------------
    // Actor 状態 / 識別子
    //---------------------------------------------------------
    enum State mStatus;
    std::string mActorID;
};

} // namespace toy
