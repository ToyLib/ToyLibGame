#include "KitPrefab/MovementUtil.h"
#include "KitPrefab/Prefab.h"

#include <cmath>
#include <cstdlib>

namespace toy::kit {

//-----------------------------------------------------------------------------
Vector3 PickRandomDirectionXZ()
{
    const float angle = (static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX))
                       * 2.0f * 3.14159265f;
    return Vector3(sinf(angle), 0.0f, cosf(angle));
}

//-----------------------------------------------------------------------------
Vector3 DirectionAwayFromPointXZ(const Prefab& body, const Vector3& targetPos)
{
    const Vector3& self = body.GetPosition();
    const float dx = self.x - targetPos.x;
    const float dz = self.z - targetPos.z;
    const float lenSq = dx * dx + dz * dz;
    if (lenSq < 1.0e-6f) return Vector3::UnitZ; // 距離ゼロで方向が定まらない場合の既定値

    const float invLen = 1.0f / sqrtf(lenSq);
    return Vector3(dx * invLen, 0.0f, dz * invLen);
}

//-----------------------------------------------------------------------------
void FaceDirectionXZ(Prefab& body, const Vector3& directionXZ)
{
    const float angle = atan2f(directionXZ.x, directionXZ.z);
    body.SetRotation(Quaternion(Vector3::UnitY, angle));
}

//-----------------------------------------------------------------------------
void MoveInDirectionXZ(Prefab& body, const Vector3& directionXZ, float speed, float dt)
{
    const Vector3& pos = body.GetPosition();
    body.SetPosition(pos + directionXZ * speed * dt);
}

//-----------------------------------------------------------------------------
float GetDistanceXZ(const Prefab& body, const Vector3& targetPos)
{
    const Vector3& self = body.GetPosition();
    const float dx = targetPos.x - self.x;
    const float dz = targetPos.z - self.z;
    return sqrtf(dx * dx + dz * dz);
}

//-----------------------------------------------------------------------------
void FaceTowardPointXZ(Prefab& body, const Vector3& targetPos)
{
    const Vector3& self = body.GetPosition();
    const float dx = targetPos.x - self.x;
    const float dz = targetPos.z - self.z;
    if (dx * dx + dz * dz < 0.01f) return;

    const float angle = atan2f(-dx, -dz);
    body.SetRotation(Quaternion(Vector3::UnitY, angle));
}

//-----------------------------------------------------------------------------
void MoveTowardPointXZ(Prefab& body, const Vector3& targetPos, float speed, float dt, float stopRange)
{
    const Vector3& self = body.GetPosition();
    const float dx   = targetPos.x - self.x;
    const float dz   = targetPos.z - self.z;
    const float dist = sqrtf(dx * dx + dz * dz);

    if (dist < 1.0e-4f) return; // ゼロ割回避
    if (dist < stopRange) return;

    const float step = speed * dt / dist;
    body.SetPosition(Vector3(self.x + dx * step, self.y, self.z + dz * step));
}

} // namespace toy::kit
