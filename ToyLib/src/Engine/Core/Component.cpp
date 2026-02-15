#include "Engine/Core/Component.h"
#include "Engine/Core/Actor.h"
#include <iostream>

namespace toy {

//-------------------------------------------------------------
// Component
// ・Actor に紐づく機能ブロックの基底クラス
// ・デフォルト実装では何もせず、派生クラスで用途ごとに拡張する
//-------------------------------------------------------------

// コンストラクタ
//  owner: 所有する Actor
//  order: Update の優先度（小さいほど先に Update される）
Component::Component(Actor* a, int order)
    : mOwnerActor(a)
    , mUpdateOrder(order)
{
}

// デストラクタ（基底では特別な処理なし）
Component::~Component()
{
    
}

// 毎フレーム更新（基底では何もしない）
// ・派生クラス側で必要な処理を実装する
void Component::Update(float deltaTime)
{
    (void)deltaTime;
}

// この Component の位置を返す
// ・デフォルトでは Owner Actor の位置をそのまま返す
// ・ローカルオフセットを持ちたい場合は派生クラスで override する
Vector3 Component::GetPosition() const
{
    return mOwnerActor->GetPosition();
}

} // namespace toy
