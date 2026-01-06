#include "Camera/OrbitCameraComponent.h"
#include "Engine/Core/Actor.h"
#include "Engine/Runtime/InputSystem.h"
#include "Engine/Core/Application.h"
#include "Physics/PhysWorld.h"
#include <cmath>
#include <cfloat>

namespace toy {

OrbitCameraComponent::OrbitCameraComponent(Actor* owner)
    : CameraComponent(owner)
    , mOffset(0.0f, 4.0f, -5.0f)   // 初期オフセット（やや上＋後ろ）
    , mUpVector(Vector3::UnitY)
    , mYawSpeed(0.0f)
    , mDistance(0.0f)
    , mTargetDistance(0.0f)
    , mMinDistance(5.0f)
    , mMaxDistance(20.0f)
    , mMinOffsetY(-2.0f)
    , mMaxOffsetY(8.0f)
    , mHeightInput(0.0f)
{
    // 初期距離をオフセットから算出し、許容範囲にクランプ
    mDistance = mOffset.Length();
    if (mDistance < mMinDistance)
    {
        mDistance = mMinDistance;
    }
    if (mDistance > mMaxDistance)
    {
        mDistance = mMaxDistance;
    }
    mTargetDistance = mDistance;
    
    // Y オフセットもクランプ
    if (mOffset.y < mMinOffsetY)
    {
        mOffset.y = mMinOffsetY;
    }
    if (mOffset.y > mMaxOffsetY)
    {
        mOffset.y = mMaxOffsetY;
    }
}

void OrbitCameraComponent::ProcessInput(const InputState& state)
{
    // 入力値 → 「1フレーム分のヨー角速度 / 高さ操作」に変換するだけ
    // 実際の適用は Update 側で行う
    
    const float yawSpeedBase = Math::ToRadians(120.0f); // 最大左右回転速度
    
    float yawInput    = 0.0f;
    float heightInput = 0.0f;   // 上を +1 とする
    
    // 将来の右スティック対応（今はコメントアウト）
    // const Vector2 rs = state.Controller.GetRightStick();
    // yawInput    += rs.x;
    // heightInput += -rs.y;   // 上を + にしたいので反転
    
    // キーボード入力
    if (state.IsButtonDown(GameButton::KeyD))
    {
        yawInput += 1.0f;
    }
    if (state.IsButtonDown(GameButton::KeyA))
    {
        yawInput -= 1.0f;
    }
    if (state.IsButtonDown(GameButton::KeyS))
    {
        heightInput += 1.0f;   // 上方向
    }
    if (state.IsButtonDown(GameButton::KeyW))
    {
        heightInput -= 1.0f;   // 下方向
    }
    
    // 実角速度へ変換（rad/s）
    mYawSpeed    = yawInput * yawSpeedBase;
    mHeightInput = heightInput;
}

void OrbitCameraComponent::UpdateCamera(float deltaTime)
{
    
    //--------------------------------
    // 1. ヨー回転（水平公転）
    //--------------------------------
    {
        Quaternion yawRot(Vector3::UnitY, mYawSpeed * deltaTime);
        mOffset   = Vector3::Transform(mOffset,   yawRot);
        mUpVector = Vector3::Transform(mUpVector, yawRot);
    }
    
    //--------------------------------
    // 2. 高さ更新（Yオフセットのみ操作）
    //--------------------------------
    const float heightSpeed = 7.0f;
    
    if (std::fabs(mHeightInput) > 1e-4f)
    {
        // 入力に応じてオフセットYを移動
        mOffset.y += mHeightInput * heightSpeed * deltaTime;
        
        // 高さクランプ
        if (mOffset.y < mMinOffsetY)
        {
            mOffset.y = mMinOffsetY;
        }
        if (mOffset.y > mMaxOffsetY)
        {
            mOffset.y = mMaxOffsetY;
        }
    }
    
    // 次フレーム用に入力は消費済みにしておく
    mHeightInput = 0.0f;
    
    //--------------------------------
    // 3. 高さ → 距離マッピング
    //    ・低いほど近い
    //    ・高いほど遠い
    //--------------------------------
    float t = (mOffset.y - mMinOffsetY) / (mMaxOffsetY - mMinOffsetY);
    if (t < 0.0f)
    {
        t = 0.0f;
    }
    if (t > 1.0f)
    {
        t = 1.0f;
    }
    
    const float nearDist = mMinDistance;
    const float farDist  = mMaxDistance;
    mTargetDistance = nearDist + (farDist - nearDist) * t;
    
    //--------------------------------
    // 4. 距離をターゲットに補間
    //--------------------------------
    const float zoomLerpSpeed = 10.0f; // 追従の速さ（大きいほどキビキビ）
    mDistance += (mTargetDistance - mDistance) * zoomLerpSpeed * deltaTime;
    
    if (mDistance < mMinDistance)
    {
        mDistance = mMinDistance;
    }
    if (mDistance > mMaxDistance)
    {
        mDistance = mMaxDistance;
    }
    
    // オフセット方向は維持しつつ、距離だけ反映
    Vector3 dir = mOffset;
    dir.Normalize();
    mOffset = dir * mDistance;
    
    //--------------------------------
    // 5. カメラ位置の算出
    //--------------------------------
    // ターゲットの少し上を狙う（主人公の頭上あたり）
    Vector3 target    = GetOwner()->GetPosition() + Vector3(0.0f, 2.5f, 0.0f);
    Vector3 cameraPos = target + mOffset;
    
    //--------------------------------
    // 6. 地面との当たり補正
    //--------------------------------
    if (Application* app = GetOwner()->GetApp())
    {
        if (PhysWorld* phys = app->GetPhysWorld())
        {
            // 仮のカメラ Actor にも位置を入れておく（他のシステムで使う場合用）
            mCameraActor->SetPosition(cameraPos);
            
            float groundY = phys->GetGroundHeightAt(mCameraActor->GetPosition());
            if (groundY != -FLT_MAX)
            {
                const float margin = 0.1f;      // めり込み防止マージン
                float minY = groundY + margin;
                
                if (cameraPos.y < minY)
                {
                    cameraPos.y = minY;
                }
            }
        }
    }
    
    //--------------------------------
    // 7. オフセット・距離の再同期
    //--------------------------------
    {
        // 補正後のカメラ位置から改めてオフセットと距離を更新
        Vector3 toCam = cameraPos - target;
        mOffset = toCam;
        
        mDistance = mOffset.Length();
        if (mDistance < mMinDistance)
        {
            mDistance = mMinDistance;
        }
        
        if (mDistance > mMaxDistance)
        {
            mDistance = mMaxDistance;
        }
        
        // 高さも再クランプ
        if (mOffset.y < mMinOffsetY)
        {
            mOffset.y = mMinOffsetY;
        }
        if (mOffset.y > mMaxOffsetY)
        {
            mOffset.y = mMaxOffsetY;
        }
        
        // 最終的なオフセットを距離付きで再構築
        Vector3 n = mOffset;
        n.Normalize();
        mOffset   = n * mDistance;
        cameraPos = target + mOffset;
    }
    
    //--------------------------------
    // 8. ビュー行列反映
    //--------------------------------
    mCameraPosition = cameraPos;
    
    Matrix4 view = Matrix4::CreateLookAt(
        cameraPos,
        target,
        mUpVector
    );
    SetViewMatrix(view);
    
    // カメラ Actor の位置も更新しておく
    mCameraActor->SetPosition(cameraPos);
}

} // namespace toy
