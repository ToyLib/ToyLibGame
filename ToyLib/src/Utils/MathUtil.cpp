#include "Utils/MathUtil.h"

//======================================
// 定数ベクトル／行列／クォータニオン
//======================================

const Vector2 Vector2::Zero     (0.0f, 0.0f);
const Vector2 Vector2::UnitX    (1.0f, 0.0f);
const Vector2 Vector2::UnitY    (0.0f, 1.0f);
const Vector2 Vector2::NegUnitX(-1.0f, 0.0f);
const Vector2 Vector2::NegUnitY(0.0f, -1.0f);

const Vector3 Vector3::Zero       (0.0f, 0.0f, 0.0f);
const Vector3 Vector3::UnitX      (1.0f, 0.0f, 0.0f);
const Vector3 Vector3::UnitY      (0.0f, 1.0f, 0.0f);
const Vector3 Vector3::UnitZ      (0.0f, 0.0f, 1.0f);
const Vector3 Vector3::NegUnitX   (-1.0f, 0.0f, 0.0f);
const Vector3 Vector3::NegUnitY   (0.0f, -1.0f, 0.0f);
const Vector3 Vector3::NegUnitZ   (0.0f, 0.0f, -1.0f);
const Vector3 Vector3::Infinity   (Math::Infinity, Math::Infinity, Math::Infinity);
const Vector3 Vector3::NegInfinity(Math::NegInfinity, Math::NegInfinity, Math::NegInfinity);

static float m3Ident[3][3] =
{
    { 1.0f, 0.0f, 0.0f },
    { 0.0f, 1.0f, 0.0f },
    { 0.0f, 0.0f, 1.0f }
};
const Matrix3 Matrix3::Identity(m3Ident);

static float m4Ident[4][4] =
{
    { 1.0f, 0.0f, 0.0f, 0.0f },
    { 0.0f, 1.0f, 0.0f, 0.0f },
    { 0.0f, 0.0f, 1.0f, 0.0f },
    { 0.0f, 0.0f, 0.0f, 1.0f }
};
const Matrix4 Matrix4::Identity(m4Ident);

const Quaternion Quaternion::Identity(0.0f, 0.0f, 0.0f, 1.0f);

//======================================
// Vector2 実装
//======================================

Vector2 Vector2::Transform(const Vector2& vec, const Matrix3& mat, float w /*= 1.0f*/)
{
    Vector2 retVal;
    retVal.x = vec.x * mat.mat[0][0] + vec.y * mat.mat[1][0] + w * mat.mat[2][0];
    retVal.y = vec.x * mat.mat[0][1] + vec.y * mat.mat[1][1] + w * mat.mat[2][1];
    // w は返さない
    return retVal;
}

//======================================
// Vector3 実装
//======================================

Vector3 Vector3::Transform(const Vector3& vec, const Matrix4& mat, float w /*= 1.0f*/)
{
    Vector3 retVal;
    retVal.x = vec.x * mat.mat[0][0] + vec.y * mat.mat[1][0] +
               vec.z * mat.mat[2][0] + w * mat.mat[3][0];
    retVal.y = vec.x * mat.mat[0][1] + vec.y * mat.mat[1][1] +
               vec.z * mat.mat[2][1] + w * mat.mat[3][1];
    retVal.z = vec.x * mat.mat[0][2] + vec.y * mat.mat[1][2] +
               vec.z * mat.mat[2][2] + w * mat.mat[3][2];
    return retVal;
}

// 透視除算付き Transform
Vector3 Vector3::TransformWithPerspDiv(const Vector3& vec, const Matrix4& mat, float w /*= 1.0f*/)
{
    Vector3 retVal;
    retVal.x = vec.x * mat.mat[0][0] + vec.y * mat.mat[1][0] +
               vec.z * mat.mat[2][0] + w * mat.mat[3][0];
    retVal.y = vec.x * mat.mat[0][1] + vec.y * mat.mat[1][1] +
               vec.z * mat.mat[2][1] + w * mat.mat[3][1];
    retVal.z = vec.x * mat.mat[0][2] + vec.y * mat.mat[1][2] +
               vec.z * mat.mat[2][2] + w * mat.mat[3][2];

    float transformedW =
        vec.x * mat.mat[0][3] + vec.y * mat.mat[1][3] +
        vec.z * mat.mat[2][3] + w * mat.mat[3][3];

    if (!Math::NearZero(Math::Abs(transformedW)))
    {
        transformedW = 1.0f / transformedW;
        retVal *= transformedW;
    }
    return retVal;
}

// Quaternion で Vector3 を回転
Vector3 Vector3::Transform(const Vector3& v, const Quaternion& q)
{
    // v' = v + 2 * cross(q.xyz, cross(q.xyz, v) + q.w * v)
    Vector3 qv(q.x, q.y, q.z);
    Vector3 retVal = v;
    retVal += 2.0f * Vector3::Cross(qv, Vector3::Cross(qv, v) + q.w * v);
    return retVal;
}

//======================================
// Matrix4 実装
//======================================

void Matrix4::Invert()
{
    // 「Slow Math」系のフルアンロール版
    float tmp[12];
    float src[16];
    float dst[16];
    float det;

    // 転置して 1 次元配列へ
    // row 1 → col 1
    src[0]  = mat[0][0];
    src[4]  = mat[0][1];
    src[8]  = mat[0][2];
    src[12] = mat[0][3];

    // row 2 → col 2
    src[1]  = mat[1][0];
    src[5]  = mat[1][1];
    src[9]  = mat[1][2];
    src[13] = mat[1][3];

    // row 3 → col 3
    src[2]  = mat[2][0];
    src[6]  = mat[2][1];
    src[10] = mat[2][2];
    src[14] = mat[2][3];

    // row 4 → col 4
    src[3]  = mat[3][0];
    src[7]  = mat[3][1];
    src[11] = mat[3][2];
    src[15] = mat[3][3];

    // 余因子の一部
    tmp[0]  = src[10] * src[15];
    tmp[1]  = src[11] * src[14];
    tmp[2]  = src[9]  * src[15];
    tmp[3]  = src[11] * src[13];
    tmp[4]  = src[9]  * src[14];
    tmp[5]  = src[10] * src[13];
    tmp[6]  = src[8]  * src[15];
    tmp[7]  = src[11] * src[12];
    tmp[8]  = src[8]  * src[14];
    tmp[9]  = src[10] * src[12];
    tmp[10] = src[8]  * src[13];
    tmp[11] = src[9]  * src[12];

    dst[0] =  tmp[0]  * src[5] + tmp[3]  * src[6] + tmp[4]  * src[7]
            - tmp[1]  * src[5] - tmp[2]  * src[6] - tmp[5]  * src[7];
    dst[1] =  tmp[1]  * src[4] + tmp[6]  * src[6] + tmp[9]  * src[7]
            - tmp[0]  * src[4] - tmp[7]  * src[6] - tmp[8]  * src[7];
    dst[2] =  tmp[2]  * src[4] + tmp[7]  * src[5] + tmp[10] * src[7]
            - tmp[3]  * src[4] - tmp[6]  * src[5] - tmp[11] * src[7];
    dst[3] =  tmp[5]  * src[4] + tmp[8]  * src[5] + tmp[11] * src[6]
            - tmp[4]  * src[4] - tmp[9]  * src[5] - tmp[10] * src[6];

    dst[4] =  tmp[1]  * src[1] + tmp[2]  * src[2] + tmp[5]  * src[3]
            - tmp[0]  * src[1] - tmp[3]  * src[2] - tmp[4]  * src[3];
    dst[5] =  tmp[0]  * src[0] + tmp[7]  * src[2] + tmp[8]  * src[3]
            - tmp[1]  * src[0] - tmp[6]  * src[2] - tmp[9]  * src[3];
    dst[6] =  tmp[3]  * src[0] + tmp[6]  * src[1] + tmp[11] * src[3]
            - tmp[2]  * src[0] - tmp[7]  * src[1] - tmp[10] * src[3];
    dst[7] =  tmp[4]  * src[0] + tmp[9]  * src[1] + tmp[10] * src[2]
            - tmp[5]  * src[0] - tmp[8]  * src[1] - tmp[11] * src[2];

    tmp[0]  = src[2] * src[7];
    tmp[1]  = src[3] * src[6];
    tmp[2]  = src[1] * src[7];
    tmp[3]  = src[3] * src[5];
    tmp[4]  = src[1] * src[6];
    tmp[5]  = src[2] * src[5];
    tmp[6]  = src[0] * src[7];
    tmp[7]  = src[3] * src[4];
    tmp[8]  = src[0] * src[6];
    tmp[9]  = src[2] * src[4];
    tmp[10] = src[0] * src[5];
    tmp[11] = src[1] * src[4];

    dst[8]  =  tmp[0]  * src[13] + tmp[3]  * src[14] + tmp[4]  * src[15]
             - tmp[1]  * src[13] - tmp[2]  * src[14] - tmp[5]  * src[15];
    dst[9]  =  tmp[1]  * src[12] + tmp[6]  * src[14] + tmp[9]  * src[15]
             - tmp[0]  * src[12] - tmp[7]  * src[14] - tmp[8]  * src[15];
    dst[10] =  tmp[2]  * src[12] + tmp[7]  * src[13] + tmp[10] * src[15]
             - tmp[3]  * src[12] - tmp[6]  * src[13] - tmp[11] * src[15];
    dst[11] =  tmp[5]  * src[12] + tmp[8]  * src[13] + tmp[11] * src[14]
             - tmp[4]  * src[12] - tmp[9]  * src[13] - tmp[10] * src[14];

    dst[12] =  tmp[2]  * src[10] + tmp[5]  * src[11] + tmp[1]  * src[9]
             - tmp[4]  * src[11] - tmp[0]  * src[9]  - tmp[3]  * src[10];
    dst[13] =  tmp[8]  * src[11] + tmp[0]  * src[8]  + tmp[7]  * src[10]
             - tmp[6]  * src[10] - tmp[9]  * src[11] - tmp[1]  * src[8];
    dst[14] =  tmp[6]  * src[9]  + tmp[11] * src[11] + tmp[3]  * src[8]
             - tmp[10] * src[11] - tmp[2]  * src[8]  - tmp[7]  * src[9];
    dst[15] =  tmp[10] * src[10] + tmp[4]  * src[8]  + tmp[9]  * src[9]
             - tmp[8]  * src[9]  - tmp[11] * src[10] - tmp[5]  * src[8];

    // 行列式
    det = src[0] * dst[0] + src[1] * dst[1] + src[2] * dst[2] + src[3] * dst[3];

    // 逆行列 = 余因子行列 / det
    det = 1.0f / det;
    for (int j = 0; j < 16; ++j)
    {
        dst[j] *= det;
    }

    // 4x4 に戻す
    for (int i = 0; i < 4; ++i)
    {
        for (int j = 0; j < 4; ++j)
        {
            mat[i][j] = dst[i * 4 + j];
        }
    }
}

// Quaternion → 回転行列
Matrix4 Matrix4::CreateFromQuaternion(const Quaternion& q)
{
    float m[4][4];

    m[0][0] = 1.0f - 2.0f * q.y * q.y - 2.0f * q.z * q.z;
    m[0][1] = 2.0f * q.x * q.y + 2.0f * q.w * q.z;
    m[0][2] = 2.0f * q.x * q.z - 2.0f * q.w * q.y;
    m[0][3] = 0.0f;

    m[1][0] = 2.0f * q.x * q.y - 2.0f * q.w * q.z;
    m[1][1] = 1.0f - 2.0f * q.x * q.x - 2.0f * q.z * q.z;
    m[1][2] = 2.0f * q.y * q.z + 2.0f * q.w * q.x;
    m[1][3] = 0.0f;

    m[2][0] = 2.0f * q.x * q.z + 2.0f * q.w * q.y;
    m[2][1] = 2.0f * q.y * q.z - 2.0f * q.w * q.x;
    m[2][2] = 1.0f - 2.0f * q.x * q.x - 2.0f * q.y * q.y;
    m[2][3] = 0.0f;

    m[3][0] = 0.0f;
    m[3][1] = 0.0f;
    m[3][2] = 0.0f;
    m[3][3] = 1.0f;

    return Matrix4(m);
}

Quaternion Quaternion::CreateFromMatrix(const Matrix4& m)
{
    // スケール除去 + 正規化済みの基底を使う
    Vector3 xAxis = m.GetXAxis();
    Vector3 yAxis = m.GetYAxis();
    Vector3 zAxis = m.GetZAxis();

    // row-vector / row-major 前提
    const float m00 = xAxis.x;
    const float m01 = xAxis.y;
    const float m02 = xAxis.z;

    const float m10 = yAxis.x;
    const float m11 = yAxis.y;
    const float m12 = yAxis.z;

    const float m20 = zAxis.x;
    const float m21 = zAxis.y;
    const float m22 = zAxis.z;

    Quaternion q;
    const float trace = m00 + m11 + m22;

    if (trace > 0.0f)
    {
        const float s = Math::Sqrt(trace + 1.0f) * 2.0f;
        q.w = 0.25f * s;
        q.x = (m12 - m21) / s;
        q.y = (m20 - m02) / s;
        q.z = (m01 - m10) / s;
    }
    else if (m00 > m11 && m00 > m22)
    {
        const float s = Math::Sqrt(1.0f + m00 - m11 - m22) * 2.0f;
        q.w = (m12 - m21) / s;
        q.x = 0.25f * s;
        q.y = (m10 + m01) / s;
        q.z = (m20 + m02) / s;
    }
    else if (m11 > m22)
    {
        const float s = Math::Sqrt(1.0f + m11 - m00 - m22) * 2.0f;
        q.w = (m20 - m02) / s;
        q.x = (m10 + m01) / s;
        q.y = 0.25f * s;
        q.z = (m21 + m12) / s;
    }
    else
    {
        const float s = Math::Sqrt(1.0f + m22 - m00 - m11) * 2.0f;
        q.w = (m01 - m10) / s;
        q.x = (m20 + m02) / s;
        q.y = (m21 + m12) / s;
        q.z = 0.25f * s;
    }

    q.Normalize();
    return q;
}
