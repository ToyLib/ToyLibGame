#pragma once

#include "Utils/MathUtil.h"

namespace toy::kit {

class Prefab;

//=============================================================================
// MovementUtil
//  Walk/Run（速度の違い）・Random/ToTarget（向かう先の決め方の違い）を、
//  IBehavior 側で共通に使える小さな関数に落とし込んだもの。
//  Prefab の Interface（GetPosition/SetPosition/SetRotation）だけを使い、
//  壁避け等は行わない（ChaseBehavior が元々持っていた簡略化と同じ）。
//=============================================================================

// XZ平面でランダムな単位方向ベクトルを1つ選ぶ（Random Walk/Run 用）
Vector3 PickRandomDirectionXZ();

// targetPos から見て body が離れていく方向（＝body から見て targetPos と反対側）の
// 単位方向ベクトル（XZ）。距離がほぼ0で方向が定まらない場合は Vector3::UnitZ を返す
// （Flee=ToTargetの逆、として使う）
Vector3 DirectionAwayFromPointXZ(const Prefab& body, const Vector3& targetPos);

// 指定方向（XZ）を向く。正規化は不要
void FaceDirectionXZ(Prefab& body, const Vector3& directionXZ);

// 指定方向へ speed * dt だけ前進する（Y は変更しない＝重力に任せる）
void MoveInDirectionXZ(Prefab& body, const Vector3& directionXZ, float speed, float dt);

// body と targetPos の XZ 距離
float GetDistanceXZ(const Prefab& body, const Vector3& targetPos);

// targetPos の方を向く（XZ平面）。距離がほぼ0なら何もしない（ToTarget 用）
void FaceTowardPointXZ(Prefab& body, const Vector3& targetPos);

// targetPos へ向かって進む。stopRange 以内なら何もしない（ToTarget 用）
void MoveTowardPointXZ(Prefab& body, const Vector3& targetPos, float speed, float dt, float stopRange = 0.0f);

} // namespace toy::kit
