#pragma once

#include "Camera/CameraComponent.h"
#include "Utils/MathUtil.h"

namespace toy {

//======================================================================
// OrbitCameraComponent
//
//  ・ターゲット Actor 周囲を“公転”する 3rd Person カメラ
//  ・左手座標系（+Z が奥）前提
//  ・フィールドアクション / 見下ろし寄りのゲームに向いた汎用カメラ
//
//  特徴：
//    - 左右入力で水平回転（ヨー）
//    - 上下入力で高さ変更（結果として距離も自動で変化）
//    - （将来）ホイール等でズーム
//    - FollowCamera などから切り替え時、前カメラ位置から
//      スムーズに理想軌道へ寄せていく想定
//======================================================================
class OrbitCameraComponent : public CameraComponent
{
public:
    explicit OrbitCameraComponent(class Actor* owner);
    
    //------------------------------------------------------------------
    // 入力処理
    //
    //  ・左右入力：公転（ヨー回転）の入力値を設定
    //  ・上下入力：高さ変更の入力値を設定
    //  ・実際の適用は UpdateCamera() 側で行う
    //------------------------------------------------------------------
    void ProcessInput(const struct InputState& state) override;
    
    //------------------------------------------------------------------
    // 毎フレーム更新
    //
    //  ・ヨー回転 / 高さ変更 / 距離（ズーム）を更新
    //  ・理想オフセット mOffset から理想位置を計算
    //  ・mCurrentPos → 理想位置 へ補間してカメラ位置とする
    //  ・最後に View 行列を Renderer に適用
    //------------------------------------------------------------------
    void UpdateCamera(float deltaTime) override;
    
    //------------------------------------------------------------------
    // 設定用（ヨー回転速度）
    //------------------------------------------------------------------
    float GetYawSpeed() const          { return mYawSpeed; }
    void  SetYawSpeed(float speed)     { mYawSpeed = speed; }
    
    //------------------------------------------------------------------
    // OnActivated
    //
    //  ・他のカメラから Orbit カメラに切り替わった瞬間に呼ばれる
    //  ・prevPos / prevTarget には「直前のカメラ」の情報が渡される
    //  ・典型的な実装では：
    //      - mCurrentPos を prevPos からスタートさせて
    //      - そこから orbit の理想オフセットへスムーズに遷移させる
    //------------------------------------------------------------------
    void OnActivated(const Vector3& prevPos,
                     const Vector3& prevTarget) override;

    
private:
    //==============================
    // カメラの基礎プロパティ
    //==============================
    
    // ターゲット位置からのオフセット
    //
    //  ・最終的なカメラ位置は
    //      cameraPos = target + mOffset
    //  ・Y 成分が高さ、XZ 成分が水平距離・方向を表す
    Vector3 mOffset{0.0f, 4.0f, -5.0f};
    
    // Up ベクトル（基本は World の +Y 軸）
    Vector3 mUpVector{Vector3::UnitY};

    //==============================
    // 公転（水平回転）
    //==============================
    
    // ヨー角速度（ラジアン/秒）
    //   + … 左回り（反時計回り）
    //   - … 右回り
    float mYawSpeed{};

    //==============================
    // ズーム（距離）
    //==============================

    // 現在の実距離
    float mDistance{};

    // スムーズに追従させるための「目標距離」
    float mTargetDistance{};

    // 距離の下限 / 上限
    float mMinDistance{5.0f};
    float mMaxDistance{20.0f};

    //==============================
    // 高さ（オフセット Y）
    //==============================
    
    // カメラの最低 / 最高 Y 位置（target から見た相対高さ）
    float mMinOffsetY{-2.0f};
    float mMaxOffsetY{8.0f};

    //==============================
    // 入力蓄積（ProcessInput → UpdateCamera）
    //==============================

    // 高さ操作（-1 ～ +1）
    //   ・+1 = 上へ
    //   ・-1 = 下へ
    float mHeightInput{};
    
    //==============================
    // 位置補間（スムーズなカメラ移動用）
    //==============================

    // 実際のカメラ位置（理想位置へ補間していく）
    Vector3 mCurrentPos{};

    // 最初の補間フレームかどうか
    //  （必要に応じて「初回はスナップ」などに使用）
    bool    mFirstInterp{true};

    // 位置補間の速さ（大きいほど素早く理想位置に寄る）
    float   mPosLerpSpeed{8.0f};

    // mCurrentPos が初期化済みかどうか
    //  （OnActivated などで初期位置をセット済みかの判定に使用）
    bool    mHasCurrentPos{false};
};

} // namespace toy
