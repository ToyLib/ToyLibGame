#pragma once

#include "Utils/MathUtil.h"
#include <cstdint>

namespace toy {

//-------------------------------------------------------------
// Component
// ・Actor に付与される機能ブロックの基底クラス
// ・Update / Input / Transform のフックを提供
// ・SpriteComponent / MeshComponent / MoveComponent などが派生
//-------------------------------------------------------------
class Component
{
public:
    // コンストラクタ（update order が小さいほど先に更新される）
    Component(class Actor* a, int order = 100);

    virtual ~Component();
    
    //---------------------------------------------------------
    // 更新系（Actor から毎フレーム呼ばれる）
    //---------------------------------------------------------
    
    // 通常の毎フレーム更新（必要なら override）
    virtual void Update(float deltaTime);
    
    // 入力処理フック（必要なコンポーネントで override）
    virtual void ProcessInput(const struct InputState& state) {}
    
    // Actor のワールド行列更新後に呼ばれるフック
    // 座標系の依存がある Component（Mesh 等）が override
    virtual void OnUpdateWorldTransform() {}
    
    //---------------------------------------------------------
    // 情報取得
    //---------------------------------------------------------
    
    // 更新順序（小さいほど優先）
    int GetUpdateOrder() const { return mUpdateOrder; }
    
    // コンポーネントのワールド位置（必要に応じて override）
    virtual Vector3 GetPosition() const;
    
    // 所属する Actor
    class Actor* GetOwner() const { return mOwnerActor; }
    
private:
    // この Component を所有している Actor
    class Actor* mOwnerActor{};

protected:
    // 更新優先度（UpdateOrder）
    int mUpdateOrder{};
};

} // namespace toy
