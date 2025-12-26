#include "Asset/AssetManager.h"
#include "Asset/Material/Texture.h"
#include "Asset/Geometry/Mesh.h"
#include "Asset/Audio/SoundEffect.h"
#include "Asset/Audio/Music.h"
#include "Asset/Font/TextFont.h"
#include <iostream>

namespace toy {

AssetManager::AssetManager()
    : mAssetsPath("ToyGame/Assets") // デフォルトのアセット基準パス
    , mWindowDisplayScale(1.0f)
{
    
}

void AssetManager::UnloadData()
{
    // すべてのアセットを破棄（シーン切り替えなど）
    for (auto& [name, tex] : mTextures)
    {
        if (tex)
        {
            tex->Unload();   // ★ ここで glDelete
        }
    }
    mTextures.clear();
    for (auto& [name, mesh] : mMeshes)
    {
        if (mesh)
        {
            mesh->Unload();
        }
    }
    mMeshes.clear();
    for (auto& [name, se] : mSoundEffects)
    {
        if (se)
        {
            se->Unload();
        }
    }
    mSoundEffects.clear();
    for (auto& [name, music] : mMusics)
    {
        if (music)
        {
            music->Unload();
        }
    }
    mMusics.clear();
    for (auto& [name, font] : mTextFonts)
    {
        if (font)
        {
            font->Unload();
        }
    }
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

    // DPI スケールを反映してロード
    if (!font->Load(fullPath, pointSize * mWindowDisplayScale))
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
