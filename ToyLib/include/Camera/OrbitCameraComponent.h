#pragma once

#include "Camera/CameraComponent.h"
#include "Utils/MathUtil.h"

namespace toy {

//--------------------------------------
// OrbitCameraComponent
// -------------------------------------
// ・ターゲット Actor の周囲を“公転”するカメラ
// ・基準は左手座標系（+Z が奥方向）
// ・俯瞰ゲーム/フィールドアクション用カメラに最適
//--------------------------------------
class OrbitCameraComponent : public CameraComponent
{
public:
    OrbitCameraComponent(class Actor* owner);
    
    // 入力処理（左右＝公転、上下＝高さ、ホイール＝ズーム）
    void ProcessInput(const struct InputState& state) override;
    
    // 毎フレーム更新（位置更新 & View 行列適用）
    void UpdateCamera(float deltaTime) override;
    
    // 設定用
    float GetYawSpeed() const                { return mYawSpeed; }
    void  SetYawSpeed(float speed)           { mYawSpeed = speed; }
    
    
private:
    //==============================
    // カメラの基礎プロパティ
    //==============================
    
    // ターゲット中心からのオフセット
    //   （Y は高さ、XZ 平面は正規化して距離と回転で決定）
    Vector3 mOffset;
    
    // 常に固定の Up ベクトル（通常は Y 軸）
    Vector3 mUpVector;

    //==============================
    // 公転（水平回転）
    //==============================
    
    // ヨー角速度（ラジアン/秒）
    //   +…左回り（反時計回り）
    //   -…右回り
    float mYawSpeed;

    //==============================
    // ズーム（距離）
    //==============================

    // 実際の距離
    float mDistance;

    // スムーズズーム用：目標距離
    float mTargetDistance;

    // ズーム下限/上限
    float mMinDistance;
    float mMaxDistance;

    //==============================
    // 高さ（オフセット Y）
    //==============================
    
    float mMinOffsetY;      // カメラの最低位置（Y）
    float mMaxOffsetY;      // カメラの最高位置（Y）

    //==============================
    // 入力蓄積（ProcessInput → Update）
    //==============================

    // 高さ操作（-1 ～ +1）
    //   ・+1 = 上へ
    //   ・-1 = 下へ
    float mHeightInput;
};

} // namespace toy
