#include "Environment/SkyDomeMeshGenerator.h"
#include "Asset/Geometry/VertexArray.h"
#include "Utils/MathUtil.h"
#include <vector>
#include <cmath>

namespace toy {
namespace SkyDomeMeshGenerator {

/**
 * @brief スカイドーム用の半球メッシュ（実際はフルスフィア）を生成する。
 *
 * - 単純な UV 球体メッシュを生成する。
 * - VertexArray は Position / Normal / TexCoord / Indices を持つ。
 * - WeatherDomeComponent / SkyDomeComponent が色・天気の描画を行うため、
 *   本処理は純粋にジオメトリ生成に特化している。
 *
 * @param slices 経度方向の分割数（ぐるっと横方向）
 * @param stacks 緯度方向の分割数（縦方向）
 * @param radius 半径
 */
std::unique_ptr<VertexArray> CreateSkyDomeVAO(int slices, int stacks, float radius)
{
    // ------------------------------
    // 頂点バッファ（位置・法線・UV）
    // ------------------------------
    std::vector<float> positions;   // x, y, z
    std::vector<float> normals;     // nx, ny, nz
    std::vector<float> texCoords;   // u, v
    std::vector<unsigned int> indices;

    // ------------------------------
    // Sphere 頂点生成
    // stacks（縦） × slices（横）
    //
    // v: 0〜1 → φ: 0〜π（Math::Pi）を掃く
    // u: 0〜1 → θ: 0〜2π（Math::TwoPi）を掃く
    // ------------------------------
    for (int y = 0; y <= stacks; ++y)
    {
        float v   = static_cast<float>(y) / stacks;
        float phi = v * Math::Pi;  // 0〜π のフルスフィア

        for (int x = 0; x <= slices; ++x)
        {
            float u     = static_cast<float>(x) / slices;
            float theta = u * Math::TwoPi; // 0〜2π

            // 球面座標 → XYZ
            float xPos = radius * sinf(phi) * cosf(theta);
            float yPos = radius * cosf(phi);
            float zPos = radius * sinf(phi) * sinf(theta);

            // 位置
            positions.push_back(xPos);
            positions.push_back(yPos);
            positions.push_back(zPos);

            // 法線（スカイドームは内向きではなく外向きだが、描画側で Cull しないので問題なし）
            Vector3 n = Vector3(xPos, yPos, zPos);
            n.Normalize();
            normals.push_back(n.x);
            normals.push_back(n.y);
            normals.push_back(n.z);

            // UV
            // u:0〜1
            // v:0〜1（上下反転が必要な場合はここで調整）
            texCoords.push_back(u);
            texCoords.push_back(1.0f - v);
        }
    }

    // ------------------------------
    // インデックス生成（2 三角形で 1 quad）
    //
    // (i0) -- (i1)
    //   |   x   |
    // (i2) -- (i3)
    // ------------------------------
    for (int y = 0; y < stacks; ++y)
    {
        for (int x = 0; x < slices; ++x)
        {
            int i0 = y * (slices + 1) + x;
            int i1 = i0 + 1;
            int i2 = i0 + (slices + 1);
            int i3 = i2 + 1;

            // 1枚目の三角形
            indices.push_back(i0);
            indices.push_back(i2);
            indices.push_back(i1);

            // 2枚目の三角形
            indices.push_back(i1);
            indices.push_back(i2);
            indices.push_back(i3);
        }
    }

    // ------------------------------
    // VertexArray を生成して返す
    // ------------------------------
    return std::make_unique<VertexArray>(
        static_cast<unsigned int>(positions.size() / 3),
        positions.data(),
        normals.data(),
        texCoords.data(),
        static_cast<unsigned int>(indices.size()),
        indices.data()
    );
}

} // namespace SkyDomeMeshGenerator
} // namespace toy
