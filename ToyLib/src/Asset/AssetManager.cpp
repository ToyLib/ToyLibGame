#include "Asset/AssetManager.h"
#include "Asset/Material/Texture.h"
#include "Asset/Geometry/Mesh.h"
#include "Asset/Audio/SoundEffect.h"
#include "Asset/Audio/Music.h"
#include "Asset/Font/TextFont.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <string>
#include <vector>

namespace toy {

AssetManager::AssetManager()
{
    
}
AssetManager::~AssetManager()
{
    UnloadData();
}   

void AssetManager::UnloadData()
{

    mTextures.clear();

    mMeshes.clear();

    mMusics.clear();
    mSoundEffects.clear();

    mTextFonts.clear();
}

//======================================================================
// テクスチャ取得
//  - キャッシュに同名のテクスチャがあればそれを返す
//  - なければロードして登録
//======================================================================
std::shared_ptr<Texture> AssetManager::GetTexture(const std::string& fileName)
{
    auto iter = mTextures.find(fileName);
    if (iter != mTextures.end())
    {
        return iter->second;      // 既存テクスチャを返す
    }
    
    auto tex = std::make_shared<Texture>();
    if (tex->Load(fileName, this))
    {
        mTextures[fileName] = tex;
        return tex;
    }
    
    return nullptr;               // ロード失敗
}

//======================================================================
// 埋め込みテクスチャ（FBX/GLTFの aiTexture 用）
//======================================================================
std::shared_ptr<Texture> AssetManager::GetEmbeddedTexture(
                                                          const std::string& nameKey,
                                                          const uint8_t* data,
                                                          size_t dataSize)
{
    auto iter = mTextures.find(nameKey);
    if (iter != mTextures.end())
    {
        return iter->second;
    }
    
    auto tex = std::make_shared<Texture>();
    if (tex->LoadFromMemory(data, static_cast<unsigned int>(dataSize)))
    {
        mTextures[nameKey] = tex;
        return tex;
    }
    
    return nullptr;
}

//=========================================================
// 白 1x1 テクスチャ生成
// 識別キー不要
//=========================================================
std::shared_ptr<class Texture> AssetManager::GetWhite1x1Texture()
{
    const std::string nameKey = "__sys_WHITE_1x1";
    auto iter = mTextures.find(nameKey);
    if (iter != mTextures.end())
    {
        return iter->second;
    }
    
    auto tex = std::make_shared<Texture>();
    uint8_t px[4] = {255, 255, 255, 255};
    if (tex->CreateFromPixels(px, 1, 1, true))
    {
        mTextures[nameKey] = tex;
        return tex;
    }
    std::cerr << "[Texture] White 1x1 Texture Cereate Error" << std::endl;
    return nullptr;
}

//=========================================================
// 角丸矩形テクスチャ生成
//=========================================================
namespace {

// 角丸矩形（原点中心、半サイズ half、角の半径 r）までの符号付き距離（内側が負）
float RoundedRectSDF(float px, float py, float half, float r)
{
    const float qx = std::fabs(px) - (half - r);
    const float qy = std::fabs(py) - (half - r);
    const float ox = std::max(qx, 0.0f);
    const float oy = std::max(qy, 0.0f);
    return std::sqrt(ox * ox + oy * oy) + std::min(std::max(qx, qy), 0.0f) - r;
}

// 距離 → 被覆率（1px 幅でアンチエイリアス）
float Coverage(float sdf)
{
    return std::clamp(0.5f - sdf, 0.0f, 1.0f);
}

} // namespace

std::shared_ptr<class Texture> AssetManager::GetRoundedRectTexture(int radius, int lineWidth)
{
    radius    = std::max(radius, 1);
    lineWidth = std::clamp(lineWidth, 0, radius);

    const std::string nameKey =
        "__sys_ROUNDRECT_r" + std::to_string(radius) + "_w" + std::to_string(lineWidth);
    auto iter = mTextures.find(nameKey);
    if (iter != mTextures.end())
    {
        return iter->second;
    }

    // 角 radius px ＋ 中央 4px（9-slice で伸ばす部分）
    const int   size = radius * 2 + 4;
    const float half = size * 0.5f;

    std::vector<uint8_t> px(static_cast<size_t>(size) * size * 4);
    for (int y = 0; y < size; ++y)
    {
        for (int x = 0; x < size; ++x)
        {
            // ピクセル中心で評価
            const float cx = (x + 0.5f) - half;
            const float cy = (y + 0.5f) - half;

            float a = Coverage(RoundedRectSDF(cx, cy, half, static_cast<float>(radius)));
            if (lineWidth > 0)
            {
                // 内側の角丸矩形をくり抜いて枠線にする
                const float inner = Coverage(RoundedRectSDF(cx, cy, half - lineWidth,
                                                            static_cast<float>(radius - lineWidth)));
                a = std::max(0.0f, a - inner);
            }

            uint8_t* p = &px[(static_cast<size_t>(y) * size + x) * 4];
            p[0] = 255;
            p[1] = 255;
            p[2] = 255;
            p[3] = static_cast<uint8_t>(a * 255.0f + 0.5f);
        }
    }

    auto tex = std::make_shared<Texture>();
    if (tex->CreateFromPixels(px.data(), size, size, true))
    {
        mTextures[nameKey] = tex;
        return tex;
    }
    std::cerr << "[Texture] RoundedRect Texture Create Error" << std::endl;
    return nullptr;
}

//======================================================================
// メッシュ取得
//======================================================================
std::shared_ptr<Mesh> AssetManager::GetMesh(const std::string& fileName,
                                            bool isRightHanded)
{
    auto iter = mMeshes.find(fileName);
    if (iter != mMeshes.end())
    {
        return iter->second;
    }

    auto mesh = std::make_shared<Mesh>();
    if (mesh->Load(fileName, this, isRightHanded))
    {
        mMeshes[fileName] = mesh;
        return mesh;
    }

    return nullptr;
}

//======================================================================
// 効果音（SoundEffect）取得
//======================================================================
std::shared_ptr<SoundEffect> AssetManager::GetSoundEffect(const std::string& fileName)
{
    auto iter = mSoundEffects.find(fileName);
    if (iter != mSoundEffects.end())
    {
        return iter->second;
    }

    auto se = std::make_shared<SoundEffect>();
    if (se->Load(fileName, this))
    {
        mSoundEffects[fileName] = se;
        return se;
    }

    return nullptr;
}

//======================================================================
// BGM（Music）取得
//======================================================================
std::shared_ptr<Music> AssetManager::GetMusic(const std::string& fileName)
{
    auto iter = mMusics.find(fileName);
    if (iter != mMusics.end())
    {
        return iter->second;
    }

    auto music = std::make_shared<Music>();
    if (music->Load(fileName, this))
    {
        mMusics[fileName] = music;
        return music;
    }

    return nullptr;
}

//======================================================================
// フォント取得
//  - サイズ違いも区別してキャッシュするため key にサイズを含む
//  - DPI スケールを反映
//======================================================================
std::shared_ptr<TextFont> AssetManager::GetFont(const std::string& fileName,
                                                int pointSize)
{
    // サイズも含めたキー
    const std::string key = fileName + "#" + std::to_string(pointSize);

    // キャッシュ済みなら返す
    auto iter = mTextFonts.find(key);
    if (iter != mTextFonts.end())
    {
        return iter->second;
    }

    // 新規ロード（font->Load はフルパスを想定）
    auto font = std::make_shared<TextFont>();

    const std::string fullPath = mAssetsPath + fileName;

    if (!font->Load(fullPath, pointSize))
    {
        std::cerr << "[AssetManager] Failed to load font: "
                  << fullPath << " (size: " << pointSize << ")"
                  << std::endl;
        return nullptr;
    }

    // 登録して返す
    mTextFonts.emplace(key, font);
    return font;
}

} // namespace toy
